# Releasing easy_profiler

The release pipeline lives in `.github/workflows/release.yml`. This file is the
part that cannot be automated: the order of the steps, the decisions that belong
to a person, and the reasoning behind the pipeline's shape.

## What a release contains

| Asset | Who it is for |
| --- | --- |
| `easy_profiler-<v>-src.tar.gz`, `.zip` | Packagers. `git archive` output, so the bytes are stable and the untracked working tree cannot leak in. Conan Center and the Bazel Central Registry need exactly this |
| `easy_profiler-<v>-linux-x86_64-{shared,static}.tar.gz` | Linux consumers. glibc floor 2.35 |
| `easy_profiler-<v>-macos-universal-{shared,static}.tar.gz` | macOS consumers, arm64 and x86_64, deployment target 11.0 |
| `easy_profiler-<v>-windows-x64-{shared,static}.zip` | Windows consumers. The shared zip carries the PDB |
| `easy_profiler_gui-<v>-linux-x86_64.tar.gz` | The GUI, self-contained except for X11, OpenGL and glibc. glibc floor 2.35, the same as the SDK packages |
| `easy_profiler_gui-<v>-windows-x86_64.zip` | The GUI, self-contained except for the MSVC runtime. Has not yet been through CI; see below |
| `SHA256SUMS` | Everyone |

This table and the `ASSETS` list in the `checksums` job of the workflow name
the same files, and the job publishes exactly that list: a listed file that
was not built fails the run, and anything a job uploaded that is not listed
stays out of the release, with a warning. Nothing is swept up by pattern.

### Releasing without a product

A red cell on the publish chain means no draft. To ship without a product -
the Windows GUI is the likely case - make two edits in the `checksums` job,
together: delete the product's line from `ASSETS`, and its build and verify
jobs from `needs`. The first is what leaves the product out; the second is what
lets the job run while those cells are red. Deleting only the jobs from
`needs` would have the job fail on the missing asset if the build was red, or
publish a bundle nobody verified if only the verification was. Update the
table above and the release notes to match.

Each SDK package holds `include/`, `lib/` with the CMake package config,
`bin/profiler_converter` and `bin/profiler_reader`, both licences, `README.md`,
`CHANGELOG.md` and `BUILD-INFO.txt`, which records the toolchain that produced
it. A prebuilt C++ binary is only usable against a compatible toolchain, so the
package says which one it was.

Windows packages deliberately do **not** carry the MSVC runtime
(`CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP` in `easy_profiler_core/CMakeLists.txt`).
Consumers install the official Visual C++ Redistributable, which is the
redistribution route Microsoft supports.

## The GUI

The pipeline builds and verifies two GUI bundles, Linux x86_64 and Windows
x64, and both are on the publish chain. The Linux one has the evidence: the
GUI builds unmodified against every Qt 6 tried, from Ubuntu 24.04's 6.4.2 to
the official 6.12.0, loads a six-million block capture, and runs from a
self-contained bundle on a machine with no Qt installed. The Windows one has
never run through CI; whether it ships is decided at the rehearsal, step 7
below, and leaving it out is the edit described under "Releasing without a
product".

### What still blocks the Windows GUI

The packaging exists: `gui-windows` and `gui-verify-windows` in the release
workflow, and `scripts/make_gui_bundle.ps1`, which does for `windeployqt` what
`make_gui_bundle.sh` does for `ldd`. None of it has run yet.

Both bundle scripts create their output directory and refuse one that already
exists, so a mistyped destination cannot cost a tree; the workflow stages into
a fresh `stage/` on every run. Both hand the licence work to
`scripts/qt_notices.py`, which writes every file under `licenses/` except
easy_profiler's own two, and `tests/test_qt_notices.py` holds the generator to
that contract; the bundle scripts only act on its exit status.

What stood in front of it was not our code. From 6.11 onwards Qt publishes its
Windows packages under a per-compiler folder,
`windows_x86/desktop/qt6_6120/qt6_6120_msvc2022_64/`, where Linux and macOS
still use a flat `qt6_6120/qt6_6120/`. aqtinstall 3.3.0, the newest release,
only reads the flat path for a desktop Windows architecture, so asking it for
6.11 or newer fails with `Failed to locate XML data` before a compiler runs.
Verified against download.qt.io: 6.10.3 and earlier are flat, 6.11.0 through
6.12.0 are split.

