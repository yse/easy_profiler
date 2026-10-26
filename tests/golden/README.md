# Golden profiler files

These fixtures pin the byte-level layouts supported by the core reader:
v1.3, v2.0, and the current v2.1 wire format. They contain one descriptor,
one thread, and one block. Hex text keeps reviews and cross-platform checkouts
stable; tests decode it to the original binary bytes before parsing.

Changing a fixture requires an intentional format-compatibility decision. A
normal product patch release must not rewrite the v2.1 fixture.

`v2_1_writer.prof.hex` is different in kind: it was produced by building
easy_profiler v2.1.0 and running its writer (the path behind the GUI's
"Save As"), not by the current code. It pins the historic spelling of
`descriptors_memory_size`, which counted the two-byte record prefixes.
Regenerating it with a current build defeats its purpose and the test
that uses it will say so.

`v2_1_writer_resaved.prof.hex` comes from the same writer code (reader.cpp
and writer.cpp at `7ef14b9` are byte-identical to the v2.1.0 tag) and pins
the compounding effect of that spelling: a dump with 17 descriptors and one
block was loaded and saved six times in a row, so its header declares
`payloadSum + 12 * descriptors_count` for a descriptor section that holds
only the payload. The reader must open it like the first save.
