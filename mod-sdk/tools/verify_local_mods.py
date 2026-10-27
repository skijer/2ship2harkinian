"""Check installed MM packages and their imports against the built game module."""
import argparse
import hashlib
import json
import sys
import zipfile
from pathlib import Path

import lief

# Mods import from the game module; it forwards libultraship's symbols to libultraship.dll.
HOST_MODULE = '2ship.dll'
MODAPI_MAJOR = 3


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mods', type=Path, required=True)
    parser.add_argument('--packages', type=Path, required=True)
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    host_path = args.game / HOST_MODULE
    host = lief.PE.parse(str(host_path))
    if host is None or not host.has_exports:
        raise SystemExit('The host is missing or has no exports.')
    exports = {entry.name for entry in host.get_export().entries}
    results = []
    for manifest_path in sorted(args.mods.glob('*/manifest.json')):
        name = manifest_path.parent.name
        package = args.packages / (name + '.o2r')
        installed = args.game / 'mods' / package.name
        problems = []
        if not package.is_file() or not installed.is_file():
            problems.append('package missing from build or installation')
        else:
            if digest(package) != digest(installed):
                problems.append('installed package differs from the build')
            with zipfile.ZipFile(package) as archive:
                if archive.testzip() is not None:
                    problems.append('archive integrity failure')
                manifest = json.loads(archive.read('manifest.json'))
                if manifest.get('game') != 'mm' or manifest.get('modapi') != MODAPI_MAJOR:
                    problems.append(f'expected game=mm, modapi={MODAPI_MAJOR}')
                binary_path = manifest.get('binaries', {}).get('windows_x64')
                binary = lief.PE.parse(list(archive.read(binary_path))) if binary_path else None
                if binary is None:
                    problems.append('Windows binary missing or invalid')
                else:
                    entry_points = {entry.name for entry in binary.get_export().entries} if binary.has_exports else set()
                    for entry in ('ModSetApi', 'ModGetRequirements', 'ModInit'):
                        if entry not in entry_points:
                            problems.append('missing entry point: ' + entry)
                    for library in binary.imports:
                        if library.name.lower() == HOST_MODULE:
                            for entry in library.entries:
                                if entry.is_ordinal or entry.name not in exports:
                                    problems.append('unresolved host import: ' + (entry.name or str(entry.ordinal)))
        results.append({'mod': name, 'problems': problems})
    report = {'host': str(host_path), 'host_sha256': digest(host_path),
              'checked': len(results), 'failed': sum(bool(row['problems']) for row in results), 'mods': results}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f"Checked {report['checked']} installed packages; {report['failed']} failed.")
    for row in results:
        for problem in row['problems']:
            print(row['mod'] + ': ' + problem)
    return 1 if report['failed'] else 0


if __name__ == '__main__':
    sys.exit(main())
