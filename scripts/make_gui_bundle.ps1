# Assemble a self-contained profiler_gui bundle for Windows.
#
# Usage: pwsh scripts/make_gui_bundle.ps1 <build-dir> <output-dir>
#
# Layout, which is the layout Qt itself looks for next to an executable:
#   profiler_gui.exe, easy_profiler.dll, Qt6*.dll
#   platforms/, imageformats/, iconengines/, ...   the Qt plugins
#   licenses/        Qt's licence texts and third-party attributions
#   README.txt
#
# The counterpart of scripts/make_gui_bundle.sh, which is irreducibly Linux:
# windeployqt reads the executable's import table and copies the Qt libraries
# and plugins itself, so there is no dependency walk here. What is here instead
# is everything windeployqt does not do or does not document.
#
# Qt is copied out of the Qt that built the binary, so the bundle is dynamically
# linked against Qt as the LGPL requires, and the licence texts travel with it.
param(
    [Parameter(Mandatory = $true)][string] $BuildDir,
    [Parameter(Mandatory = $true)][string] $OutDir
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
# Off so that a failing windeployqt or notices run reaches the exit-code checks
# below and reports what it means. PowerShell 7.4 turned this on by default,
# which would instead throw a generic native-command error from the call itself.
$PSNativeCommandUseErrorActionPreference = $false

function Fail($message) {
    throw $message
}

# Addressed relative to the script, not the working directory, so the caller
# need not be standing in the repository root.
$repository = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$noticesGenerator = Join-Path $repository 'scripts/qt_notices.py'

$binary = Get-ChildItem -Path $BuildDir -Recurse -Filter 'profiler_gui.exe' -File |
    Select-Object -First 1
if (-not $binary) { Fail "profiler_gui.exe was not found under $BuildDir" }

foreach ($tool in @('windeployqt.exe', 'qtpaths6.exe', 'python.exe')) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        Fail "$tool is required and was not found on PATH"
    }
}

$qtVersion = (& qtpaths6 --query QT_VERSION).Trim()
$qtPrefix = (& qtpaths6 --query QT_INSTALL_PREFIX).Trim()
$qtPlugins = (& qtpaths6 --query QT_INSTALL_PLUGINS).Trim()

# Created here, never cleared: a mistyped destination must not cost a tree.
if (Test-Path $OutDir) {
    Fail ("$OutDir already exists; this script creates its output directory" +
          " and will not touch an existing one. Remove it or name another.")
}
New-Item -ItemType Directory -Path $OutDir, (Join-Path $OutDir 'licenses') | Out-Null
# Absolute from here on: the caller passes a relative path, and the listing at
# the end cuts it off the front of each full name.
$OutDir = (Resolve-Path $OutDir).Path

Copy-Item $binary.FullName $OutDir
# easy_profiler's own, which windeployqt leaves alone: it deploys Qt.
$core = Get-ChildItem -Path $BuildDir -Recurse -Filter 'easy_profiler.dll' -File |
    Select-Object -First 1
if (-not $core) { Fail "easy_profiler.dll was not found under $BuildDir; configure with -DBUILD_SHARED_LIBS=ON" }
Copy-Item $core.FullName $OutDir

$deployed = Join-Path $OutDir 'profiler_gui.exe'

# Every exclusion is a binary this bundle has no attribution for and no right
# to redistribute, which matters because the notices generator below refuses to
# describe a bundle it cannot account for:
#   --no-compiler-runtime    Qt's own documentation warns that the MSVC runtime
#                            windeployqt falls back to on a developer machine is
#                            "not intended or licensed for redistribution".
#                            README.txt sends the user to the redistributable.
#   --no-opengl-sw           opengl32sw.dll is Mesa's software rasteriser, a
#                            third party whose notice is not in Qt's SBOM.
#   --no-system-*-compiler   the Direct3D compilers belong to the system.
#   --no-translations        the GUI has no translations to go with them.
& windeployqt --release --verbose 1 `
    --no-compiler-runtime --no-opengl-sw `
    --no-system-d3d-compiler --no-system-dxc-compiler `
    --no-translations `
    $deployed