The Windows jobs sidestep this with `scripts/aqt_windows_workaround.py`, which
walks back from the newest Qt 6 to the newest whose package index aqtinstall
can read, 6.10.3 today. It is a workaround for aqtinstall's bug and nothing
more: the day an aqtinstall that reads the split layout reaches PyPI, delete
the script and give the Windows jobs the same `aqt list-qt ... --latest-version`
one-liner the Linux job uses. The Windows bundle lags the Linux one until then;
each bundle's `licenses/QT_NOTICE.md` names its own Qt. The notices
generator has been run against the official 6.10.3 Windows package on a bundle
shaped the way windeployqt produces one, and attributes it. That needed one
manifest entry Linux never asks for: windeployqt deploys every plugin in
`imageformats/`, so a Windows bundle carries `qjpeg.dll` and therefore
LibJPEG-turbo, whose licence is `IJG AND BSD-3-Clause`.

`checksums` depends on `gui-windows` and `gui-verify-windows`, so a red
Windows cell means a tag cannot produce a draft; "Releasing without a product"
above says how to leave the bundle out.

`scripts/aqt_windows_workaround.py` is covered by
`tests/test_aqt_windows_workaround.py`: offline against a stand-in `aqt` that
answers with aqtinstall's own messages, and, with `EASY_PROFILER_ONLINE_TESTS=1`
and aqtinstall importable, against the real aqtinstall and Qt's servers with a
TLS failure injected by `tests/support/aqt_fault`. The online tests are the
proof that the stand-in's messages are the real ones; run them when aqtinstall
is upgraded.

These are the answers still owed, none of which can be had without a green
run:

- whether the GUI compiles with MSVC at all. Nothing has ever tried.
- whether `windeployqt` deploys the SVG image-format and icon-engine plugins.
  Qt's deployment documentation states no default plugin set and no rule for
  choosing one. The bundle script asserts both are present and fails if they
  are not; the fix is `--include-plugins qsvg,qsvgicon`, not deleting the check.
  All 57 toolbar icons are SVG resources, and without those plugins the toolbars
  render empty with no diagnostic of any kind, which is also why
  `gui-verify-windows` runs `--self-test`: it counts rendered icons.
- whether the bundle starts without the MSVC runtime. It is built with
  `--no-compiler-runtime`, because Qt's own documentation says the runtime
  windeployqt falls back to on a developer machine is "not intended or licensed
  for redistribution", and because the notices generator cannot attribute it.
  `README.txt` sends the user to Microsoft's redistributable. Both GitHub
  Windows images have it installed, so a green `gui-verify-windows` does *not*
  prove the bundle runs on a machine that does not. Shipping the official
  redistributable instead is a licensing decision, not a technical one.
- what `Compress-Archive` and `Expand-Archive` do with the bundle directory.
  Expected to behave like `tar -C stage` with the directory as the archive root.

### Which Qt

**No Qt version is pinned anywhere.** The GUI uses nothing exotic - it has
compiled unmodified against 6.4.2, 6.8.3, 6.9.3 and 6.12.0, needing one
four-line fix in all that span, when 6.9 removed `QChar(signed char)` - and
`profiler_gui/CMakeLists.txt` asks for `Qt6` with no version. The pipeline
follows suit.

The bundles are built against **whatever Qt 6 release is current on the day**,
which aqtinstall reads off Qt's own repository
(`aqt list-qt <host> desktop --spec 6 --latest-version`), from the
**official open-source packages**. Official rather than a distribution's
because only the official packages carry the SPDX SBOM the notices below are
generated from: `apt install qt6-base-dev` on Ubuntu 24.04 yields no
`*.spdx.json` at all, and Homebrew's Qt none either. That is a constraint on
the *source* of the Qt a bundle redistributes, not on its version. The exact
version a bundle carries is in its `licenses/QT_NOTICE.md`.

