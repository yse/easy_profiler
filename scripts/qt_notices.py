#!/usr/bin/env python3
"""Generate the Qt licence texts and notices the GUI bundle ships.

Writes into --out: LICENSE.LGPLv3, LICENSE.GPLv3, LICENSE.ICU when the bundle
carries ICU, THIRD-PARTY-NOTICES.md with the attributions, and QT_NOTICE.md,
the notice to the user that names the Qt and points at its source. Nothing
here is kept in the repository: easy_profiler is MIT and Apache-2.0, and the
LGPL's obligations attach to the bundle that carries Qt, not to this source
tree. The bundle scripts run this at bundle time and refuse to ship without
its output; tests/test_qt_notices.py holds it to its contract.

Inputs, all authoritative, none edited by hand:

  --bundle       the assembled bundle. Every shared library in it is looked up
                 in the SBOM and attributed; one that cannot be is an error,
                 so a library added to a bundle script cannot ship silently.
  --qt-install   the official Qt install the bundle is assembled from. Its
                 sbom/<module>-<ver>.spdx.json names every third-party package
                 each Qt target depends on, with the upstream version,
                 copyright and SPDX licence expression, and embeds the text of
                 the LicenseRef- licences Qt defines itself.
  licence texts  the verbatim LICENSES/<id>.txt files of the matching Qt
                 modules, fetched from the v<ver> tag on code.qt.io (or its
                 GitHub mirror) and kept in --cache-dir; or read offline from
                 --qt-source, an unpacked qt*-everywhere-src tree.
  ICU's LICENSE  fetched from the ICU release tag, or given as --icu-license.
                 Qt's official Linux binaries carry ICU in lib/, so a Linux
                 bundle redistributes it, but Qt's SBOM records it as a system
                 dependency with no licence of its own. The Windows and macOS
                 packages carry no ICU, so --icu-version is required exactly
                 when the bundle has it.

Every text that does not come out of the SBOM is checked against the checksums
in qt_notices.sha256 next to this script, so a release is built from known
content whether it was fetched or cached, and a moved tag or a poisoned cache
cannot put a different text into a bundle. The manifest records reviewed
licence texts, not Qt versions: a standard licence text is the same file in
every Qt release that refers to it, so a new Qt needs nothing here unless it
refers to a licence no release before it did, or Qt changed the bytes of a
text. Either case fails the build until the text is reviewed and recorded with
--update-manifest, which adds to the manifest and never removes from it.

Usage:
  scripts/qt_notices.py --bundle <bundle> --qt-install <dir> \
      --qt-version <version qtpaths6 reports> [--icu-version 73.2] \
      --out <bundle>/licenses [--cache-dir <dir>]
"""

import argparse
import hashlib
import json
import os
import pathlib
import re
import sys
import textwrap
import urllib.error
import urllib.request

# Every shared library a bundle can legitimately carry that no Qt SBOM
# describes: easy_profiler's own, and ICU, which Qt records as a system
# dependency and which this script attributes in a section of its own. Anything
# else in the bundle is a library whose third-party code would ship
# unattributed, and is a hard error rather than a silent omission.
EASY_PROFILER_LIBRARY = re.compile(r"^(lib)?easy_profiler", re.IGNORECASE)
ICU_LIBRARY = re.compile(r"^(lib)?icu", re.IGNORECASE)

# What a bundle loads at run time. Static libraries and import libraries appear
# in Qt's SBOM but are never shipped, so they are not scanned for.
SHARED_LIBRARY = re.compile(r"\.(so|dylib|dll)(\.[0-9.]+)?$", re.IGNORECASE)

# The suffixes that distinguish one build of a Qt target from another without
# changing which target it is.
VERSIONED_SUFFIX = re.compile(r"\.(so|dylib|dll|a|lib)(\.[0-9.]+)?$", re.IGNORECASE)

# Qt's SBOM gives every Qt target the same disjunction, including the commercial
# option that does not apply to a bundle built from the open-source packages.
QT_LICENCE_TEXTS = ["LGPL-3.0-only", "GPL-3.0-only"]