if ($LASTEXITCODE -ne 0) { Fail "windeployqt failed with exit code $LASTEXITCODE" }

# Qt loads this rather than linking it, and nothing in the executable refers to
# it, so windeployqt cannot know it is wanted. Shipped so --self-test can run
# the bundle with no desktop session, which is how the release pipeline
# verifies it - the same reason libqoffscreen.so is in the Linux bundle.
$offscreen = Join-Path $qtPlugins 'platforms/qoffscreen.dll'
if (-not (Test-Path $offscreen)) { Fail "the offscreen platform plugin is missing: $offscreen" }
$platforms = Join-Path $OutDir 'platforms'
# Without this, a windeployqt that deployed no platform plugin at all would
# leave Copy-Item writing a *file* called platforms, and the assertion below
# would then fail for the wrong reason.
New-Item -ItemType Directory -Force -Path $platforms | Out-Null
Copy-Item $offscreen $platforms

# windeployqt's plugin selection is not documented: Qt's deployment page states
# no default set and no rule by which one is chosen. The SVG plugins are
# believed to arrive through the icon engine, but believing is not enough -
# every toolbar icon is an SVG resource, and without them the toolbars come up
# empty with no diagnostic at all. So assert, and if this ever fires the fix is
# to pass --include-plugins qsvg,qsvgicon rather than to delete the check.
$required = @(
    'platforms/qwindows.dll',
    'imageformats/qsvg.dll',
    'iconengines/qsvgicon.dll'
)
$missing = $required | Where-Object { -not (Test-Path (Join-Path $OutDir $_)) }
if ($missing) {
    Fail ("windeployqt did not deploy these plugins:`n  " + ($missing -join "`n  ") +
          "`nPass --include-plugins for them; without the SVG pair the toolbars" +
          " render empty and nothing reports it.")
}

# windeployqt copies from its own Qt, so a qtpaths6 from a different Qt than
# the one that built the binary would produce a bundle whose notices describe
# the wrong Qt. Cheapest honest check available on Windows, where there is no
# ldd: the Qt Core that actually landed has to be the Qt we are attributing.
$coreVersion = (Get-Item (Join-Path $OutDir 'Qt6Core.dll')).VersionInfo.FileVersion
if (-not $coreVersion) { Fail "the deployed Qt6Core.dll states no file version" }
if (-not $coreVersion.StartsWith($qtVersion)) {
    Fail ("qtpaths6 reports Qt $qtVersion in $qtPrefix, but the deployed" +
          " Qt6Core.dll is $coreVersion. Put the Qt that built the binary" +
          " first on PATH.")
}

# The official Windows Qt packages ship no ICU of their own, yet Qt6Core.dll
# imports icuuc.dll - by unversioned symbol names, which is how a build links
# the ICU that Windows itself provides in System32. windeployqt resolves that
# import against the host and copies the system DLL into the bundle, which is
# neither ours to redistribute nor anything Qt's SBOM describes, so the notices
# generator below refuses the bundle outright. Drop it: Qt6Core finds the same
# library through the normal search order on the user's machine, which is where
# it was always meant to come from.
$qtBin = Join-Path $qtPrefix 'bin'
foreach ($icu in @(Get-ChildItem -Path $OutDir -Filter 'icu*.dll' -File)) {
    if (Test-Path (Join-Path $qtBin $icu.Name)) {
        Fail ("$($icu.Name) was deployed from $qtBin, so this Qt ships its own" +
              " ICU and the bundle must attribute it rather than drop it. Pass" +
              " --icu-version to $noticesGenerator with that ICU's release.")
    }
    $system = Join-Path $env:SystemRoot "System32/$($icu.Name)"
    if (-not (Test-Path $system)) {
        Fail ("$($icu.Name) came from the host rather than from $qtBin, and" +
              " $system does not exist, so removing it would leave a bundle" +
              " that cannot start. Find what put $($icu.Name) on PATH.")
    }
    Remove-Item $icu.FullName
    Write-Host "dropped $($icu.Name): the system ICU, which Windows provides"
}

