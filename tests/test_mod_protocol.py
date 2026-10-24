import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "mod-sdk" / "tools"
sys.path.insert(0, str(TOOLS))
import build_catalog
import mod_signature

GAME = "mm"

class ModProtocolTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)

    def test_network_and_archive_permission_scanning(self):
        import scan_mod_sources
        mod = self.directory / "mods" / "sample"
        mod.mkdir(parents=True)
        (mod / "manifest.json").write_text(json.dumps({"permissions": ["network", "files"]}))
        self.assertEqual(scan_mod_sources.check_manifests(mod.parent), [])
        source = mod / "sample.cpp"
        tokens = '# 1 "' + source.as_posix() + '"\napi->RegisterNetChannel(); api->MountUserArchive();\n'
        problems, used = scan_mod_sources.scan_tokens(tokens, self.directory, mod.parent)
        self.assertEqual(problems, set())
        self.assertEqual(used["sample"], {"network", "files"})

    def package(self, game=GAME, major=1):
        mod = self.directory / "sample"
        mod.mkdir(exist_ok=True)
        (mod / "manifest.json").write_text(json.dumps({"name": "Sample", "permissions": []}))
        binary = self.directory / "sample.dll"
        binary.write_bytes(b"packaging fixture")
        package = self.directory / "sample.o2r"
        subprocess.run([sys.executable, str(TOOLS / "pack_mod.py"), "--mod-directory", str(mod),
                        "--binary", str(binary), "--platform", "windows_x64", "--output", str(package),
                        "--game", game, "--modapi-major", str(major)], check=True, capture_output=True)
        return package

    def test_package_identity(self):
        with zipfile.ZipFile(self.package()) as archive:
            manifest = json.loads(archive.read("manifest.json"))
            self.assertEqual((manifest["format"], manifest["game"], manifest["modapi"]), (2, GAME, 1))
            self.assertEqual(archive.read(manifest["binaries"]["windows_x64"]), b"packaging fixture")

    def test_reject_other_game_and_major_in_publication(self):
        for game, major in [("mm" if GAME == "oot" else "oot", 1), (GAME, 2)]:
            with self.subTest(game=game, major=major):
                result = build_catalog.plan(build_catalog.empty_catalog(), [self.package(game, major)],
                                            {}, 10, True, False, set(), 1, GAME)
                self.assertIn("sample", result["rejected"])
                self.assertFalse(result["publish"])

    def test_package_rejects_nonpositive_major(self):
        with self.assertRaises(subprocess.CalledProcessError):
            self.package(major=0)

    def test_catalog_preserves_other_game_build(self):
        package = self.package()
        catalog = build_catalog.empty_catalog()
        other_game = "mm" if GAME == "oot" else "oot"
        catalog["mods"]["sample"] = {"issue": 10, "version": "old-version", "builds": {f"{other_game}:1": {"url": "other"}}}
        plan = build_catalog.plan(catalog, [package], {}, 10, True, False, set(), 1, GAME)
        request = {"issue": 10, "author": "test", "repository": "test/mods", "commit": "0" * 40,
                   "directory": "mods"}
        build_catalog.apply(catalog, plan, request, self.directory, {}, [], [], "https://example.com")
        self.assertEqual(catalog["mods"]["sample"]["builds"][f"{other_game}:1"], {"url": "other"})
        build = catalog["mods"]["sample"]["builds"][f"{GAME}:1"]
        self.assertEqual(build["size"], package.stat().st_size)
        if GAME == "mm":
            self.assertEqual(catalog["mods"]["sample"]["variants"]["oot"]["version"], "old-version")
        self.assertEqual(catalog["format"], 2)
        self.assertEqual(catalog["mods"]["sample"]["variants"][GAME]["name"], "Sample")
        self.assertIn(build["url"], build_catalog.summarize(plan, catalog))

    def test_shared_catalog_owns_variants_and_permissions_separately(self):
        catalog = build_catalog.empty_catalog()
        request = {"issue": 11, "author": "test", "repository": "test/mods", "commit": "0" * 40, "directory": "mods"}
        other = "mm" if GAME == "oot" else "oot"
        first = build_catalog.plan(catalog, [self.package(other)], {}, 11, True, False, set(), 1, other, "test")
        build_catalog.apply(catalog, first, request, self.directory, {}, [], [], "https://example.com")
        second_package = self.package(GAME)
        refused = build_catalog.plan(catalog, [second_package], {}, 12, True, False, set(), 1, GAME, "attacker")
        self.assertIn("sample", refused["rejected"])
        second = build_catalog.plan(catalog, [second_package], {}, 12, True, False, set(), 1, GAME, "test")
        self.assertEqual(second["publish"], ["sample"])
        request["issue"] = 12
        build_catalog.apply(catalog, second, request, self.directory, {}, [], [], "https://example.com")
        self.assertEqual(catalog["mods"]["sample"]["variants"][GAME]["issue"], 12)
        self.assertEqual(catalog["mods"]["sample"]["variants"][other]["issue"], 11)
        self.assertIn(f"{GAME}:12", catalog["sources"])
        self.assertIn(f"{other}:11", catalog["sources"])
        build_catalog.set_listed(catalog, 12, "sample", False, GAME)
        self.assertFalse(catalog["mods"]["sample"]["variants"][GAME]["listed"])
        self.assertTrue(catalog["mods"]["sample"]["variants"][other]["listed"])

    def test_document_signatures_are_domain_separated(self):
        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
        from cryptography.hazmat.primitives import serialization
        key = Ed25519PrivateKey.generate()
        public = key.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
        envelope = mod_signature.sign_document('{"format":2}', "catalog", key)
        self.assertEqual(mod_signature.open_document(envelope, "catalog", public), '{"format":2}')
        with self.assertRaises(ValueError):
            mod_signature.open_document(envelope, "release", public)
        tampered = json.loads(envelope)
        tampered["payload"] = '{"format":1}'
        with self.assertRaises(ValueError):
            mod_signature.open_document(json.dumps(tampered), "catalog", public)

if __name__ == "__main__":
    unittest.main()
