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
# The Qt libraries are copied from the distribution, so the bundle is dynamically
# linked against Qt as the LGPL requires, and the licence texts travel with it.
# Replacing a library in lib/ with another build of the same Qt minor version is
# expected to work and is the user's right under that licence.
set -euo pipefail

if [ $# -ne 2 ]; then
    echo "usage: $0 <build-dir> <output-dir>" >&2
    exit 2
fi

build_dir=$1
out=$2

binary=$(find "$build_dir" -type f -name profiler_gui -perm -u+x | head -1)
if [ -z "$binary" ]; then
    echo "profiler_gui was not found under $build_dir" >&2
    exit 1
fi

for tool in qtpaths6 ldd; do
    command -v "$tool" > /dev/null || { echo "$tool is required" >&2; exit 1; }
done

qt_plugins=$(qtpaths6 --query QT_INSTALL_PLUGINS)
qt_version=$(qtpaths6 --query QT_VERSION)

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

for relative in "${plugin_files[@]}"; do
    source_file="$qt_plugins/$relative"
    if [ ! -f "$source_file" ]; then
        echo "required Qt plugin is missing: $source_file" >&2
        echo "on Debian and Ubuntu the SVG plugins come from qt6-svg-dev" >&2
        exit 1
    fi
    mkdir -p "$out/plugins/$(dirname "$relative")"
    cp "$source_file" "$out/plugins/$relative"
done

for directory in "${optional_plugin_dirs[@]}"; do
    if [ -d "$qt_plugins/$directory" ]; then
        mkdir -p "$out/plugins/$directory"
        find "$qt_plugins/$directory" -maxdepth 1 -name '*.so' -exec cp {} "$out/plugins/$directory/" \;
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
                 grep -E '/(libQt6[A-Za-z0-9]*|libicu[a-z]*|libeasy_profiler)\.so' || true)
    done
}

mapfile -t plugin_list < <(find "$out/plugins" -name '*.so')
bundle_dependencies "$binary" "${plugin_list[@]}"

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
# directions to the corresponding source. The Qt libraries also embed freetype,
# harfbuzz, libpng, pcre2 and zlib, each with its own notice requirement; the
# distribution's copyright file carries those attributions.
#####################################################################
for candidate in /usr/share/doc/qt6-base-dev/copyright /usr/share/doc/libqt6core6t64/copyright; do
    if [ -f "$candidate" ]; then
        cp "$candidate" "$out/licenses/qt6-copyright-and-third-party-notices.txt"
        break
    fi
done
if [ ! -f "$out/licenses/qt6-copyright-and-third-party-notices.txt" ]; then
    echo "Qt's copyright file was not found; the bundle would ship without the" >&2
    echo "third-party attributions Qt requires" >&2
    exit 1
fi

for license in LGPL-3 GPL-3; do
    if [ -f "/usr/share/common-licenses/$license" ]; then
        cp "/usr/share/common-licenses/$license" "$out/licenses/$license.txt"
    fi
done

cp easy_profiler_core/LICENSE.MIT easy_profiler_core/LICENSE.APACHE "$out/licenses/"

distribution=$(. /etc/os-release && echo "$PRETTY_NAME")

cat > "$out/licenses/QT_NOTICE.md" <<NOTICE
# Qt in this bundle

This application uses the Qt toolkit, version $qt_version, under the GNU Lesser
General Public License version 3. Qt is not part of easy_profiler and is not
covered by easy_profiler's own MIT or Apache-2.0 licence.

The Qt libraries in \`lib/\` and the Qt plugins in \`plugins/\` are the binaries
packaged by $distribution. Qt is linked dynamically: you may replace any of them
with another build of the same Qt minor version and run the result.

## Corresponding source

The complete corresponding source of this Qt, including the distribution's
patches, is published by $distribution alongside the binary packages, and
upstream at <https://download.qt.io/archive/qt/>. Fetch the packaged source with:

    apt-get source qt6-base qt6-svg qt6-5compat

## Licence texts

- \`LGPL-3.txt\` - the GNU Lesser General Public License v3
- \`GPL-3.txt\` - the GNU General Public License v3, which the LGPL refers to
- \`qt6-copyright-and-third-party-notices.txt\` - Qt's copyright statement and
  the attributions for the third-party code Qt embeds, among them freetype,
  harfbuzz, libpng, pcre2 and zlib
NOTICE

cat > "$out/README.txt" <<READ
easy_profiler GUI

Run ./run.sh

The bundle carries Qt and easy_profiler. It expects X11, fontconfig and a C++
runtime from the host; on a Debian or Ubuntu system those are already present.

Qt is included under the LGPL v3. See licenses/QT_NOTICE.md.
READ

printf 'bundle size: %s\n' "$(du -sh "$out" | cut -f1)"
find "$out" -type f | sort
