"""Tests for scripts/changelog_section.py.

The script builds the GitHub release body, so a defect in it ships wrong
release notes. Run with: python3 -m unittest tests/test_changelog_section.py
"""

import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir, "scripts"))

import changelog_section  # noqa: E402

SAMPLE = """# Changelog

## 3.0.0 - 2026-10-27

Intro line.

### Breaking changes

- Something broke.

## 2.1.0 - 2019-01-01

### Fixed

- Older entry.
"""

UNRELEASED = """# Changelog

## 3.1.0 - Unreleased

- Pending work.

## 3.0.0 - 2026-10-27

- Shipped work.
"""

EMPTY = """# Changelog

## 3.0.0 - 2026-10-27

## 2.1.0 - 2019-01-01

- Older entry.
"""


class FindSectionTest(unittest.TestCase):
    def test_extracts_only_the_requested_version(self):
        body, rest = changelog_section.find_section(SAMPLE, "3.0.0")
        self.assertIn("Something broke.", body)
        self.assertNotIn("Older entry.", body)
        self.assertNotIn("## 2.1.0", body)
        self.assertEqual(rest, "- 2026-10-27")

    def test_extracts_the_last_section(self):
        body, _ = changelog_section.find_section(SAMPLE, "2.1.0")
        self.assertIn("Older entry.", body)
        self.assertNotIn("Something broke.", body)

    def test_missing_version_raises(self):
        with self.assertRaises(ValueError):
            changelog_section.find_section(SAMPLE, "9.9.9")

    def test_a_prefix_of_a_version_is_not_a_match(self):
        with self.assertRaises(ValueError):
            changelog_section.find_section(SAMPLE, "3.0")

    def test_unreleased_marker_is_reported_in_the_heading(self):
        _, rest = changelog_section.find_section(UNRELEASED, "3.1.0")
        self.assertIn("Unreleased", rest)


class MainTest(unittest.TestCase):
    def run_main(self, text, *args):
        handle = tempfile.NamedTemporaryFile("w", suffix=".md", delete=False, encoding="utf-8")
        try:
            handle.write(text)
            handle.close()
            return changelog_section.main(list(args) + [handle.name])
        finally:
            os.unlink(handle.name)

    def test_released_section_succeeds(self):
        self.assertEqual(self.run_main(SAMPLE, "3.0.0"), 0)

    def test_missing_section_fails(self):
        self.assertEqual(self.run_main(SAMPLE, "9.9.9"), 1)

    def test_empty_section_fails(self):
        self.assertEqual(self.run_main(EMPTY, "3.0.0"), 1)

    def test_unreleased_is_allowed_without_the_flag(self):
        self.assertEqual(self.run_main(UNRELEASED, "3.1.0"), 0)

    def test_unreleased_fails_when_a_release_is_required(self):
        self.assertEqual(self.run_main(UNRELEASED, "3.1.0", "--require-released"), 1)

    def test_dated_section_passes_when_a_release_is_required(self):
        self.assertEqual(self.run_main(SAMPLE, "3.0.0", "--require-released"), 0)


if __name__ == "__main__":
    unittest.main()