Two things follow from not pinning:

- **A new Qt can break a release build on the day it appears.** The rehearsal
  is the mitigation: run it before tagging, and if a cell is red because Qt
  moved, fix the code - every Qt break so far was a one-off API removal - or,
  as a last resort for that one release, hand `aqt install-qt` a version in the
  two bundle jobs. That is a decision to make on the day, with the failure in
  front of you, not something to write into the pipeline in advance.
- **The platforms need not carry the same Qt.** Windows resolves to 6.10.3
  today while Linux resolves to 6.12.0, because aqtinstall cannot yet read Qt's
  newer Windows repository layout and the Windows jobs go through the
  workaround script (above). Two bundles, two `QT_NOTICE.md` files, each
  naming its own.

macOS has a **floor** of Qt 6.9.2, for people building from source: earlier
Qt 6 releases link `-framework AGL`, which Apple removed from the macOS 26 SDK
([QTBUG-137687]). The fix is also in Qt 6.8.4, whose binaries are
commercial-only, so the 6.8 line is unusable on macOS at all. That floor is the
project's only Qt version constraint, and `README.md` states it.

`gui-build-check` asks a different question from the bundle jobs - "does the
GUI compile here, against the Qt a person on this platform would have" - so it
uses the platform's own Qt where there is one: Ubuntu's `qt6-*-dev` packages
on Linux, which also keeps the oldest Qt 6 anything builds against in view, and
`brew install qtbase qtsvg qt5compat` on macOS. Windows has no system Qt, so
that cell resolves one through the same workaround script as the bundle job.

[QTBUG-137687]: https://bugreports.qt.io/browse/QTBUG-137687

### Qt's LGPL obligations

The bundle links Qt dynamically, which is what the LGPL requires of a work that
uses the library. The bundle must carry:

- Qt's `LICENSE.LGPLv3` and `LICENSE.GPLv3`;
- the third-party attributions, because the Qt libraries embed pcre2, harfbuzz,
  libpng and some thirty smaller components, each with its own notice
  requirement, and the official Qt packages put ICU in `lib/` as well;
- `QT_NOTICE.md` naming the exact Qt version and package source, with a link to
  the corresponding source. GPLv3 section 6(d) is the operative clause for a
  download: directions next to the object code plus continued availability of
  the source. The written offer of section 6(b) applies to physical products and
  does no work here.

None of this lives in the repository. easy_profiler is MIT and Apache-2.0, the
LGPL's obligations attach to the bundle that carries Qt, and GPL texts sitting
in the tree would make the project look like something it is not. The
attributions are generated when the bundle is assembled:

- Neither Homebrew's Qt nor the official Qt installers ship any attribution
  document. The distribution's `/usr/share/doc/qt6-base-dev/copyright` was the
  only source that ever had one, and copying it into a bundle built from
  official Qt would attach a document describing a different build.
- Official Qt installs do, however, carry an SPDX SBOM under `sbom/` for every
  module. The bundle script runs `scripts/qt_notices.py` against the SBOM of
  the Qt it is bundling, so the component list and copyrights describe the exact
  binaries in the bundle and cannot go stale on a Qt bump. The SBOM
  embeds the texts of the licences Qt defines itself; the standard ones
  (LGPL-3.0, GPL-3.0, BSD, MIT and so on) it only names, so their `LICENSES/`
  files are fetched from the `v<version>` tag of Qt's module repositories on
  code.qt.io, with GitHub's mirror as the fallback, and ICU's `LICENSE` from
  the ICU release tag. Nineteen files for a Linux bundle, about 180 KB, some
  eight seconds. The binary Qt packages ship no `LICENSES/` directory, so
  there is no offline source for these inside the install itself. A Windows
  bundle asks for `MIT-open-group`, `Zlib` and `IJG` on top: it carries bundled
  freetype and zlib where Linux links the system's, and windeployqt deploys the
  whole `imageformats/` directory, JPEG included.
