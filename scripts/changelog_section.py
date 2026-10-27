#!/usr/bin/env python3
"""Print one version's section of CHANGELOG.md.

The release workflow uses this to build the GitHub release body, so nobody
copies the changelog by hand. It also serves as the release-day gate: a section
that is missing, empty or still marked Unreleased exits non-zero.
"""

import argparse
import re
import sys

UNRELEASED = "unreleased"


def find_section(text, version):
    """Return the body under '## <version>', or raise ValueError."""
    heading = re.compile(
        r"^##[ \t]+" + re.escape(version) + r"(?P<rest>[ \t].*)?$",
        re.MULTILINE,
    )
    match = heading.search(text)
    if match is None:
        raise ValueError("CHANGELOG.md has no '## {}' section".format(version))

    following = re.compile(r"^## ", re.MULTILINE).search(text, match.end())
    end = following.start() if following else len(text)

    return text[match.end():end].strip("\n"), (match.group("rest") or "").strip()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version", help="version to extract, for example 3.0.0")
    parser.add_argument("changelog", help="path to CHANGELOG.md")
    parser.add_argument(
        "--require-released",
        action="store_true",
        help="fail when the heading is still marked Unreleased (use on a tag)",
    )
    args = parser.parse_args(argv)

    with open(args.changelog, "r", encoding="utf-8") as handle:
        text = handle.read()

    try:
        body, heading_rest = find_section(text, args.version)
    except ValueError as error:
        print(str(error), file=sys.stderr)
        return 1

    if not body.strip():
        print(
            "the '## {}' section of CHANGELOG.md is empty".format(args.version),
            file=sys.stderr,
        )
        return 1

    if args.require_released and UNRELEASED in heading_rest.lower():
        print(
            "the '## {}' heading is still marked Unreleased; date it before "
            "tagging".format(args.version),
            file=sys.stderr,
        )
        return 1

    sys.stdout.write(body + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
