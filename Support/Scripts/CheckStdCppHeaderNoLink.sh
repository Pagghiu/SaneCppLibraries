#!/bin/sh
set -eu

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" 2>/dev/null && pwd)
REPO_ROOT=$(cd -- "$SCRIPT_DIR/../.." 2>/dev/null && pwd)
TEST_FILE="$REPO_ROOT/Support/CompileTests/StdCppHeaderNoLinkProbe.cpp"
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

COMPILERS=""
for compiler in clang++ g++; do
    if command -v "$compiler" >/dev/null 2>&1; then
        COMPILERS="$COMPILERS $compiler"
    fi
done

if [ -z "$COMPILERS" ]; then
    echo "No supported compiler found (expected clang++ or g++)." >&2
    exit 1
fi

echo "Running standard C++ header without C++ runtime link check..."

for compiler in $COMPILERS; do
    binary="$TMP_DIR/$(basename "$compiler")-probe"
    echo "Testing $compiler"
    "$compiler" -std=c++20 -fno-exceptions -fno-rtti -nostdlib++ "$TEST_FILE" -o "$binary"

    if command -v otool >/dev/null 2>&1; then
        linked="$(otool -L "$binary")"
        echo "$linked"
        if echo "$linked" | grep -E 'libc\+\+|libstdc\+\+' >/dev/null; then
            echo "$compiler linked a C++ standard runtime unexpectedly" >&2
            exit 1
        fi
    elif command -v ldd >/dev/null 2>&1; then
        linked="$(ldd "$binary")"
        echo "$linked"
        if echo "$linked" | grep -E 'libc\+\+|libstdc\+\+' >/dev/null; then
            echo "$compiler linked a C++ standard runtime unexpectedly" >&2
            exit 1
        fi
    else
        echo "Cannot inspect linked libraries: expected otool or ldd" >&2
        exit 1
    fi
done

echo "Standard C++ header without C++ runtime link check passed."
