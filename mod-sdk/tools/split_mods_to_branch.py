"""Move the mods out of the host checkout into a mods-only branch worktree, as Ship keeps unbound-mod-nei.

Rerunning moves any new mod folders and refreshes UNBOUND_REF with the host commit the mods build against.
"""
import argparse
import shutil
import subprocess
import sys
from pathlib import Path

SDK_DIRECTORY = Path(__file__).resolve().parent.parent
REPOSITORY_ROOT = SDK_DIRECTORY.parent
HOST_MODS = SDK_DIRECTORY / "mods"
HOST_ONLY_ENTRIES = {"README.md"}


def git(*arguments, cwd=REPOSITORY_ROOT):
    result = subprocess.run(["git", *arguments], cwd=cwd, check=True, capture_output=True, text=True)
    return result.stdout.strip()


def has_branch(branch):
    return subprocess.run(["git", "show-ref", "--verify", "--quiet", f"refs/heads/{branch}"],
                          cwd=REPOSITORY_ROOT).returncode == 0


def ensure_worktree(worktree, branch, dry_run):
    if worktree.exists():
        return
    command = ["worktree", "add", str(worktree), branch] if has_branch(branch) else \
              ["worktree", "add", "-b", branch, str(worktree), "HEAD"]
    print("git", " ".join(command))
    if not dry_run:
        git(*command)


def list_movable_entries():
    return sorted(entry for entry in HOST_MODS.iterdir() if entry.name not in HOST_ONLY_ENTRIES)


def move_entries(entries, destination, dry_run):
    collisions = [entry.name for entry in entries if (destination / entry.name).exists()]
    if collisions:
        raise SystemExit(f"Already in {destination}, nothing moved: {', '.join(collisions)}")
    if not dry_run:
        destination.mkdir(parents=True, exist_ok=True)
    for entry in entries:
        print(f"move {entry.name}")
        if not dry_run:
            shutil.move(str(entry), str(destination / entry.name))


def write_unbound_ref(worktree, dry_run):
    host_commit = git("rev-parse", "HEAD")
    if git("status", "--porcelain", "--untracked-files=no"):
        print(f"warning: the host has uncommitted changes; UNBOUND_REF names {host_commit[:9]}, "
              "rerun after committing the host")
    print(f"UNBOUND_REF {host_commit}")
    if not dry_run:
        (worktree / "UNBOUND_REF").write_text(host_commit + "\n", encoding="ascii")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--branch", default="2ship-unbound-mod-nei")
    parser.add_argument("--worktree", type=Path, default=REPOSITORY_ROOT.parent / "2ship-unbound-mod-nei")
    parser.add_argument("--dry-run", action="store_true", help="Print what would happen without changing anything")
    args = parser.parse_args()

    worktree = args.worktree.resolve()
    ensure_worktree(worktree, args.branch, args.dry_run)
    move_entries(list_movable_entries(), worktree / "mod-sdk" / "mods", args.dry_run)
    write_unbound_ref(worktree, args.dry_run)
    print(f"\nBuild them against this host with:\n  python mod-sdk/build_mods.py --mods-dir "
          f"\"{(worktree / 'mod-sdk' / 'mods').as_posix()}\" --game <game folder>")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        print(error.stderr, file=sys.stderr)
        sys.exit(error.returncode)