- Which libraries get attributed is read off the finished bundle, not from a
  list in the generator: every shared library in it is matched against the
  `files` array of the Qt SBOMs, which ties each installed path to the CMake
  target it was built from, and the target's third-party packages are what the
  notices report. A library the bundle script starts copying is therefore
  attributed the moment it appears, and one that matches nothing is a hard
  error rather than a silent omission - the same rule for ICU, which the SBOM
  records as a system dependency with no licence, so `--icu-version` is
  required exactly when ICU is in the bundle and refused when it is not. This
  is also what makes the generator work on all three platforms from one code
  path: the SBOM names `lib/libQt6Core.so.<version>` on Linux,
  `bin/Qt6Core.dll` on Windows and the `QtCore` binary inside a framework on
  macOS, and all three resolve to the same `Core` target.
- Every fetched text is checked against `scripts/qt_notices.sha256`, so a
  release is built from known content: a `v<version>` tag is a git tag and
  can move, and a cache can be fed, but neither can put a text into a bundle
  that nobody has reviewed. The manifest is keyed by **text**, not by Qt
  release - `LICENSES/LGPL-3.0-only.txt`, not `qt-6.12.0/...` - because a
  standard licence text is the same file in every Qt release that refers to
  it: all twenty texts are byte-identical across the 6.9.3, 6.10.3 and 6.12.0
  tags. So a new Qt needs no manifest change at all unless it refers to a
  licence no earlier release did, or Qt changed the bytes of a text; either
  fails the build and names the file, and that is the one moment a person
  should be looking at it. A key may carry several digests, each a reviewed
  version of that text, so recording a changed text does not stop an older
  Qt from building. ICU's `LICENSE` is keyed by ICU release, since that file
  genuinely differs between releases.
- The release job caches the fetched texts under the manifest's hash and the
  resolved Qt version. Do not count on that cache: Actions evicts an entry
  after seven days, and a tag run cannot read a cache another tag wrote, so
  it helps only when a `workflow_dispatch` rehearsal ran on the default branch
  within the preceding week - which is the sequence below, but by arrangement
  rather than by guarantee. Assume a real release fetches. The script fails if
  a text is missing from the manifest, does not match it, or cannot be
  fetched, and the bundle script refuses to continue - no bundle ships with
  missing or unreviewed attributions.

When the generator does stop on a text, review it and record it: run the
generator with `--update-manifest`, which adds the new digest and removes
nothing, read the diff of the manifest, and commit it. To regenerate without
the network, point `--qt-source` at unpacked
`qt{base,svg,5compat}-everywhere-src` tarballs from
<https://download.qt.io/archive/qt/> and `--icu-license` at ICU's `LICENSE`;
the texts are still held to the manifest. The script's header spells out the
invocation.

### Where the GUI bundle is built

In the `ubuntu:22.04` container, like the SDK packages, so the whole release has
one glibc floor of 2.35 rather than 2.35 for the libraries and 2.39 for the GUI.
A GUI that will not start on a machine whose SDK package runs there would be a
poor thing to explain, and Ubuntu 22.04 is exactly the sort of box a profiler GUI
gets copied onto.

The cost is that the container needs a CMake newer than the 3.22.1 that 22.04
ships, because current Qt requires 3.25 or later, and that Qt comes from
`aqtinstall` by hand rather than from `install-qt-action`, which expects a runner
rather than a bare container. Qt's
`-DQT_FORCE_MIN_CMAKE_VERSION_FOR_USING_QT` would avoid the CMake upgrade; Qt
documents it as unsupported, so the pipeline installs CMake from pip instead, at
a pinned version. CMake 4.x also works - configure and build are clean on
4.4.4, because the root `CMakeLists.txt` uses the range form of
`cmake_minimum_required` - so moving the pin is a decision, not a repair.

`install-qt-action` is not used anywhere, also on the runners: on
`windows-2025` it reported "Action not built" in both v4.4.1 and v4.5.0, and
the build check exists to test this project's code, not a third party's
packaging. aqtinstall is the one route to Qt in the pipeline and in this file.

Replacing the bundled Qt libraries in place works within the same Qt minor
version. Say that rather than implying any Qt will do.

## Release day

