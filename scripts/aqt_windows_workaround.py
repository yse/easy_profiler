#!/usr/bin/env python3
"""Workaround for an aqtinstall bug: print the newest Qt 6 it can install on
Windows.

The Linux GUI job asks aqtinstall itself, and so should this one:

    aqt list-qt <host> desktop --spec 6 --latest-version

On Windows that answer, 6.12.0 today, cannot be installed. From Qt 6.11 the
Windows repository keeps one folder per compiler,
windows_x86/desktop/qt6_6120/qt6_6120_msvc2022_64/, where Linux and macOS keep
the flat qt6_6120/qt6_6120/. aqtinstall 3.3.0, the newest on PyPI, still
builds the flat path for Windows, so it fails with "Failed to locate XML data"
before it downloads anything. This walks back from the newest Qt 6 release to
the newest whose package index aqtinstall can read, 6.10.3 today.

This is a workaround for someone else's bug, not a design of this project.
When an aqtinstall that reads the split layout is on PyPI, delete this file
and give the Windows jobs the one-liner above. Do not extend it.
"""

import subprocess
import sys
import tempfile
from pathlib import Path

# The probe reads each release's package index with hash checking off: with it
# on, aqtinstall reports a TLS failure and a missing index in the same words,
# "Failed to download checksum for the file 'Updates.xml'", and only a missing
# index may be walked past. Anything else stops the job with aqt's message
# rather than quietly resolving an older Qt. The probe installs nothing; the
# install step verifies every hash itself. The fallback mirror is download.qt.io
# too, so a 404 is Qt's own answer and not a stale mirror's.
PROBE_SETTINGS = """\
[aqt]
baseurl : https://download.qt.io
[mirrors]
fallbacks :
    https://download.qt.io
[requests]
INSECURE_NOT_FOR_PRODUCTION_ignore_hash : True
"""
MISSING_INDEX = "server response code: 404"


def main():
    with tempfile.TemporaryDirectory() as directory:
        settings = Path(directory, "aqt.ini")
        settings.write_text(PROBE_SETTINGS)

        def aqt(*arguments):
            return subprocess.run(
                [sys.executable, "-m", "aqt", "-c", str(settings),
                 "list-qt", "windows", "desktop", *arguments],
                capture_output=True, text=True)

        listing = aqt("--spec", "6")
        if listing.returncode != 0:
            sys.exit(listing.stderr.strip())
        versions = sorted(listing.stdout.split(), reverse=True,
                          key=lambda v: tuple(int(n) for n in v.split(".")))
        for version in versions:
            index = aqt("--arch", version)
            if index.returncode == 0:
                print(version)
                return
            if MISSING_INDEX not in index.stderr.lower():
                sys.exit(index.stderr.strip())
            print(f"skipping {version}: aqtinstall cannot read its package "
                  "index", file=sys.stderr)
    sys.exit("no Qt 6 release has a package index aqtinstall can read")


if __name__ == "__main__":
    main()
