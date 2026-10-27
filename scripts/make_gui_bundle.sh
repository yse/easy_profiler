#!/bin/bash
# Assemble a self-contained profiler_gui bundle for Linux.
#
# Usage: scripts/make_gui_bundle.sh <build-dir> <output-dir>
#
# Layout:
#   bin/profiler_gui, bin/qt.conf
#   lib/            the Qt and easy_profiler libraries the binary needs
#   plugins/        the Qt plugins Qt loads at run time rather than links
#   licenses/       Qt's licence texts and third-party attributions
#   run.sh          sets LD_LIBRARY_PATH and execs the binary
#
# The Qt libraries are copied out of the Qt that built the binary, so the bundle
# is dynamically linked against Qt as the LGPL requires, and the licence texts
# travel with it. Replacing a library in lib/ with another build of the same Qt
# minor version is expected to work and is the user's right under that licence.
set -euo pipefail

if [ $# -ne 2 ]; then
    echo "usage: $0 <build-dir> <output-dir>" >&2
    exit 2
fi

build_dir=$1
out=$2

# Addressed relative to the script, not the working directory, so the caller
# need not be standing in the repository root.
repository=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
notices_generator="$repository/scripts/qt_notices.py"

binary=$(find "$build_dir" -type f -name profiler_gui -perm -u+x | head -1)
if [ -z "$binary" ]; then
    echo "profiler_gui was not found under $build_dir" >&2
    exit 1
fi

for tool in qtpaths6 ldd python3; do
    command -v "$tool" > /dev/null || { echo "$tool is required" >&2; exit 1; }
done

qt_plugins=$(qtpaths6 --query QT_INSTALL_PLUGINS)
qt_version=$(qtpaths6 --query QT_VERSION)
qt_libs=$(qtpaths6 --query QT_INSTALL_LIBS)

# Everything below trusts qtpaths6 to describe the Qt the binary was linked
# against: the plugins come from it, and its SBOM is what the notices are
# generated from. A qtpaths6 from a different Qt than the build used would
# produce a bundle that mixes two Qt versions and attributes the wrong one.
linked_core=$(ldd "$binary" | sed -n 's|.*=> \(/[^ ]*/libQt6Core\.so[^ ]*\).*|\1|p' | head -1)
if [ -z "$linked_core" ]; then
    echo "$binary does not link libQt6Core; is this a GUI build?" >&2
    exit 1
fi
if [ "$(cd "$(dirname "$linked_core")" && pwd)" != "$(cd "$qt_libs" && pwd)" ]; then
    echo "qtpaths6 reports Qt $qt_version in $qt_libs, but $binary links" >&2
    echo "$linked_core. Put the Qt that built the binary first on PATH." >&2
    exit 1
fi

rm -rf "$out"
mkdir -p "$out/bin" "$out/lib" "$out/plugins" "$out/licenses"

cp "$binary" "$out/bin/"

# Qt loads these rather than linking them, so no dependency scan finds them.
# qsvg and qsvgicon are not optional: every toolbar icon is an SVG resource and
# without them the toolbars come up empty with no diagnostic.
plugin_files=(
    platforms/libqxcb.so
    # Shipped so --self-test can run the bundle on a machine with no display,
    # which is how the release pipeline verifies it.
    platforms/libqoffscreen.so
    imageformats/libqsvg.so
    iconengines/libqsvgicon.so
)
optional_plugin_dirs=(platforminputcontexts xcbglintegrations)

# Kept alongside the copies: ldd has to be run on the plugin where it still
# sits in the Qt installation. A Qt plugin's RUNPATH is $ORIGIN/../../lib, so
# ldd run on the copy resolves against the half-filled bundle and reports the
# Qt libraries it has not reached yet as "not found" - which the filter below
# silently drops, leaving libQt6Svg and libQt6XcbQpa out of the bundle.
plugin_sources=()

for relative in "${plugin_files[@]}"; do
    source_file="$qt_plugins/$relative"
    if [ ! -f "$source_file" ]; then
        echo "required Qt plugin is missing: $source_file" >&2
        exit 1
    fi
    mkdir -p "$out/plugins/$(dirname "$relative")"
    cp "$source_file" "$out/plugins/$relative"
    plugin_sources+=("$source_file")
done

for directory in "${optional_plugin_dirs[@]}"; do
    if [ -d "$qt_plugins/$directory" ]; then
        mkdir -p "$out/plugins/$directory"
        while IFS= read -r source_file; do
            cp "$source_file" "$out/plugins/$directory/"
            plugin_sources+=("$source_file")
        done < <(find "$qt_plugins/$directory" -maxdepth 1 -name '*.so')
    fi
done

# Walk the dependency closure of the binary and of every plugin, since a plugin
# pulls in Qt libraries the executable itself never references. Only Qt, ICU and
# easy_profiler are bundled; X11, fontconfig and the C++ runtime come from the
# host, which is what keeps the bundle to a sane size.
bundle_dependencies() {
    local -a queue=("$@")
    local -A seen=()
    local item

    while [ ${#queue[@]} -gt 0 ]; do
        item=${queue[0]}
        queue=("${queue[@]:1}")

        [ -n "${seen[$item]:-}" ] && continue
        seen[$item]=1

        while read -r library; do
            [ -z "$library" ] && continue
            local base
            base=$(basename "$library")
            if [ ! -e "$out/lib/$base" ]; then
                cp -L "$library" "$out/lib/$base"
                queue+=("$library")
            fi
        done < <(ldd "$item" 2>/dev/null |
                 sed -n 's/.*=> \(\/[^ ]*\).*/\1/p' |
                 grep -E '/(libQt6[A-Za-z0-9]*|libicu[a-z0-9]*|libeasy_profiler)\.so' || true)
    done
}

bundle_dependencies "$binary" "${plugin_sources[@]}"

# The walk above drops anything ldd cannot resolve, so a missing library leaves
# a bundle that assembles cleanly and dies at start-up. Re-resolve every binary
# against the finished lib/ and fail here instead.
missing=0
while IFS= read -r object; do
    while read -r line; do
        echo "$object needs $line" >&2
        missing=1
    done < <(LD_LIBRARY_PATH="$out/lib" ldd "$object" 2>/dev/null |
             grep -E '^\s*(libQt6|libicu|libeasy_profiler)[^ ]* => not found' || true)
done < <(find "$out/bin" "$out/lib" "$out/plugins" -type f \
         \( -name '*.so*' -o -name profiler_gui \))
if [ "$missing" -ne 0 ]; then
    echo "the bundle is incomplete; see the unresolved libraries above" >&2
    exit 1
fi

# Qt finds its plugins relative to the executable through this file, so the
# bundle does not depend on the host's Qt installation being present at all.
cat > "$out/bin/qt.conf" <<'CONF'
[Paths]
Prefix = ..
Plugins = plugins
Libraries = lib
CONF

cat > "$out/run.sh" <<'LAUNCH'
#!/bin/sh
here=$(cd "$(dirname "$0")" && pwd)
LD_LIBRARY_PATH="$here/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" exec "$here/bin/profiler_gui" "$@"
LAUNCH
chmod +x "$out/run.sh"

#####################################################################
# Licences. The LGPL requires the licence text, a notice that Qt is used, and
# directions to the corresponding source. The Qt libraries also embed pcre2,
# harfbuzz, libpng and some thirty smaller components, each with its own notice
# requirement, and the official Qt packages carry ICU in lib/ as well.
#
# The attributions are generated here, from the SPDX SBOM inside the very Qt
# install the libraries above were copied from, so they cannot describe a
# different Qt than the bundle carries. The licence texts the SBOM only refers
# to are fetched from Qt's and ICU's release tags, checksum-pinned by
# scripts/qt_notices.sha256, and cached in QT_NOTICES_CACHE when it is set.
# Nothing LGPL-related lives in this repository: the obligation attaches to
# the bundle, and easy_profiler itself is MIT and Apache-2.0.
#####################################################################

# ICU's release is not in the SBOM, which records it as a system dependency;
# the Qt package names it in the library's real file name, libicuuc.so.73.2.
icu_soname=$(find "$out/lib" -maxdepth 1 -name 'libicuuc.so.*' -exec basename {} \; | head -1)
if [ -z "$icu_soname" ]; then
    echo "no ICU was bundled from $qt_libs; the official Qt packages carry it" >&2
    echo "and the notices generator attributes it. Is this an official Qt?" >&2
    exit 1
fi
icu_version=$(basename "$(readlink -f "$qt_libs/$icu_soname")")
icu_version=${icu_version#libicuuc.so.}
if ! [[ "$icu_version" =~ ^[0-9]+\.[0-9]+$ ]]; then
    echo "cannot tell the ICU release from $qt_libs/$icu_soname" >&2
    echo "(resolved to $icu_version); the notices would name the wrong ICU." >&2
    exit 1
fi

cache_option=()
if [ -n "${QT_NOTICES_CACHE:-}" ]; then
    cache_option=(--cache-dir "$QT_NOTICES_CACHE")
fi
if ! python3 "$notices_generator" \
        --qt-install "$(qtpaths6 --query QT_INSTALL_PREFIX)" \
        --qt-version "$qt_version" --icu-version "$icu_version" \
        --out "$out/licenses" ${cache_option[@]+"${cache_option[@]}"}; then
    echo "the notices could not be generated; the bundle would ship without" >&2
    echo "the attributions Qt requires. See the header of $notices_generator." >&2
    exit 1
fi

# The generator is handed the same Qt and ICU the bundle carries, so a mismatch
# here is a defect in it; the bundle still must not leave with one.
notices=$out/licenses/THIRD-PARTY-NOTICES.md
for required in THIRD-PARTY-NOTICES.md LICENSE.LGPLv3 LICENSE.GPLv3 LICENSE.ICU; do
    if [ ! -s "$out/licenses/$required" ]; then
        echo "$out/licenses/$required was not generated" >&2
        exit 1
    fi
done
notices_qt_version=$(sed -n 's/^Qt version: *//p' "$notices" | head -1)
notices_icu_version=$(sed -n 's/^ICU version: *//p' "$notices" | head -1)
if [ "$notices_qt_version" != "$qt_version" ] ||
   [ "$notices_icu_version" != "$icu_version" ]; then
    echo "the notices describe Qt ${notices_qt_version:-<unstated>} and ICU" >&2
    echo "${notices_icu_version:-<unstated>}; the bundle carries Qt $qt_version" >&2
    echo "and ICU $icu_version." >&2
    exit 1
fi

cp "$repository/easy_profiler_core/LICENSE.MIT" \
   "$repository/easy_profiler_core/LICENSE.APACHE" "$out/licenses/"

qt_series=${qt_version%.*}

cat > "$out/licenses/QT_NOTICE.md" <<NOTICE
# Qt in this bundle

This application uses the Qt toolkit, version $qt_version, under the GNU Lesser
General Public License version 3. Qt is not part of easy_profiler and is not
covered by easy_profiler's own MIT or Apache-2.0 licence.

The Qt libraries in \`lib/\` and the Qt plugins in \`plugins/\` are the
open-source binaries published by The Qt Company for Qt $qt_version. Qt is
linked dynamically: you may replace any of them with another build of the same
Qt minor version and run the result.

\`lib/\` also carries ICU, which the Qt packages bundle and Qt Core links
against. It is covered by \`LICENSE.ICU\`.

## Corresponding source

The complete corresponding source of this Qt is published by The Qt Company at
<https://download.qt.io/archive/qt/$qt_series/$qt_version/submodules/>, and will
remain available there for as long as that archive is maintained. The modules
bundled here are:

    qtbase-everywhere-src-$qt_version.tar.xz
    qtsvg-everywhere-src-$qt_version.tar.xz
    qt5compat-everywhere-src-$qt_version.tar.xz

\`THIRD-PARTY-NOTICES.md\` records the module revisions the bundled binaries were
built from, as far as Qt's own SBOM states them, and the configure options Qt
built with.

## Licence texts

- \`LICENSE.LGPLv3\` - the GNU Lesser General Public License v3, the licence
  this Qt is used under
- \`LICENSE.GPLv3\` - the GNU General Public License v3, which the LGPL refers to
- \`LICENSE.ICU\` - the Unicode licence, covering the bundled ICU
- \`THIRD-PARTY-NOTICES.md\` - the attributions for the third-party code the Qt
  libraries in this bundle embed, among them pcre2, harfbuzz and libpng
NOTICE

cat > "$out/README.txt" <<READ
easy_profiler GUI

Run ./run.sh

The bundle carries Qt, ICU and easy_profiler. From the host it expects X11 and
xcb, xkbcommon, fontconfig, freetype, glib, D-Bus, libglvnd for OpenGL, and a
C++ runtime. A desktop Debian or Ubuntu has all of them; a minimal or container
install may be missing the OpenGL ones, which on Ubuntu are libopengl0 and
libglx0.

Qt is included under the LGPL v3. See licenses/QT_NOTICE.md.
READ

printf 'bundle size: %s\n' "$(du -sh "$out" | cut -f1)"
find "$out" -type f | sort