Do these in order. Steps 1 to 4 are a single commit.

1. `VERSION` holds the version being released. Confirm it is what you intend.
2. `CHANGELOG.md`: change `## <version> — Unreleased` to
   `## <version> — YYYY-MM-DD`.
3. `README.md`: move the `stable-` badge to this version. The `development-`
   badge already tracks `VERSION` and is checked on every commit.
4. Run `ctest --preset dev -R version_consistency` and
   `python3 -m unittest tests/test_changelog_section.py`.
5. Commit and push. Let CI go green.
6. **Run the rehearsal.** Actions → Release → Run workflow, on the branch you
   are about to tag. This builds and verifies every artifact and touches no
   release. It is the only place a red cell is cheap: after the tag exists, a
   fix costs a new version number.
7. Read the rehearsal's job list. Every `sdk`, `sdk-verify` and `gui` cell is on
   the publish chain, so any red cell means no draft release. Decide here
   whether a platform ships, not after tagging; "Releasing without a product"
   is the edit.
8. Tag and push: `git tag -a v<version> -m "easy_profiler <version>"`, then
   `git push origin v<version>`.
9. The workflow builds a **draft** release. Download an asset or two, unpack
   one, run `bin/profiler_converter` on a profile.
10. Press Publish.

### Never move a tag

A published version number is spent. If something is wrong after the tag exists,
release the next patch version. Re-running the workflow on the same tag is safe:
the draft is deleted and recreated, and the workflow refuses to touch a release
that has already been published.

Two repository settings make this a rule the platform enforces rather than a
sentence in a file. Both are free and worth enabling:

- a tag ruleset on `v*` that blocks deletion and non-fast-forward updates, so a
  tag cannot move before publication;
- Immutable Releases, so a published release locks its assets and its tag.

Without them, a force-pushed tag re-triggers the workflow and recreates the
draft, and re-running an old job after a tag move would attach artifacts built
from the old commit. The workflow checks the tag *name* against `VERSION`; it
cannot see that the tag now points somewhere else.

### The monthly rehearsal is a courtesy, not a guard

`release.yml` runs on a cron so that an upstream action bump or a retired runner
image breaks on the first of a month instead of on release day. GitHub disables
scheduled workflows in a public repository after 60 days without repository
activity, and notifies only whoever committed the workflow file. For a project
that goes quiet between releases, that is exactly the window the cron was meant
to cover. Step 6 is the guard.

## Version consistency

`VERSION` is the only place the product version is written. It is the version
under development, which between releases is ahead of the last released one.

Derived from it: the CMake project version and the library's compiled
`apiVersion*()` values.

Checked against it on **every commit**, by `easy_profiler.version_consistency`
and `tests/test_version.cpp`:

- `MODULE.bazel` module version and `version.bzl` definitions, neither of which
  can read a file;
- a `## <version>` section in `CHANGELOG.md`;
- the `development-` badge in `README.md`, in both the badge text and its URL;
- no build file defining `EASY_PROFILER_FORMAT_VERSION_*`, which would let a
  release change the on-disk format silently. That version belongs only in
  `easy_profiler_core/profiler.cpp` and is pinned to 2.1.0 by a test;
- the compiled library's own `apiVersion*()` against the file on disk.

Checked **only when a tag is built**, by the `version-gate` job:

- the tag name equals `v<VERSION>`;
- the changelog section is dated rather than marked Unreleased;
- the `stable-` badge in `README.md` equals `VERSION`.

Those three are release-day facts. Enforcing them on every commit would make the
repository permanently red between releases, because the stable badge correctly
names the previous release and the changelog section is correctly Unreleased.

Note the limit of the mechanism: the Bazel and README checks are regular
expressions over text. They catch forgetfulness, not a determined rewrite. The
case where wrong bytes actually ship is covered by `tests/test_version.cpp`,
which compares the compiled binary against `VERSION` itself.

## How artifacts are verified

Not "the tests passed in CI" — the file that will be downloaded is shown to
work, on a runner that did not build it.

