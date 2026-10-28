#!/usr/bin/env python3
"""Print the newest Qt 6 release aqtinstall can install for a host and arch.

The release pipeline does not pin a Qt version: it bundles whatever the current
open-source release is on the day it runs, and this is the one place that
decides what "current" means. Newest first, a release counts only when
aqtinstall can read its package index for the requested architecture and the
required modules are in it. The index check is what makes the Windows jobs
work today: from Qt 6.11 the Windows repository is laid out per compiler, which
aqtinstall 3.3.0 cannot read, so those releases are skipped until an aqtinstall
that can read them is on PyPI, at which point they resolve with no change here.

Usage:
  scripts/resolve_qt_version.py <host> <arch> [--module qt5compat ...]
    host  windows | linux | linux_arm64 | mac     (aqtinstall's names)
    arch  the aqtinstall architecture, e.g. win64_msvc2022_64, linux_gcc_64,
          clang_64
"""

import argparse
import subprocess
import sys


def aqt(*arguments):
    """Run an aqt subcommand; stdout on success, None when aqt refused."""
    result = subprocess.run([sys.executable, "-m", "aqt", *arguments],
                            capture_output=True, text=True)
    if result.returncode != 0:
        return None
    return result.stdout


def version_key(version):
    return tuple(int(part) for part in version.split("."))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("host")
    parser.add_argument("arch")
    parser.add_argument("--module", action="append", default=[],
                        help="a module that must be published for the release"
                             " to count, e.g. qt5compat")
    args = parser.parse_args()

    # Qt 7 would not be a newer Qt for this GUI, which is written against Qt 6
    # and the Qt5Compat module: this is a compatibility bound, not a pin.
    listing = aqt("list-qt", args.host, "desktop", "--spec", "6")
    if listing is None:
        sys.exit(f"aqt could not list Qt 6 releases for {args.host}")
    versions = sorted({v for v in listing.split() if v[0].isdigit()},
                      key=version_key, reverse=True)
    if not versions:
        sys.exit(f"aqt lists no Qt 6 release for {args.host}")

    for version in versions:
        arches = aqt("list-qt", args.host, "desktop", "--arch", version)
        if arches is None or args.arch not in arches.split():
            print(f"skipping {version}: aqt cannot read its {args.arch}"
                  " package index", file=sys.stderr)
            continue
        if args.module:
            modules = aqt("list-qt", args.host, "desktop",
                          "--modules", version, args.arch)
            missing = [m for m in args.module
                       if modules is None or m not in modules.split()]
            if missing:
                print(f"skipping {version}: {', '.join(missing)} not"
                      " published", file=sys.stderr)
                continue
        print(version)
        return
    sys.exit(f"no Qt 6 release is installable for {args.host} {args.arch}")


if __name__ == "__main__":
    main()
