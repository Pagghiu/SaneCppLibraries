# Bounded binary file copy

Build `copy`, a Sane C++ Libraries command-line program with this interface:

`copy SOURCE DEST BUFFER_BYTES`

`SOURCE` and `DEST` are absolute native file paths. `BUFFER_BYTES` is an integer from 1 through 4096. Copy the entire source byte-for-byte to the destination using a fixed caller-owned buffer of at most `BUFFER_BYTES`; do not read the whole file into memory. A successful copy creates or truncates `DEST`. Open and validate the source before touching `DEST`, so a missing source leaves an existing destination unchanged.

Use `SC::FileDescriptor` and its span-based reads and writes. Treat an empty successful read as EOF. Report read, write, and open failures distinctly on stderr. Exit 0 on success, 1 on filesystem failure, and 2 on invalid command-line input, with a diagnostic on stderr for every failure. Do not use STL, exceptions, RTTI, heap allocation, or other file I/O APIs to do the copy.

Supply `build.sh` that writes `copy` to `SC_SKILL_EVAL_BUILD_DIR`, tests, and `DESIGN.md` explaining buffer ownership, EOF, resource cleanup, and error handling.

Do not read this benchmark's acceptance or review files while implementing.
