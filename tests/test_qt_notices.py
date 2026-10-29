"""Tests for scripts/qt_notices.py, which writes the GUI bundle's licence files.

The SBOMs under tests/qt_notices/ are verbatim excerpts of the ones the
official Qt packages carry: qtbase and qtsvg from the Linux arm64 6.12.0
package, qtbase from the Windows msvc2022_64 6.10.3 package, each cut down to
the Core or Svg module, one or two plugins, and the third-party packages, files
and relationships the SBOM ties to them. The Windows excerpt also carries Qt
Network's Public Suffix List entry, with no file of its own: it is there so the
manifest check below sees a licence no Core or Svg component asks for, which is
how the real Windows bundle differs. The licence texts are stand-ins the
test writes, held to a manifest the test writes for them: no GPL text belongs
in this repository. Run with: python3 -m unittest tests/test_qt_notices.py
"""

import contextlib
import hashlib
import io
import os
import pathlib
import shutil
import sys
import tempfile
import unittest
import urllib.error
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir, "scripts"))

import qt_notices  # noqa: E402

FIXTURES = pathlib.Path(__file__).resolve().parent / "qt_notices"

# The standard licence ids the fixture SBOMs refer to, which a real run fetches
# from Qt's LICENSES/ directory.
LINUX_TEXTS = ["Apache-2.0", "BSD-2-Clause", "BSD-3-Clause", "CC0-1.0",
               "GPL-2.0-only", "GPL-3.0-only", "LGPL-3.0-only", "MIT", "Unicode-3.0"]
WINDOWS_TEXTS = LINUX_TEXTS + ["IJG", "Zlib"]

LINUX_BUNDLE = [
    "bin/profiler_gui", "lib/libQt6Core.so.6", "lib/libQt6Svg.so.6",
    "lib/libeasy_profiler.so", "lib/libicuuc.so.73", "lib/libicui18n.so.73",
    "lib/libicudata.so.73", "plugins/platforms/libqoffscreen.so",
    "plugins/iconengines/libqsvgicon.so", "plugins/imageformats/libqsvg.so",
]
WINDOWS_BUNDLE = ["profiler_gui.exe", "easy_profiler.dll", "Qt6Core.dll",
                  "imageformats/qjpeg.dll"]


def text_of(identifier):
    return f"Stand-in text of {identifier}\n"


class Workspace:
    """A Qt install, a bundle, offline licence texts and a manifest, in a temp dir."""

    def __init__(self, version, sboms, bundle, texts):
        self.root = pathlib.Path(tempfile.mkdtemp())
        self.version = version
        self.qt = self.root / "qt"
        (self.qt / "sbom").mkdir(parents=True)
        for name in sboms:
            shutil.copy(FIXTURES / name, self.qt / "sbom" / name)
        self.bundle = self.root / "bundle"
        for relative in bundle:
            self.add(relative)
        self.source = self.root / "source"
        for identifier in texts:
            path = self.source / "LICENSES" / f"{identifier}.txt"
            path.parent.mkdir(parents=True, exist_ok=True)
            # Bytes, because these are the texts the manifest holds to a digest:
            # written as text, Windows would store a file no digest describes.
            path.write_bytes(text_of(identifier).encode("utf-8"))
        self.icu_license = self.root / "icu-LICENSE"
        self.icu_license.write_bytes(text_of("ICU").encode("utf-8"))
        self.cache = self.root / "cache"
        self.out = self.root / "out"
        self.manifest = self.root / "manifest.sha256"
        entries = {f"LICENSES/{i}.txt": text_of(i) for i in texts}
        entries["icu-73.2/LICENSE"] = text_of("ICU")
        self.write_manifest({key: sha256(body) for key, body in entries.items()})

    def add(self, relative):
        path = self.bundle / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"")

    def write_manifest(self, digests):
        self.manifest.write_text(
            "".join(f"{digest}  {key}\n" for key, digest in sorted(digests.items())),
            encoding="utf-8", newline="\n")

    def read_manifest(self):
        entries = {}
        for line in self.manifest.read_text(encoding="utf-8").splitlines():
            if line and not line.startswith("#"):
                digest, key = line.split(None, 1)
                entries.setdefault(key, set()).add(digest)
        return entries

    def run(self, *extra, offline=True, icu=True):
        """Run the generator; return (ok, message) where message is its stderr
        plus the text of a refusal."""
        argv = ["qt_notices.py", "--bundle", str(self.bundle),
                "--qt-install", str(self.qt), "--qt-version", self.version,
                "--out", str(self.out), "--cache-dir", str(self.cache),
                "--manifest", str(self.manifest)]
        if icu:
            argv += ["--icu-version", "73.2", "--icu-license", str(self.icu_license)]
        if offline:
            argv += ["--qt-source", str(self.source)]
        argv += list(extra)
        stdout, stderr = io.StringIO(), io.StringIO()
        with mock.patch.object(sys, "argv", argv), \
                contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            try:
                qt_notices.main()
            except SystemExit as stop:
                return False, f"{stderr.getvalue()}{stop.code}"
        return True, stderr.getvalue()

    def generated(self, name):
        return (self.out / name).read_text(encoding="utf-8")


