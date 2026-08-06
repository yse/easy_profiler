# easy_profiler core tests

Unit tests for `easy_profiler_core` built on [Catch2 v3](https://github.com/catchorg/Catch2).
The suite is meant to be an executable specification of the core's current
behaviour, so that refactoring rests on a checkable contract rather than on care.

## Quick start

```bash
cmake -S . -B build -DEASY_PROFILER_NO_GUI=ON -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j8

ctest --test-dir build -LE known-issue --output-on-failure   # the working run, green
ctest --test-dir build -L  known-issue                       # the defect checklist, red
```

Catch2 comes from the system (`find_package(Catch2 3)`) and is fetched with
`FetchContent` when it is not there. To build without network access, install
Catch2 3.x system-wide or point at already downloaded sources:

```bash
cmake -S . -B build -DBUILD_TESTING=ON -DFETCHCONTENT_SOURCE_DIR_CATCH2=/path/to/Catch2
```

## Targets and labels

| Target | CTest label | Contents |
|---|---|---|
| `easy_profiler_unit_tests` | `unit` | allocator, structure layout, descriptors, blocks, values, format, tree |
| `easy_profiler_thread_tests` | `unit`, `threads` | multithreaded capture, spin lock |
| `easy_profiler_robustness_tests` | `robustness` | parsing corrupt and forged `.prof` files |
| `easy_profiler_net_tests` | `net` | protocol structures, loopback sockets |
| `easy_profiler_disabled_tests` | `unit`, `disabled` | build with `DISABLE_EASY_PROFILER` |
| `easy_profiler_known_issues_tests` | `known-issue` | **deliberately red** tests for confirmed defects |

Every `TEST_CASE` is registered as its own CTest test (`catch_discover_tests`), so
a failure or a crash in one case does not take its neighbours with it. That
matters for `robustness` and `known-issue`, where crashing is an expected outcome.

## The red tests are a checklist, not a breakage

The `known-issue` target holds tests for defects confirmed in the current code.
They fail, and that is by design: the suite doubles as a work list. Each such test
carries a `file:line` reference and a description of what should happen once fixed.

When a defect is fixed its test turns green and moves to a regular target.

An unfiltered `ctest` will be red. Use `ctest -LE known-issue` to check whether
everything is in order.

## What the tests already pin down

Beyond the obvious coverage, the suite locks in several non-obvious properties of
the core — the ones that break most quietly under refactoring:

- **The call tree is not stored in the file.** Topology is reconstructed from
  write order (post-order) and the `[begin, end)` intervals.
  `test_tree_reconstruction.cpp` asserts the exact shape of the tree, not merely
  that some block is present.
- **23 bytes per block.** `test_layout_invariants.cpp` and
  `test_bytes_per_block.cpp` pin the structure sizes and the cost of a record.
  The README's claim about 12 million blocks follows from this figure, which has
  no headroom.
- **`used_memory_size` differs from the size on the wire.** The header counts the
  payload (21 bytes); the allocator adds the two-byte length prefix, making 23.
- **`EASY_FUNCTION` stores the name as a runtime name**, while
  `EASY_BLOCK("literal")` stores it in the descriptor. The branch is chosen by
  `std::is_reference<decltype(name)>` and it changes the record size.
- **The reader reports nanoseconds, not ticks.** A duration read back cannot be
  compared against the tick count passed to `storeBlock`.
- **`EASY_THREAD_SCOPE` adds a `ThreadFinished` event** per thread, so the total
  block count exceeds the expected one by the number of threads.
- **An empty dump returns 0 but still creates a file** — with a header and the
  descriptor table.
- **`toValue()` and `toArray()` are asymmetric:** an array reads as a scalar, but
  not the other way round.

## Sanitizers

```bash
cmake -S . -B build-san -DEASY_PROFILER_NO_GUI=ON -DBUILD_TESTING=ON \
      -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=OFF \
      -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g"
cmake --build build-san -j8
ctest --test-dir build-san -LE known-issue
```

ASan is clean. **UBSan does print misaligned-access diagnostics** — expected
rather than a defect: the format packs records back to back, without alignment,
for the sake of those 23 bytes per block. Hence messages such as

```
alignment_helpers.h:71: runtime error: store to misaligned address ... requires 2 byte alignment
reader.cpp:1136: runtime error: store to misaligned address ... requires 8 byte alignment
serialized_block.h:251: runtime error: load of misaligned address ... requires 4 byte alignment
```

Formally this is UB by the standard; practically it works on x86 and ARM64. For
platforms where unaligned access is not allowed, `alignment_helpers.h` provides
the `EASY_ENABLE_STRICT_ALIGNMENT` path with byte-wise access.

Note that by default UBSan prints diagnostics but **does not fail the test**. A
green `ctest` under sanitizers therefore does not mean there were no UBSan
messages — they have to be read in the output. To fail on them, add
`-fno-sanitize-recover=undefined`.

## Coverage

LLVM source-based coverage (it gives regions and branches, not just lines):

```bash
cmake -S . -B build-cov -DEASY_PROFILER_NO_GUI=ON -DEASY_PROFILER_NO_SAMPLES=ON \
      -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=OFF \
      -DCMAKE_CXX_FLAGS="-fprofile-instr-generate -fcoverage-mapping -g -O0" \
      -DCMAKE_EXE_LINKER_FLAGS="-fprofile-instr-generate"
cmake --build build-cov -j8

mkdir -p /tmp/easyprof-cov
LLVM_PROFILE_FILE=/tmp/easyprof-cov/cov-%p.profraw ctest --test-dir build-cov

llvm-profdata merge -sparse /tmp/easyprof-cov/cov-*.profraw -o /tmp/easyprof-cov/all.profdata

MAIN=build-cov/bin/easy_profiler_unit_tests
OBJS=$(for b in build-cov/bin/easy_profiler_*_tests; do [ "$b" = "$MAIN" ] || printf -- "-object %s " "$b"; done)
llvm-cov report "$MAIN" $OBJS -instr-profile=/tmp/easyprof-cov/all.profdata \
    -ignore-filename-regex='(_deps|/tests/|/usr/)'
```

On macOS `llvm-profdata`/`llvm-cov` are invoked through `xcrun`. The `%p`
placeholder in `LLVM_PROFILE_FILE` is mandatory: `catch_discover_tests` runs each
`TEST_CASE` as its own process, and without the pid substitution they would
overwrite each other's profile.

A line-by-line report for one file:

```bash
llvm-cov show "$MAIN" $OBJS -instr-profile=/tmp/easyprof-cov/all.profdata \
    easy_profiler_core/chunk_allocator.h
```

### Current numbers

Measured on macOS arm64, AppleClang, `Debug -O0`:

| | Coverage |
|---|---:|
| Lines | **78.9 %** |
| Regions | **79.2 %** |
| Functions | **86.0 %** |
| Branches | **63.6 %** |

By key file: `spin_lock.h`, `stack_buffer.h`, `thread_storage.h`,
`serialized_block.cpp`, `profiler_colors.h`, `writer.h` — 100 %;
`profiler.cpp` — 94.6 %; `thread_storage.cpp` — 90.6 %; `reader.h` — 90.7 %;
`easy_socket.cpp` — 84.7 %; `profile_manager.cpp` — 83.8 %; `reader.cpp` — 70.5 %.

### Remaining gaps

What follows is unreachable from the suite by construction, not by oversight:

| Area | Why it is not covered |
|---|---|
| `event_trace_win.cpp` | Windows-only, needs administrator rights |
| `#ifdef _WIN32` branches in `profile_manager.cpp` and `easy_socket.cpp` | unreachable on Unix, and vice versa |
| the asynchronous dump in `dumpBlocksToStream` | only used when streaming over the network with an interrupt |
| part of `writer.cpp` (65 %) | serializing context switches and bookmarks on re-save |
| `nonscoped_block.cpp` (56 %) | destruction paths of a block with a dynamic name |
| the stubs in `include/easy/profiler.h` | measured by the separate `easy_profiler_disabled_tests` binary, only partly reflected in the combined report |

```
tests/
  support/prof_fixture.h   # capture(): enable, dump, read back
  support/prof_image.h     # take a .prof image and corrupt header fields
  test_*.cpp               # one file per area
```

`capture()` resets state left over from the previous test: the profiler is a
process-wide singleton, and without the reset one test's blocks leak into the
next one's dump.

Two things worth remembering when adding tests:

1. **The descriptor table is process-wide and never cleared.** Every dump carries
   all descriptors registered since process start, so the file size depends on
   what ran earlier. Compare `used_memory_size`, not file sizes.
2. **`EASY_BLOCK` builds variable names from `__LINE__`.** Two blocks on one line
   produce a `redefinition` compile error.
