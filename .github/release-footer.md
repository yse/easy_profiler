---

## Verifying these files

`SHA256SUMS` detects a truncated or swapped download. It is not proof of
authorship: whoever could replace an asset could replace the sums file.

Every asset carries a build provenance attestation, which is. With the GitHub
CLI, signed in to GitHub:

```
gh attestation verify <file> --repo yse/easy_profiler
```

That confirms the file came out of this repository's release workflow at a
named commit.

## First run

Binaries are **not** code signed.

On Windows an unsigned executable raises "Windows protected your PC": choose
*More info*, then *Run anyway*. Where Smart App Control is enforcing, it blocks
unsigned executables outright and the only way through is to turn it off.

On macOS, download and unpack in a terminal:

```
curl -LO <asset url>
tar -xzf <asset>
```

A tarball fetched in a browser and unpacked by double-clicking marks every file
inside as quarantined, and Gatekeeper then refuses the library and the
command-line tools with no useful message. `curl` attaches no quarantine
attribute and `tar` does not propagate one. This applies to the library and the
command-line tools, not only to applications.

## What is in a package

Headers, the library, the CMake package config, `profiler_converter`,
`profiler_reader`, both licences, and a `BUILD-INFO.txt` naming the toolchain
that produced it. A prebuilt C++ binary is only usable against a compatible
toolchain, so check that file before reporting a link error.

Windows packages do not contain the MSVC runtime; install the official Visual
C++ Redistributable.

The GUI bundle is Linux x86_64 only in this release. It includes Qt under the
LGPL v3; see `licenses/QT_NOTICE.md` inside it. On Windows and macOS, build the
GUI from source.

The `.prof` wire format is unchanged at 2.1.0, so profiles written by 2.1
remain readable.