# code.qt.io is where Qt's release tags are cut; GitHub mirrors it. A text is
# tried in every module's LICENSES/ in turn, because the SBOM does not say
# which module's copy a licence id refers to and they are the same file.
QT_TEXT_URLS = [
    "https://code.qt.io/cgit/qt/{module}.git/plain/LICENSES/{id}.txt?h=v{version}",
    "https://raw.githubusercontent.com/qt/{module}/v{version}/LICENSES/{id}.txt",
]
ICU_LICENSE_URL = ("https://raw.githubusercontent.com/unicode-org/icu/"
                   "release-{major}-{minor}/icu4c/LICENSE")

MANIFEST = pathlib.Path(__file__).resolve().with_name("qt_notices.sha256")

_OPERATORS = {"AND", "OR", "WITH"}


def spdx_ids(expression):
    """Split an SPDX licence expression into the individual licence ids."""
    tokens = re.split(r"[()\s]+", expression)
    return [t for t in tokens if t and t not in _OPERATORS]


def load_sbom(path):
    # Qt's generator emits literal newlines inside JSON strings.
    with path.open(encoding="utf-8") as handle:
        return json.load(handle, strict=False)


def library_key(name):
    """A platform-independent handle for one Qt library or plugin file.

    One Qt target is installed under a different file name on every platform
    and under a different one again inside a bundle: Core is
    lib/libQt6Core.so.6.12.0 in a Linux SBOM but libQt6Core.so.6 once copied,
    bin/Qt6Core.dll and lib/Qt6Core.lib on Windows, and the QtCore binary
    inside a framework on macOS. Comparing file names verbatim therefore finds
    nothing; comparing content does not work either, because macOS code-signs
    the binaries after the SBOM records their checksums.
    """
    stem = VERSIONED_SUFFIX.sub("", pathlib.PurePosixPath(name).name)
    if stem.startswith("lib"):
        stem = stem[3:]
    return stem.lower()


def shipped_index(qt_install, version):
    """Map every library and plugin Qt installs to the target it was built from.

    Qt's SBOM carries a `files` array of install-relative paths, each tied by a
    CONTAINS relationship to the package of the CMake target it came out of.
    That is what lets the shipped set be read off the bundle instead of kept in
    a table here, which is the only form of the list that cannot fall behind
    the bundle it describes.
    """
    sboms = sorted((qt_install / "sbom").glob(f"*-{version}.spdx.json"))
    if not sboms:
        sys.exit(f"no Qt {version} SBOMs under {qt_install / 'sbom'}. Only the"
                 " official Qt binary packages carry them; a distribution's or"
                 " Homebrew's Qt does not, and without them the bundle cannot"
                 " be attributed. A bundle that redistributes Qt has to be"
                 " built from an official package.")

    index = {}
    for path in sboms:
        module = path.name[: -len(f"-{version}.spdx.json")]
        sbom = load_sbom(path)
        files = {f["SPDXID"]: f for f in sbom.get("files", [])}
        if not files:
            continue
        owners = {}
        for relationship in sbom["relationships"]:
            related = relationship.get("relatedSpdxElement")
            if related not in files or relationship["relationshipType"] != "CONTAINS":
                continue
            owner = relationship["spdxElementId"]
            if "-qt-module-" in owner or "-qt-plugin-" in owner:
                owners.setdefault(related, set()).add(owner)

        for identifier, entry in files.items():
            candidates = owners.get(identifier, set())
            if len(candidates) != 1:
                continue
            key = library_key(entry["fileName"])
            found = (module, path, next(iter(candidates)))
            previous = index.setdefault(key, found)
            if previous[2] != found[2]:
                sys.exit(f"{entry['fileName']} and a file in {previous[0]} both"
                         f" reduce to {key} but come from different Qt targets;"
                         " the bundle cannot be attributed by file name")
    return index


def third_party_of(sbom, root):
    """Every third-party package the Qt target at `root` depends on, per the SBOM."""
    by_id = {p["SPDXID"]: p for p in sbom["packages"]}
    collected, seen, queue = {}, {root}, [root]
    while queue:
        current = queue.pop()
        for relationship in sbom["relationships"]:
            if relationship["spdxElementId"] != current:
                continue
            if relationship["relationshipType"] not in ("DEPENDS_ON", "CONTAINS"):
                continue
            related = relationship.get("relatedSpdxElement")
            package = by_id.get(related)
            if package is None or related in seen:
                continue
            seen.add(related)
            # A bundled third-party module can itself pull in further
            # attributions, so keep walking through those but not through Qt's
            # own modules: each of those is listed separately on its own.
            if "-qt-bundled-3rdparty-module-" in related:
                collected[package["name"]] = package
                queue.append(related)
            elif "-qt-3rdparty-sources-" in related:
                collected[package["name"]] = package
    return collected


