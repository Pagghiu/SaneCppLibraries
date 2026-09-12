# Bounded native directory manifest

Build a small command-line program named `manifest` using Sane C++ Libraries that recursively lists regular files below one root. Its interface is `manifest ROOT MAX_DEPTH`, with newline-delimited JSON records `{ "path": "...", "size": N }` ordered lexically by absolute native path. Use `FileSystemIterator` and caller-owned recursion storage; do not use STL containers, exceptions, RTTI, or an unbounded heap-backed traversal stack.

`MAX_DEPTH` is 0–8. If the directory tree needs more traversal state than your fixed caller-provided capacity, exit nonzero with a clear diagnostic and no partial success claim. Distinguish that capacity failure from a filesystem error. Ignore symlinks. Make a `build.sh` that writes `manifest` to the directory named by `SC_SKILL_EVAL_BUILD_DIR`, and write `DESIGN.md` explaining ownership, capacity, and error handling.

Do not read this benchmark's acceptance or review files while implementing.