`ctest -L package` with `-DEASY_PROFILER_VERIFY_PACKAGE=<unpacked prefix>`
runs `easy_profiler.packaged_consumer`, which

1. checks the package contains everything a consumer needs;
2. configures and builds `tests/consumer` against the package, which asserts
   that the CMake package version and the library binary agree;
3. runs the package's **own** `bin/profiler_converter` and `bin/profiler_reader`
   from the unpacked tree on a profile the consumer just wrote.

Step 3 is the one that matters and must never be trimmed as optional. Step 2
alone is not a package test: the consumer executable carries an absolute rpath
to the package's `lib/`, so it links and runs even when the shipped tools are
broken. That is exactly how a wrong install rpath survived in this repository
while `easy_profiler.install_consumer` stayed green.

Static packages are not affected by rpath, so for them step 2 is the real check
and a broken package shows up there as a link error.

## macOS: quarantine

Download a macOS tarball with `curl` and unpack it with `tar` in a terminal:

```
curl -LO https://github.com/yse/easy_profiler/releases/download/v<v>/easy_profiler-<v>-macos-universal-shared.tar.gz
tar -xzf easy_profiler-<v>-macos-universal-shared.tar.gz
```

A tarball downloaded in a browser and unpacked by double-clicking it carries
`com.apple.quarantine` on every file inside. Gatekeeper then refuses the
command-line tools and the library, with no useful message, and ad-hoc signing
does not change that. `curl` attaches no quarantine attribute and `tar` does not
propagate one, so the terminal route avoids the problem entirely. This applies
to the library and tools, not only to applications — the release notes must say
so.

## macOS: ad-hoc signatures

The release job re-signs every Mach-O in a macOS package with
`codesign --force --sign -` and then verifies with `codesign --verify --strict`.

This is not code signing in the Developer ID sense: it needs no certificate, no
Apple ID and no paid account. It is required because the linker ad-hoc signs
only the native slice of a universal build. On an Apple Silicon machine the
arm64 slice comes out `adhoc,linker-signed` and valid, while the x86_64 slice is
not signed at all, so `codesign --verify --strict` on the fat file fails. Verify
before you re-sign and the macOS cells go red.

## Signing and notarization

Out of scope, deliberately. The pipeline uses **no secrets at all**, so a fork
that pushes a tag gets the whole pipeline, its own draft release and its own
valid attestations.

Binaries are therefore unsigned, and the release notes say so and give the
SHA-256. On Windows an unsigned executable raises SmartScreen's "Windows
protected your PC"; More info, then Run anyway. Smart App Control, where it is
enforcing, blocks unsigned executables outright with no Run anyway, and the only
way through is to turn it off. The release notes name both so the question is
answered before it is asked.

## Integrity

- `SHA256SUMS` guards a truncated or swapped download, not an attacker who can
  replace both files. Say that.
- Artifact attestations (`actions/attest`) are signed with a short-lived
  Sigstore certificate and need no secret and no paid account, which is exactly
  why they are worth having when signing is not. `gh attestation verify <file>
  --repo yse/easy_profiler` proves the file came out of a workflow run in this
  repository at a given commit. It requires the user to be logged in to GitHub,
  and almost nobody will run it. Do not oversell it.
- No SBOM. The core has no third-party dependencies; the GUI bundle's Qt
  inventory is covered by the attribution files it carries.

## Downstream distribution

The only channels this project maintains are the GitHub release assets.

- **Conan Center** already carries `easy_profiler/2.1.0`, maintained by Conan's
  contributors rather than by us. Open an issue there pointing at the release
  and at the 3.0 breaking changes; do not adopt the recipe.
- **vcpkg** has no port. The stale request is `microsoft/vcpkg#46679`. A port is
  small but means a version-database pull request every release.
- **Bazel Central Registry** is the cheapest of these, because `MODULE.bazel`,
  the stable source tarball and a green `//tests:bazel_smoke` already exist.
  Worth doing one release after 3.0.0, once the archive URL scheme has proven
  stable, since a registry is append-only.
- **Homebrew and Linux distribution packaging** are out of scope. A personal tap
  is the low-commitment version if the GUI ever wants one.
