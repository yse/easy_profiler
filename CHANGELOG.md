# Changelog

## 3.0.0 — Unreleased

Version 3.0.0 modernizes the core test, packaging, and safety infrastructure. The
`.prof` wire format remains version 2.1.0 and existing profile files remain readable.

### Breaking changes

- Recompile applications and libraries that use easy_profiler before upgrading.
  Mixing 2.x binaries with 3.0 headers or libraries is unsupported.
- The API renames introduced by [PR #188](https://github.com/yse/easy_profiler/pull/188)
  are part of the 3.0 boundary: `now()` is now `timeNow()`, and the `version*()`
  functions are now `apiVersion*()`.
- CMake package-version compatibility is scoped to major version 3 and the
  shared-library SONAME is 3, so an incompatible 2.x runtime fails explicitly.
- `setValue(..., const T*, ..., size)` and `setText(..., const char*, ..., size)`
  now take `size_t`. Counts are validated before the serialized byte size is
  narrowed to `uint16_t`.
- `Value<T, true>::value()` is unavailable because a packed payload cannot
  safely expose a naturally aligned `T*`. Use `copyTo()`, `at()`, or
  `operator[]`; these accessors are alignment-safe in every build mode.
- `StringValue::c_str()` is unavailable because an `EASY_STRING` payload is not
  NUL-terminated, so `strlen` on it read past the record. Use `data()` with
  `length()` (the bytes before the first zero) or `size()` (the whole payload).
- `BaseBlockDescriptor::status()` is now an exported, atomically implemented
  library function. New headers therefore cannot link against an old 2.1 shared
  library.
- The CMake options `EASY_OPTION_TRUNCATE_RUNTIME_NAMES` and
  `EASY_OPTION_CHECK_MAX_VALUE_SIZE` no longer exist; passing them has no
  effect. Their checks are always on.

### Fixed

- On Windows a second process could bind the profiler's port while it was
  actively listened on. `EasySocket` set `SO_REUSEADDR` on every socket, and on
  Windows that option permits binding a port another socket holds, so two
  profiled applications could both listen on 28077 and the GUI could reach the
  wrong one. Listeners now use `SO_EXCLUSIVEADDRUSE` on Windows; POSIX keeps
  `SO_REUSEADDR`, where it only allows reuse after `TIME_WAIT`.
- `EasySocket::connect()` reports a refused connection as disconnected on every
  platform. `ECONNREFUSED` was handled on Apple only, so on Linux a refused
  connect kept retrying for the full second and left the socket in the
  `Connecting` state, which the GUI's reconnect logic reads as still alive. A
  connect that completes on a retry (`EISCONN`) now lands in `Connected` rather
  than `Connecting`, and one that gives up after a second no longer stays
  `Connecting`.
- The reader again accepts profiles saved by the 2.1 GUI. `writeTreesToStream()`
  declared `descriptors_memory_size` including the two-byte record prefixes; the
  hardened reader required the payload sum exactly and rejected those files with
  "Descriptor count or memory size does not match the profile header". Both
  spellings are now accepted, including the larger surplus left by repeated
  saves through an old GUI, with per-record bounds checks unchanged.
- Frame-time conversion calibrates the platform clock on first use again, so
  `frameTime(MICROSECONDS)` no longer returns zero in an application that never
  enables capture. This restores the documented "monitor fps while profiling is
  disabled" scenario.
- Context switch waits still open when the dump runs are closed at the capture
  boundary instead of being discarded, so a thread that is descheduled at that
  moment no longer loses its wait time.
- Context switch intervals that cross a capture boundary are clamped instead of
  dropped. Filtering the SystemTap log line by line orphaned one half of an
  interval, so a thread that left the CPU before the capture, or returned after
  it, silently lost its whole wait time.
- The GUI no longer reads past the record of a zero-element `EASY_ARRAY` when
  charting it: an element the array does not have is left out of the chart, and
  such an array is treated as an array rather than as a scalar in the values
  tree.

### Behavioral changes

- Oversized payloads and null non-empty `EASY_ARRAY`, `EASY_STRING`, and
  `EASY_TEXT` payloads are ignored safely. Zero-length arrays and explicit-size
  strings remain valid. Builds configured with `EASY_OPTION_LOG=ON` also print
  the reason to stderr.
- Oversized runtime block names are truncated safely.
- A listener can be started again after its previous `bind()` or `listen()`
  attempt failed.
- `writeTreesToStream` rejects descriptor tables with missing entries instead of
  silently writing a truncated profile.
- Buffering a non-seekable profile stream is capped at 64 MiB; seekable profile
  files are unaffected.

### Tooling and quality

- Added CMake presets, package-consumer tests, Bazel/Bzlmod support, fuzzing,
  sanitizers, LLVM branch coverage, and an uploaded HTML coverage report.
- Removed the obsolete `EASY_OPTION_TRUNCATE_RUNTIME_NAMES` and
  `EASY_OPTION_CHECK_MAX_VALUE_SIZE` CMake options. Their safety checks are now
  always enabled.

### Known issues

- Charting a single element of an `EASY_ARRAY` plots element 0 instead when
  "all threads" is selected and the profile has more than one thread. Selecting
  a specific thread, or profiling a single thread, charts the chosen element.
  Predates 3.0.0.
- `profiler_gui` does not link against Qt 6.8 on current macOS SDKs: Qt's own
  OpenGL wrapper resolves to `-framework AGL`, which Apple has removed. Use an
  older SDK or a Qt build that does not reference AGL.
