"""A stand-in for `python -m aqt list-qt`, for tests that cannot reach Qt's
servers. Put tests/support/fake_aqt on PYTHONPATH.

FAKE_AQT lists the releases and what probing each one's package index does,
as "6.12.0:404 6.10.3:ok" - 404 for a missing index, ssl for a TLS failure.
FAKE_AQT_LISTING=fail makes the version listing itself fail. Every invocation
is appended to FAKE_AQT_LOG as JSON, with the settings file it was handed.

The messages are aqtinstall 3.3.0's own, verbatim, as it prints them on the
real code path: a 404 and a connection failure both arrive at the resolver
through the same `except` in aqt, and only their wording tells them apart.
"""

import json
import os
import sys

TRAILER = ("==============================Suggested follow-up:"
           "==============================\n"
           "* Please use 'aqt list-qt windows desktop' to show versions of Qt available.")


def main():
    arguments = sys.argv[1:]
    settings = None
    if arguments[:1] == ["-c"]:
        with open(arguments[1], encoding="utf-8") as handle:
            settings = handle.read()
        arguments = arguments[2:]
    if os.environ.get("FAKE_AQT_LOG"):
        with open(os.environ["FAKE_AQT_LOG"], "a", encoding="utf-8") as handle:
            handle.write(json.dumps({"arguments": arguments, "settings": settings}) + "\n")

    if arguments[:3] != ["list-qt", "windows", "desktop"]:
        sys.exit(f"fake aqt does not understand {arguments}")
    outcomes = dict(item.split(":") for item in os.environ["FAKE_AQT"].split())

    if "--spec" in arguments:
        if os.environ.get("FAKE_AQT_LISTING") == "fail":
            print("ERROR   : Failure to connect to https://download.qt.io/online/"
                  "qtsdkrepository/windows_x86/desktop/: ConnectionError\n" + TRAILER,
                  file=sys.stderr)
            sys.exit(1)
        print(" ".join(sorted(outcomes)))
        return

    version = arguments[arguments.index("--arch") + 1]
    folder = "qt6_" + version.replace(".", "")
    url = (f"https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/"
           f"{folder}/{folder}/Updates.xml")
    outcome = outcomes[version]
    if outcome == "ok":
        print("win64_llvm_mingw win64_mingw win64_msvc2022_64")
        return
    if outcome == "404":
        print(f"ERROR   : Failed to retrieve file at {url}\n"
              f"Server response code: 404, reason: Not Found\n{TRAILER}", file=sys.stderr)
    elif outcome == "ssl":
        print(f"ERROR   : Failure to connect to {url}: SSLError\n{TRAILER}", file=sys.stderr)
    else:
        sys.exit(f"fake aqt has no outcome {outcome!r}")
    sys.exit(1)


if __name__ == "__main__":
    main()