def sha256(body):
    return hashlib.sha256(body.encode("utf-8")).hexdigest()


def linux_workspace():
    return Workspace("6.12.0", ["qtbase-6.12.0.spdx.json", "qtsvg-6.12.0.spdx.json"],
                     LINUX_BUNDLE, LINUX_TEXTS)


def windows_workspace():
    return Workspace("6.10.3", ["qtbase-6.10.3.spdx.json"], WINDOWS_BUNDLE, WINDOWS_TEXTS)


class RealMetadataTest(unittest.TestCase):
    def test_a_linux_bundle_is_attributed_from_qt_sbom(self):
        space = linux_workspace()
        ok, message = space.run()
        self.assertTrue(ok, message)

        self.assertEqual(space.generated("LICENSE.LGPLv3"), text_of("LGPL-3.0-only"))
        self.assertEqual(space.generated("LICENSE.GPLv3"), text_of("GPL-3.0-only"))
        self.assertEqual(space.generated("LICENSE.ICU"), text_of("ICU"))

        notices = space.generated("THIRD-PARTY-NOTICES.md")
        self.assertIn("Qt version: 6.12.0\nICU version: 73.2\n", notices)
        # The module revision and configure line come from the SBOM root.
        self.assertIn("- `qtbase` 025bdad1+HEAD", notices)
        self.assertIn("qtbase was configured with:", notices)
        # Core's embedded PCRE2, tied to the library it ships in.
        self.assertIn("### PCRE2\n", notices)
        self.assertIn("- Shipped in: lib/libQt6Core.so.6\n", notices)
        # A LicenseRef- text comes out of the SBOM, a standard one from LICENSES/.
        self.assertIn("### LicenseRef-SHA1-Public-Domain\n\nSource: Qt 6.12.0 SBOM", notices)
        self.assertIn("### MIT\n\nSource: Qt 6.12.0 LICENSES/MIT.txt\n\n    Stand-in text of MIT",
                      notices)

        notice = space.generated("QT_NOTICE.md")
        self.assertIn("version 6.12.0, under the GNU Lesser", notice)
        self.assertIn("    qtbase-everywhere-src-6.12.0.tar.xz\n"
                      "    qtsvg-everywhere-src-6.12.0.tar.xz\n", notice)
        self.assertIn("https://download.qt.io/archive/qt/6.12/6.12.0/submodules/", notice)
        self.assertIn("`LICENSE.ICU`", notice)
        self.assertNotIn("Independent JPEG Group", notice)

    def test_a_windows_bundle_names_libjpeg_and_carries_no_icu(self):
        space = windows_workspace()
        ok, message = space.run(icu=False)
        self.assertTrue(ok, message)

        self.assertFalse((space.out / "LICENSE.ICU").exists())
        notices = space.generated("THIRD-PARTY-NOTICES.md")
        self.assertNotIn("ICU", notices.split("## Third-party")[0])
        self.assertIn("- Shipped in: imageformats/qjpeg.dll\n", notices)
        self.assertIn("### IJG\n\nSource: Qt 6.10.3 LICENSES/IJG.txt", notices)

        notice = space.generated("QT_NOTICE.md")
        self.assertIn("This software is based in part on the work of the Independent JPEG Group",
                      notice)
        self.assertNotIn("ICU", notice)
        self.assertIn("    qtbase-everywhere-src-6.10.3.tar.xz\n", notice)


