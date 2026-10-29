# easy_profiler core tests

The core suite uses Catch2 v3 and treats tests as the executable contract for
the profiler, its binary format, networking, and build-system integration. All
checked-in tests are expected to be green; fixed defects remain as regression
tests instead of a separate deliberately failing list.

## Quick start

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

The presets build only the C++ core and tests, without Qt applications or
samples. A manual equivalent is:

```bash
cmake -S . -B .tmp/builds/dev -DEASY_PROFILER_NO_GUI=ON \
  -DEASY_PROFILER_NO_SAMPLES=ON -DBUILD_TESTING=ON \
  -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build .tmp/builds/dev --parallel 8
ctest --test-dir .tmp/builds/dev --output-on-failure --parallel 8
```

Catch2 is found on the system first and fetched otherwise. For an offline
build, install Catch2 3.x or provide an existing source checkout:

```bash
cmake --preset dev -DFETCHCONTENT_SOURCE_DIR_CATCH2=/path/to/Catch2
```

## Test layers

| Target or CTest label | Contract |
|---|---|
| `easy_profiler_unit_tests` / `unit` | allocator, packed layout, public API, values, round trips, legacy formats, statistics and trees |
| `easy_profiler_thread_tests` / `threads` | concurrent recording, spin-lock behaviour and the reader's thread pool across thousands of loads |
| `easy_profiler_thread_failure_tests` / `threads` | the reader's thread pool when `pthread_create` fails; the failure is injected by `support/thread_failure_shim`, a shared library that interposes `pthread_create` for the whole process, static and shared `easy_profiler` alike, and the test requires proof that the injection happened (not built on Windows) |
| `easy_profiler_robustness_tests` / `robustness` | malformed and truncated `.prof` input is rejected without crashes or excessive allocation |
| `easy_profiler_net_tests` / `net` | socket lifecycle plus fragmented and coalesced protocol messages on ephemeral ports |
| `easy_profiler_regression_tests` / `regression` | permanent coverage for defects fixed during the revival |
| `easy_profiler_disabled_tests` / `disabled` | headers and API with `DISABLE_EASY_PROFILER` |
| `package` | installed-package and `add_subdirectory` consumers, both compiled as C++11 |
| `fuzz` | deterministic libFuzzer smoke run for the profile reader |
| `scripts` | the Python behind the release pipeline and the coverage gate: the changelog extractor, the Qt notices generator against verbatim excerpts of real Qt SBOMs (`qt_notices/`), and the Windows Qt resolver against a stand-in `aqt` (`support/fake_aqt`); registered when a Python 3 interpreter is found |

Every Catch `TEST_CASE` is registered independently with CTest and has a
30-second timeout, raised for the targets that need it: 60 seconds for the
thread target, 120 for the thread-failure target, which loads 20000 descriptors
twice, and 180 for the two package-consumer tests, which configure and build a
project of their own. One crash therefore cannot hide the rest of the suite.

Useful focused runs:

```bash
ctest --test-dir .tmp/builds/dev -L robustness --output-on-failure
ctest --test-dir .tmp/builds/dev -L net --output-on-failure
ctest --test-dir .tmp/builds/dev -L package --output-on-failure
ctest --test-dir .tmp/builds/dev -L threads --repeat until-fail:20 --output-on-failure
```

## Strict alignment and sanitizers

Packed records remain compact on disk, but strict mode accesses their scalar
fields through `memcpy`, avoiding undefined behaviour on alignment-sensitive
architectures.

```bash
cmake --preset strict
cmake --build --preset strict
ctest --preset strict

cmake --preset asan
cmake --build --preset asan
ctest --preset asan
```

The ASan preset combines AddressSanitizer and UndefinedBehaviorSanitizer while
exercising the default packed-access implementation. UBSan suppresses only
alignment reports originating in `alignment_helpers.h`; the profiler uses those
typed unaligned accesses deliberately to keep records compact. Alignment errors
anywhere else, and every other UBSan category, still fail the run. The separate
strict preset covers the portable `memcpy` implementation, which is also used by
the reader fuzz target.

The scheduled CI job also repeats the thread suite under ThreadSanitizer.
Package-consumer tests run in the regular build: consumers intentionally do not
inherit the library's private sanitizer link flags, so the ASan preset excludes
that label.

## Reader fuzzing

The fuzz target needs upstream LLVM Clang with libFuzzer; AppleClang does not
ship that runtime. Select the compiler before configuring when necessary:

```bash
CC=/path/to/clang CXX=/path/to/clang++ cmake --preset fuzz
cmake --build --preset fuzz
ctest --preset fuzz
```

CTest performs a reproducible 1,000-input smoke run with the checked-in corpus
and format dictionary. For a longer local campaign, invoke the built
`easy_profiler_reader_fuzzer` directly and raise `-runs` or use `-max_total_time`.

## Important format invariants

- The file stores blocks in post-order; the reader reconstructs the call tree
  from ordering and `[begin, end)` intervals.
- A normal block record consumes 23 bytes: a two-byte record prefix plus a
  21-byte payload.
- Header memory fields count payload bytes, not the record-length prefixes.
- Reader timestamps are converted to nanoseconds; writer-side tick values are
  not directly comparable after a round trip.
- Descriptor registration is process-wide and descriptors are intentionally not
  cleared between captures. Compare declared payload sizes, not whole file sizes.
- `EASY_THREAD_SCOPE` records a `ThreadFinished` event for each scoped thread.

Shared test helpers live in `tests/support`: `prof_fixture.h` owns isolated
temporary captures, while `prof_image.h` creates and mutates byte-level profile
images for compatibility and corruption tests.

## Coverage

`cmake --preset coverage && cmake --build --preset coverage && ctest --preset
coverage` builds and runs a dedicated single-process runner. Keeping all
instrumented core code in one binary avoids LLVM profile-signature mismatch
warnings from merging several Catch binaries.
CI publishes an HTML/JSON/text artifact and gates the current baseline at 80%
lines and 82% branches. Pull requests also require 80% coverage of changed
executable core lines. The next project target is 85% branch coverage globally;
thresholds should rise with new tests and never be lowered.