def scan_bundle(bundle, index):
    """Resolve every shared library in the bundle to the Qt target it came from.

    Returns the Qt targets keyed by SBOM package id, the ICU libraries found,
    and exits if any library is left that nothing attributes.
    """
    if not bundle.is_dir():
        sys.exit(f"--bundle {bundle} is not a directory")

    targets, icu, unattributed = {}, [], []
    for path in sorted(p for p in bundle.rglob("*") if p.is_file()):
        name = path.name
        parts = path.relative_to(bundle).parts
        # A framework's binary carries no extension at all, as in
        # QtCore.framework/Versions/A/QtCore, so an extension test alone walks
        # straight past every Qt library in a macOS bundle.
        if not (SHARED_LIBRARY.search(name)
                or f"{name}.framework" in parts):
            continue
        relative = path.relative_to(bundle).as_posix()
        found = index.get(library_key(name))
        if found is not None:
            module, sbom_path, package = found
            targets.setdefault(package, (module, sbom_path, set()))[2].add(relative)
        elif ICU_LIBRARY.match(name):
            icu.append(relative)
        elif not EASY_PROFILER_LIBRARY.match(name):
            unattributed.append(relative)

    if unattributed:
        sys.exit("the bundle carries libraries no SBOM in the Qt install"
                 " describes, so their third-party code would ship"
                 " unattributed:\n  " + "\n  ".join(unattributed))
    if not targets:
        sys.exit(f"no Qt library or plugin was found in {bundle}")
    return targets, icu


def module_order(modules):
    """qtbase first, the rest alphabetically.

    Only the order the modules are listed in depends on this; qtbase leads
    because every other Qt module is built on it, and because the notices quote
    the configure summary its SBOM carries.
    """
    leading = ["qtbase"] if "qtbase" in modules else []
    return leading + sorted(m for m in modules if m != "qtbase")


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 16), b""):
            digest.update(chunk)
    return digest.hexdigest()


def clean(value):
    if not value or value == "NOASSERTION":
        return None
    return " ".join(value.split())


def describe(package):
    """Pull the human-readable attribution out of an SBOM package comment.

    Qt's SBOM names its packages after CMake targets, so the component a lawyer
    needs to read about appears as `Core_Attribution_blake2`. The comment keeps
    the fields of the qt_attribution.json entry the package was generated from,
    including the upstream name and the path of that file.
    """
    fields = {}
    for line in package.get("comment", "").splitlines():
        match = re.match(r"\s*(Id|Name|Qt usage|License):\s*(.+)$", line)
        if match:
            fields[match.group(1)] = match.group(2).strip()
        elif line.strip().startswith("/src_dir/"):
            fields["source"] = line.strip()
    return fields


