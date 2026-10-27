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
| `SHA256SUMS` | Everyone |

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

3.0.0 ships a Linux GUI bundle and no other. The reason is evidence: the GUI
builds unmodified against Qt 6.12.0, loads a six-million block capture, and runs
from a self-contained bundle on a machine with no Qt installed. The Windows GUI
has never been compiled; it ships when the rehearsal below is green for it, and
not before.

### One Qt version everywhere

Every platform uses **Qt 6.12.0**, from the official open-source packages. One
version means one CVE to track rather than three, and a GUI that behaves the
same wherever it was built.

The Linux bundle used to take Qt from the distribution, on the argument that
Ubuntu backports security fixes for the life of the LTS while an open-source
Qt 6.12.x stops receiving patches once 6.13 ships. That argument is dead, and
not because it was wrong: it cannot be applied on macOS or Windows, where no
distribution Qt exists, so following it meant three different Qt versions and
three sets of behaviour to reason about. A single version we bump deliberately
beats a longer patch runway on one platform out of three.

macOS has a **floor** of Qt 6.9.2 rather than a pin, for people building from
source: earlier Qt 6 releases link `-framework AGL`, which Apple removed from
the macOS 26 SDK ([QTBUG-137687]). The fix is also in Qt 6.8.4, whose binaries
are commercial-only, so the 6.8 line is unusable on macOS at all. Keep that
floor documented in `README.md` even while CI pins 6.12.0 - the floor tells a
contributor what will work, the pin says what we ship.

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
  module. `scripts/make_gui_bundle.sh` runs `scripts/qt_notices.py` against the
  SBOM of the Qt it is bundling, so the component list and copyrights describe
  the exact binaries in the bundle and cannot go stale on a Qt bump. The SBOM
  embeds the texts of the licences Qt defines itself; the standard ones
  (LGPL-3.0, GPL-3.0, BSD, MIT and so on) it only names, so their `LICENSES/`
  files are fetched from the `v<version>` tag of Qt's module repositories on
  code.qt.io, with GitHub's mirror as the fallback, and ICU's `LICENSE` from
  the ICU release tag. Nineteen files, about 180 KB, some eight seconds.
- Every fetched text is checked against `scripts/qt_notices.sha256`, so a
  release is built from known content, and the release job caches the texts
  under that manifest's hash. Do not count on that cache: Actions evicts an
  entry after seven days, and a tag run cannot read a cache another tag wrote,
  so it helps only when a `workflow_dispatch` rehearsal ran on the default
  branch within the preceding week - which is the sequence below, but by
  arrangement rather than by guarantee. Assume a real release fetches. The
  script fails if a text is missing from the
  manifest, does not match it, or cannot be fetched, and the bundle script
  refuses to continue - no bundle ships with missing or unreviewed
  attributions.

A Qt or ICU bump therefore means reviewing the new licence texts once and
recording them: run the generator with `--update-manifest`, read the diff of
the manifest, and commit it. To regenerate without the network, point
`--qt-source` at unpacked `qt{base,svg,5compat}-everywhere-src` tarballs from
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
ships, because Qt 6.12 requires 3.25 or later, and that Qt comes from
`aqtinstall` by hand rather than from `install-qt-action`, which expects a runner
rather than a bare container. Qt's
`-DQT_FORCE_MIN_CMAKE_VERSION_FOR_USING_QT` would avoid the CMake upgrade; Qt
documents it as unsupported, so the pipeline installs CMake from pip instead, at
a pinned version.

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
   whether a platform ships, not after tagging.
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