class RefusalTest(unittest.TestCase):
    def test_a_library_no_sbom_describes_is_refused_before_anything_is_written(self):
        space = linux_workspace()
        space.add("lib/libfoo.so.1")
        ok, message = space.run()
        self.assertFalse(ok)
        self.assertIn("lib/libfoo.so.1", message)
        self.assertIn("unattributed", message)
        self.assertFalse(space.out.exists())

    def test_icu_version_must_match_what_the_bundle_carries(self):
        ok, message = linux_workspace().run(icu=False)
        self.assertFalse(ok)
        self.assertIn("--icu-version is required", message)
        self.assertIn("lib/libicuuc.so.73", message)

        ok, message = windows_workspace().run(icu=True)
        self.assertFalse(ok)
        self.assertIn("carries no ICU", message)

    def test_a_qt_without_sboms_is_refused(self):
        space = linux_workspace()
        space.version = "6.99.0"
        ok, message = space.run()
        self.assertFalse(ok)
        self.assertIn("official Qt binary packages", message)

    def test_a_text_missing_from_the_manifest_is_refused(self):
        space = linux_workspace()
        digests = {key: next(iter(d)) for key, d in space.read_manifest().items()}
        del digests["LICENSES/MIT.txt"]
        space.write_manifest(digests)
        ok, message = space.run()
        self.assertFalse(ok)
        self.assertIn("LICENSES/MIT.txt is not in", message)
        self.assertIn("--update-manifest", message)

    def test_update_manifest_adds_and_never_removes(self):
        space = linux_workspace()
        digests = {key: next(iter(d)) for key, d in space.read_manifest().items()}
        digests["LICENSES/MIT.txt"] = "0" * 64
        digests["LICENSES/Unrelated.txt"] = "1" * 64
        space.write_manifest(digests)
        ok, message = space.run("--update-manifest")
        self.assertTrue(ok, message)
        self.assertIn("recording LICENSES/MIT.txt", message)
        entries = space.read_manifest()
        self.assertEqual(entries["LICENSES/MIT.txt"], {"0" * 64, sha256(text_of("MIT"))})
        self.assertEqual(entries["LICENSES/Unrelated.txt"], {"1" * 64})