class Manifest:
    """The checksums every non-SBOM licence text is held to.

    A key names a text, not the Qt release it was fetched from, and may carry
    several digests: every one of them is a version of that text somebody has
    reviewed, so two Qt releases that ship the same licence with different
    bytes can both build once both have been seen.
    """

    def __init__(self, path, update):
        self.path = path
        self.update = update
        self.entries = {}
        if path.is_file():
            for line in path.read_text(encoding="utf-8").splitlines():
                if line.strip() and not line.startswith("#"):
                    digest, key = line.split(None, 1)
                    self.entries.setdefault(key, set()).add(digest)
        self.recorded = []

    def check(self, key, path):
        digest = sha256(path)
        accepted = self.entries.get(key, set())
        if self.update:
            if digest not in accepted:
                # A hash diff says nothing about what the text became, and this
                # is the one moment a human is meant to judge the content. Show
                # enough of it that an obviously wrong file cannot pass review.
                body = path.read_text(encoding="utf-8", errors="replace")
                first = next((l.strip() for l in body.splitlines() if l.strip()), "")
                print(f"recording {key}\n"
                      f"  sha256 {digest}\n"
                      f"  {path.stat().st_size} bytes, first line: {first[:100]}",
                      file=sys.stderr)
                self.recorded.append(key)
            self.entries.setdefault(key, set()).add(digest)
            return True
        if not accepted:
            sys.exit(f"{key} is not in {self.path}; review {path} and record it"
                     f" with --update-manifest\n  sha256 {digest}")
        if digest not in accepted:
            print(f"{key}: sha256 {digest}, expected one of "
                  + ", ".join(sorted(accepted)), file=sys.stderr)
            return False
        return True

    def save(self):
        lines = ["# sha256 of every licence text scripts/qt_notices.py uses that",
                 "# does not come out of Qt's SBOM: the LICENSES/<id>.txt files of",
                 "# Qt's module repositories and ICU's LICENSE. Keyed by text, not",
                 "# by Qt release, since the texts are the same across releases.",
                 "# Maintained by --update-manifest, which adds and never removes.",
                 ""]
        lines += [f"{digest}  {key}"
                  for key, digests in sorted(self.entries.items())
                  for digest in sorted(digests)]
        self.path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def fetch(urls, destination):
    """Fetch the first URL that answers; a 404 means try the next one."""
    failures = []
    for url in urls:
        try:
            # Short: six URLs at 60s each means a blackholed network takes six
            # minutes to report what a refused connection reports in a second.
            with urllib.request.urlopen(url, timeout=15) as response:
                body = response.read()
        except urllib.error.HTTPError as error:
            failures.append(f"{url}: HTTP {error.code}")
            continue
        except (urllib.error.URLError, OSError) as error:
            failures.append(f"{url}: {error}")
            continue
        destination.parent.mkdir(parents=True, exist_ok=True)
        partial = destination.with_suffix(destination.suffix + ".part")
        partial.write_bytes(body)
        os.replace(partial, destination)
        print(f"fetched {url}")
        return
    sys.exit("could not fetch a licence text from any source:\n  "
             + "\n  ".join(failures))


class Texts:
    """Resolves each licence text to a verified file, fetching when allowed."""

    def __init__(self, args, manifest, modules):
        self.args = args
        self.manifest = manifest
        self.modules = modules
        self.offline = {}
        for directory in args.qt_source:
            for path in sorted(directory.rglob("LICENSES/*.txt")):
                self.offline.setdefault(path.stem, path)
        if args.qt_source and not self.offline:
            sys.exit("no LICENSES/*.txt found under the given Qt source trees")

    def verified(self, key, path, urls):
        """Return path once its checksum is known good, fetching if needed.

        A cached copy that fails the check is fetched again once: a stale
        cache from before a manifest update is the common case, and a text
        that still mismatches after a fresh fetch is the one worth a failure.
        """
        if urls is None:
            if not path.is_file():
                sys.exit(f"{path} does not exist")
            if not self.manifest.check(key, path):
                sys.exit(f"{path} does not match the manifest")
            return path
        for attempt in range(2):
            if not path.is_file():
                fetch(urls, path)
            if self.manifest.check(key, path):
                return path
            if attempt == 0:
                print(f"discarding cached {path} and fetching again", file=sys.stderr)
                path.unlink()
        sys.exit(f"{path} does not match the manifest even when freshly fetched;"
                 " if the text itself changed, review it and run with"
                 " --update-manifest")

    def qt(self, identifier):
        version = self.args.qt_version
        key = f"LICENSES/{identifier}.txt"
        if self.args.qt_source:
            if identifier not in self.offline:
                sys.exit(f"no LICENSES/{identifier}.txt in the given Qt sources")
            return self.verified(key, self.offline[identifier], None)
        urls = [pattern.format(module=module, id=identifier, version=version)
                for pattern in QT_TEXT_URLS for module in self.modules]
        # Cached per release even though the manifest is not: the file is what
        # that release's tag served, and a text that fails the check is
        # discarded and fetched again from the same place.
        return self.verified(key, self.args.cache_dir / f"qt-{version}" / key, urls)

    def icu(self):
        version = self.args.icu_version
        key = f"icu-{version}/LICENSE"
        if self.args.icu_license:
            return self.verified(key, self.args.icu_license, None)
        major, minor = version.split(".", 1)
        url = ICU_LICENSE_URL.format(major=major, minor=minor)
        return self.verified(key, self.args.cache_dir / key, [url])


def wrap(text, indent=""):
    return textwrap.fill(" ".join(text.split()), width=78, break_on_hyphens=False,
                         subsequent_indent=indent)