#####################################################################
# Licences. Identical duties to the Linux bundle, see make_gui_bundle.sh. The
# generator attributes the finished bundle rather than a list kept by hand, so
# a DLL windeployqt decides to add tomorrow cannot ship without its notice.
# No --icu-version is passed, because nothing ICU is left to attribute; the
# generator refuses the bundle if any of it reappears.
#####################################################################

$cacheOption = @()
if ($env:QT_NOTICES_CACHE) { $cacheOption = @('--cache-dir', $env:QT_NOTICES_CACHE) }
& python $noticesGenerator `
    --bundle $OutDir `
    --qt-install $qtPrefix `
    --qt-version $qtVersion `
    --out (Join-Path $OutDir 'licenses') `
    @cacheOption
if ($LASTEXITCODE -ne 0) {
    Fail ("the notices could not be generated; the bundle would ship without" +
          " the attributions Qt requires. See the header of $noticesGenerator.")
}

Copy-Item (Join-Path $repository 'easy_profiler_core/LICENSE.MIT') (Join-Path $OutDir 'licenses')
Copy-Item (Join-Path $repository 'easy_profiler_core/LICENSE.APACHE') (Join-Path $OutDir 'licenses')

# The output directory's own name is the archive's name, which is what a reader
# of README.txt has in front of them.
$bundleName = Split-Path $OutDir -Leaf

# The here-string reaches Windows with CRLF line endings already, because
# .gitattributes normalises this file on checkout. Converting it here would
# double the carriage returns.
Set-Content -Path (Join-Path $OutDir 'README.txt') -Encoding utf8 -Value @"
easy_profiler GUI

Run profiler_gui.exe in this folder. Keep the folder together: the Qt libraries
and the plugin subdirectories beside the executable are how it finds them.

First run on Windows

These binaries are not code-signed, and will not be. Windows therefore does not
know who published them, and you may meet that in three places.

1. "Windows protected your PC". Microsoft Defender SmartScreen shows this when
   it sees an executable it has no reputation for. Choose "More info", which
   reveals "Publisher: Unknown publisher" and a "Run anyway" button. That is
   the expected path, and the dialog is about the absence of a signature, not
   about anything found in the file.

2. A warning on download. Your browser may refuse the archive or label it as
   not commonly downloaded, for the same reason.

3. Smart App Control. If it is turned on - it is on by default only on a clean
   installation of Windows 11, never after an upgrade - it blocks unsigned
   executables outright. There is no "More info" and no "Run anyway": the app
   simply does not start. The only way past it is to turn Smart App Control
   off, and Windows will not let it be turned back on afterwards short of
   reinstalling. If you would rather not make that trade, build the GUI from
   source or run the Linux bundle.

You can confirm what you downloaded independently of any of this. Every release
asset is listed in SHA256SUMS, and carries a Sigstore build provenance
attestation that ties it to the workflow run and commit that produced it:

    gh attestation verify ${bundleName}.zip --repo yse/easy_profiler

Windows 10 version 1809 or newer

Qt requires it, and the bundle uses the Unicode library that Windows itself has
provided since Windows 10 version 1703. An error naming a missing icuuc.dll
means this Windows is older than that.

Microsoft Visual C++ runtime

The bundle does not carry the MSVC runtime, which Qt's own documentation says
is not licensed for redistribution this way. If profiler_gui.exe will not start
for a missing VCRUNTIME140.dll or MSVCP140.dll, install Microsoft's Visual C++
Redistributable for Visual Studio 2015-2022 (x64).

Qt is included under the LGPL v3. See licenses/QT_NOTICE.md.
"@

Write-Host "bundle:"
Get-ChildItem -Path $OutDir -Recurse -File |
    ForEach-Object { $_.FullName.Substring($OutDir.Length + 1).Replace('\', '/') } |
    Sort-Object
$size = (Get-ChildItem -Path $OutDir -Recurse -File | Measure-Object -Property Length -Sum).Sum
Write-Host ("bundle size: {0:N1} MiB" -f ($size / 1MB))