class FetchTest(unittest.TestCase):
    """The online path: texts come from the cache or the network, and are held
    to the manifest either way."""

    def cached(self, space, identifier):
        return space.cache / f"qt-{space.version}" / "LICENSES" / f"{identifier}.txt"

    def serving(self, body_of):
        """Patch urlopen so each URL answers with body_of(url): bytes, or an
        exception to raise."""
        def urlopen(url, timeout=None):
            answer = body_of(url)
            if isinstance(answer, BaseException):
                raise answer
            response = mock.MagicMock()
            response.__enter__.return_value.read.return_value = answer
            return response
        return mock.patch.object(qt_notices.urllib.request, "urlopen", side_effect=urlopen)

    def test_a_corrupted_cached_text_is_fetched_again_and_the_cache_repaired(self):
        space = linux_workspace()
        stale = self.cached(space, "MIT")
        stale.parent.mkdir(parents=True)
        stale.write_text("corrupted\n", encoding="utf-8")
        fetched = []

        def body_of(url):
            fetched.append(url)
            return text_of(url.split("/LICENSES/")[1].split(".txt")[0]).encode("utf-8")

        with self.serving(body_of):
            ok, message = space.run(offline=False)
        self.assertTrue(ok, message)
        self.assertIn("discarding cached", message)
        self.assertEqual(stale.read_text(encoding="utf-8"), text_of("MIT"))
        self.assertEqual(sum("/MIT.txt" in url for url in fetched), 1)

    def test_a_text_that_still_mismatches_when_freshly_fetched_is_refused(self):
        space = linux_workspace()
        stale = self.cached(space, "MIT")
        stale.parent.mkdir(parents=True)
        stale.write_text("corrupted\n", encoding="utf-8")

        def body_of(url):
            if "/MIT.txt" in url:
                return b"still corrupted\n"
            return text_of(url.split("/LICENSES/")[1].split(".txt")[0]).encode("utf-8")

        with self.serving(body_of):
            ok, message = space.run(offline=False)
        self.assertFalse(ok)
        self.assertIn("does not match the manifest even when freshly fetched", message)
        self.assertFalse(space.out.exists())

    def test_a_network_failure_is_refused_and_names_every_source(self):
        space = linux_workspace()
        with self.serving(lambda url: urllib.error.URLError("injected failure")):
            ok, message = space.run(offline=False)
        self.assertFalse(ok)
        self.assertIn("could not fetch a licence text from any source", message)
        self.assertIn("code.qt.io/cgit/qt/qtbase.git", message)
        self.assertIn("raw.githubusercontent.com/qt/qtbase", message)
        self.assertIn("injected failure", message)
        self.assertFalse(space.out.exists())

    def test_a_404_moves_on_to_the_next_source(self):
        space = linux_workspace()
        asked = []

        def body_of(url):
            asked.append(url)
            if "code.qt.io" in url:
                return urllib.error.HTTPError(url, 404, "Not Found", {}, None)
            return text_of(url.split("/LICENSES/")[1].split(".txt")[0]).encode("utf-8")

        with self.serving(body_of):
            ok, message = space.run(offline=False)
        self.assertTrue(ok, message)
        self.assertTrue(any("raw.githubusercontent.com" in url for url in asked))
        self.assertEqual(space.generated("LICENSE.LGPLv3"), text_of("LGPL-3.0-only"))


class RealManifestCoversTheFixtures(unittest.TestCase):
    """The manifest in the repository, against the licences real Qt asks for.

    Every other test here runs on a manifest the test writes, which checks the
    generator but says nothing about the file we ship with. A missing entry is
    how the JPEG plugin's IJG licence reached a release branch: nothing failed
    until a bundle was assembled. This reads the ids out of the real SBOM
    excerpts and requires the real manifest to carry each one. The texts and
    their digests stay unchecked here - only assembling a bundle can do that -
    so no licence text has to live in the repository.
    """

    def test_every_licence_id_in_the_fixtures_is_in_the_manifest(self):
        manifest = pathlib.Path(__file__).resolve().parents[1] / "scripts"
        manifest = manifest / "qt_notices.sha256"
        recorded = {line.split("  ", 1)[1].strip()
                    for line in manifest.read_text(encoding="utf-8").splitlines()
                    if line.strip() and not line.startswith("#")}

        required = {}
        for fixture in sorted(FIXTURES.glob("*.spdx.json")):
            sbom = qt_notices.load_sbom(fixture)
            extracted = {entry["licenseId"] for entry
                         in sbom.get("hasExtractedLicensingInfos", [])}
            for package in sbom.get("packages", []):
                expression = qt_notices.clean(package.get("licenseConcluded"))
                for identifier in qt_notices.spdx_ids(expression or ""):
                    # Qt carries the text of its own LicenseRef- ids in the
                    # SBOM, so those are never fetched and never recorded.
                    if identifier in extracted or identifier == "NOASSERTION":
                        continue
                    required.setdefault(identifier, fixture.name)

        missing = sorted((f"LICENSES/{i}.txt", source)
                         for i, source in required.items()
                         if f"LICENSES/{i}.txt" not in recorded)
        self.assertEqual(missing, [], "licence ids named by the SBOM fixtures "
                                      "but absent from scripts/qt_notices.sha256")


if __name__ == "__main__":
    unittest.main()