def qt_notice(args, module_roots, icu, ijg):
    """The user-facing notice: which Qt, under what licence, where its source is.

    GPLv3 section 6(d) is the clause that applies to a download: directions
    next to the object code plus continued availability of the source.
    """
    version = args.qt_version
    series = version.rsplit(".", 1)[0]
    paragraphs = [
        "# Qt in this bundle",
        f"""This application uses the Qt toolkit, version {version}, under the
            GNU Lesser General Public License version 3. Qt is not part of
            easy_profiler and is not covered by easy_profiler's own MIT or
            Apache-2.0 licence.""",
        f"""The Qt libraries and plugins in this bundle are the open-source
            binaries published by The Qt Company for Qt {version};
            `THIRD-PARTY-NOTICES.md` names each one under "Shipped in". Qt is
            linked dynamically: you may replace any of them with another build
            of the same Qt minor version and run the result.""",
    ]
    if icu:
        paragraphs.append(f"""The bundle also carries ICU {args.icu_version},
            which the Qt packages bundle and Qt Core links against. It is
            covered by `LICENSE.ICU`.""")
    paragraphs += [
        "## Corresponding source",
        f"""The complete corresponding source of this Qt is published by The Qt
            Company at <https://download.qt.io/archive/qt/{series}/{version}/submodules/>,
            and will remain available there for as long as that archive is
            maintained. The modules bundled here are:""",
        "\n".join(f"    {module}-everywhere-src-{version}.tar.xz"
                  for module, _ in module_roots),
        """`THIRD-PARTY-NOTICES.md` records the module revisions the bundled
            binaries were built from, as far as Qt's own SBOM states them, and
            the configure options Qt built with.""",
        "## Licence texts",
        "\n".join(wrap(f"- {item}", indent="  ") for item in
                  ["`LICENSE.LGPLv3` - the GNU Lesser General Public License"
                   " v3, the licence this Qt is used under",
                   "`LICENSE.GPLv3` - the GNU General Public License v3, which"
                   " the LGPL refers to"]
                  + (["`LICENSE.ICU` - the Unicode licence, covering the"
                      " bundled ICU"] if icu else [])
                  + ["`THIRD-PARTY-NOTICES.md` - the attributions for the"
                     " third-party code the Qt libraries in this bundle embed"]),
    ]
    if ijg:
        paragraphs.append("""This software is based in part on the work of the
            Independent JPEG Group. Condition 2 of the IJG licence asks that
            sentence of anyone distributing executables built on its code, and
            a Qt library in this bundle embeds libjpeg.""")
    return "\n\n".join(p if p.startswith(("#", "-", "    ")) else wrap(p)
                       for p in paragraphs) + "\n"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--bundle", required=True, type=pathlib.Path,
                        help="the assembled bundle whose libraries are attributed")
    parser.add_argument("--qt-install", required=True, type=pathlib.Path)
    parser.add_argument("--qt-version", required=True)
    parser.add_argument("--icu-version",
                        help="the ICU release the bundle carries, e.g. 73.2;"
                             " required exactly when the bundle carries ICU")
    parser.add_argument("--out", required=True, type=pathlib.Path)
    parser.add_argument("--cache-dir", type=pathlib.Path,
                        default=pathlib.Path.home() / ".cache/easy_profiler/qt-notices",
                        help="where fetched licence texts are kept")
    parser.add_argument("--qt-source", action="append", default=[],
                        type=pathlib.Path,
                        help="unpacked qt*-everywhere-src tree to take the"
                             " licence texts from instead of fetching")
    parser.add_argument("--icu-license", type=pathlib.Path,
                        help="ICU's LICENSE file, instead of fetching it")
    parser.add_argument("--manifest", type=pathlib.Path, default=MANIFEST)
    parser.add_argument("--update-manifest", action="store_true",
                        help="record the checksums of the texts used instead"
                             " of checking them")
    args = parser.parse_args()
    if args.icu_version and not re.fullmatch(r"\d+\.\d+", args.icu_version):
        sys.exit(f"--icu-version must be major.minor, got {args.icu_version!r}")

    targets, icu_libraries = scan_bundle(
        args.bundle, shipped_index(args.qt_install, args.qt_version))
    if icu_libraries and not args.icu_version:
        sys.exit("the bundle carries ICU and Qt's SBOM states no licence for"
                 " it, so --icu-version is required:\n  "
                 + "\n  ".join(icu_libraries))
    if args.icu_version and not icu_libraries:
        sys.exit(f"--icu-version {args.icu_version} was given but {args.bundle}"
                 " carries no ICU; the notices would attribute a library that"
                 " is not there")

    modules = module_order({module for module, _, _ in targets.values()})
    manifest = Manifest(args.manifest, args.update_manifest)
    texts = Texts(args, manifest, modules)

    components = {}
    extracted = {}
    sbom_paths = []
    module_roots = []
    build_comment = None
    for module in modules:
        path = next(p for m, p, _ in targets.values() if m == module)
        sbom = load_sbom(path)
        sbom_paths.append(path)
        for info in sbom["hasExtractedLicensingInfos"]:
            extracted.setdefault(info["licenseId"], info["extractedText"])
        root = next(p for p in sbom["packages"] if p["name"] == module)
        module_roots.append((module, root))
        if build_comment is None:
            build_comment = root.get("comment", "").strip()
        for package, (owner, _, shipped_as) in targets.items():
            if owner != module:
                continue
            for name, third_party in third_party_of(sbom, package).items():
                entry = components.setdefault(name, {
                    "package": third_party,
                    "module": module,
                    "shipped_in": set(),
                })
                entry["shipped_in"].update(shipped_as)

    wanted = set(QT_LICENCE_TEXTS)
    for entry in components.values():
        expression = entry["package"].get("licenseConcluded", "")
        wanted.update(spdx_ids(expression))

    # The SBOM embeds only the texts of licences Qt defines itself; a standard
    # id is a reference, and its text has to come from Qt's LICENSES/.
    texts_of = {}
    for identifier in sorted(wanted, key=str.lower):
        if identifier in extracted:
            texts_of[identifier] = (extracted[identifier],
                                    f"Qt {args.qt_version} SBOM"
                                    " hasExtractedLicensingInfos")
        else:
            path = texts.qt(identifier)
            texts_of[identifier] = (path.read_text(encoding="utf-8"),
                                    f"Qt {args.qt_version} LICENSES/{identifier}.txt")
    icu_license = texts.icu() if icu_libraries else None
    if args.update_manifest:
        manifest.save()
        for key in manifest.recorded:
            print(f"recorded {key}")

    args.out.mkdir(parents=True, exist_ok=True)

    for identifier, filename in (("LGPL-3.0-only", "LICENSE.LGPLv3"),
                                 ("GPL-3.0-only", "LICENSE.GPLv3")):
        (args.out / filename).write_text(texts_of[identifier][0], encoding="utf-8")
    if icu_license is not None:
        (args.out / "LICENSE.ICU").write_text(
            icu_license.read_text(encoding="utf-8"), encoding="utf-8")

    (args.out / "QT_NOTICE.md").write_text(
        qt_notice(args, module_roots, bool(icu_libraries), "IJG" in wanted),
        encoding="utf-8")

    lines = []
    emit = lines.append

    def para(text):
        emit(wrap(text))
        emit("")

    emit("# Third-party notices for the Qt libraries in this bundle")
    emit("")
    emit(f"Qt version: {args.qt_version}")
    if icu_libraries:
        emit(f"ICU version: {args.icu_version}")
    emit("")
    para("""This file is generated by `scripts/qt_notices.py` when the bundle
        is assembled, from the Qt install the bundle was built against; it is
        not edited by hand and not kept in easy_profiler's repository.""")
    emit("## Where this came from")
    emit("")
    para(f"""The component list below is Qt's own SPDX SBOM for the official
        binary package of Qt {args.qt_version}, which every official Qt
        install carries under `sbom/`:""")
    for path in sbom_paths:
        emit(f"- `sbom/{path.name}`")
    emit("")
    para("""For each Qt library and plugin the bundle ships, the entries are
        the third-party packages that SBOM records as dependencies of that
        target, with the version, copyright and SPDX licence expression it
        states. The SBOM lists a target's attributions as declared in Qt's
        sources, so a few cover code paths that are not compiled on this
        platform; they are kept rather than filtered, because over-reporting
        an attribution is harmless and guessing which ones the compiler
        dropped would not be.""")
    para(f"""The licence texts in the last section are the verbatim
        `LICENSES/` files of the Qt {args.qt_version} release, taken from the
        `v{args.qt_version}` tag of Qt's module repositories and checked
        against the checksums easy_profiler records in
        `scripts/qt_notices.sha256`, or the `hasExtractedLicensingInfos` text
        embedded in the SBOM for the `LicenseRef-` identifiers Qt defines
        itself.""")
    if icu_libraries:
        para("""`LICENSE.ICU` is ICU's own LICENSE file, which the SBOM does not
            carry: it records ICU as a system dependency with no licence stated,
            yet the official Linux Qt packages put ICU in `lib/`, so the bundle
            redistributes it.""")
    if build_comment:
        para("Qt's SBOM describes the bundled binaries as built this way:")
        for line in build_comment.splitlines():
            emit(f"    {line}")
        emit("")

    emit("## Qt itself")
    emit("")
    para(f"""This bundle uses the Qt toolkit, version {args.qt_version}, under
        the GNU Lesser General Public License version 3 (`LICENSE.LGPLv3`,
        which incorporates `LICENSE.GPLv3` by reference). Qt is not part of
        easy_profiler and is not covered by easy_profiler's MIT or
        Apache-2.0 licence.""")
    para("""The bundled binaries were built from these Qt modules, at the
        revisions Qt's SBOM states. This is the corresponding source the
        LGPL asks for, identified exactly rather than by version alone:""")
    for module, root in module_roots:
        version = clean(root.get("versionInfo")) or args.qt_version
        emit(f"- `{module}` {version}")
        download = clean(root.get("downloadLocation"))
        if download:
            emit(f"  - {download}")
        emit(f"  - release tarball: "
             f"`{module}-everywhere-src-{args.qt_version}.tar.xz`")
    emit("")
    if icu_libraries:
        emit("## ICU")
        emit("")
        para(f"""The bundle carries ICU {args.icu_version} (`libicuuc`,
            `libicui18n`, `libicudata`), taken from the official Qt package,
            which Qt Core and Qt Core5Compat link against. ICU is licensed under
            the Unicode licence; see `LICENSE.ICU` for the full text, which also
            carries ICU's own third-party notices.""")
    emit("## Third-party code inside the Qt libraries shipped here")
    emit("")

    def sort_key(item):
        fields = describe(components[item]["package"])
        return (fields.get("Name") or item).lower()

    for name in sorted(components, key=sort_key):
        entry = components[name]
        package = entry["package"]
        fields = describe(package)
        emit(f"### {fields.get('Name') or name}")
        emit("")
        version = clean(package.get("versionInfo"))
        if version:
            emit(f"- Version: {version}")
        expression = clean(package.get("licenseConcluded")) or "not stated"
        emit(f"- Licence: {expression}")
        if fields.get("License"):
            emit(f"- Licence name as Qt states it: {fields['License']}")
        if fields.get("Qt usage"):
            emit(f"- How Qt uses it: {fields['Qt usage']}")
        download = clean(package.get("downloadLocation"))
        if download:
            emit(f"- Upstream: {download}")
        emit(f"- Shipped in: {', '.join(sorted(entry['shipped_in']))}")
        emit(f"- SBOM package: `{name}`"
             + (f", from `{fields['source']}`" if fields.get("source") else ""))
        copyright_text = package.get("copyrightText")
        if copyright_text and copyright_text != "NOASSERTION":
            emit("- Copyright:")
            emit("")
            for line in copyright_text.strip().splitlines():
                if line.strip():
                    emit(f"      {line.strip()}")
        emit("")

    emit("## Licence texts")
    emit("")
    for identifier in sorted(wanted, key=str.lower):
        body, origin = texts_of[identifier]
        emit(f"### {identifier}")
        emit("")
        emit(f"Source: {origin}")
        emit("")
        for line in body.rstrip("\n").splitlines():
            emit(f"    {line}" if line.strip() else "")
        emit("")

    notices = args.out / "THIRD-PARTY-NOTICES.md"
    notices.write_text("\n".join(lines).rstrip("\n") + "\n", encoding="utf-8")

    print(f"{len(targets)} Qt targets in {args.bundle}, "
          f"{len(components)} third-party components, "
          f"{len(wanted)} licence texts")
    print(f"wrote {notices}")
    for path in sbom_paths:
        print(f"  from {path}  sha256 {sha256(path)}")
    if icu_license is not None:
        print(f"  from {icu_license}  sha256 {sha256(icu_license)}")


if __name__ == "__main__":
    main()
