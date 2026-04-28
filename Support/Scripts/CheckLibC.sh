#!/bin/sh
set -eu

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" 2>/dev/null && pwd)
REPO_ROOT=$(cd -- "$SCRIPT_DIR/../.." 2>/dev/null && pwd)
TEST_FILE="$REPO_ROOT/Support/CompileTests/LibCCompileTest.cpp"
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

STANDARDS="c++14 c++17 c++20"
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

echo "Running LibC compatibility check..."

for compiler in $COMPILERS; do
    case "$compiler" in
        clang++)
            EXTRA_FLAGS="-nostdinc++ -fno-exceptions -fno-rtti"
            ;;
        g++)
            EXTRA_FLAGS="-fno-exceptions -fno-rtti -DSC_COMPILER_ENABLE_STD_CPP=1"
            ;;
        *)
            echo "Unsupported compiler: $compiler" >&2
            exit 1
            ;;
    esac

    for standard in $STANDARDS; do
        object_file="$TMP_DIR/$(basename "$compiler")-$standard.o"
        echo "Testing $compiler with -std=$standard"
        if ! "$compiler" -I "$REPO_ROOT" -std="$standard" $EXTRA_FLAGS -c "$TEST_FILE" -o "$object_file"; then
            echo "LibC compatibility check failed for $compiler with -std=$standard" >&2
            exit 1
        fi
    done
done

echo "LibC compatibility check passed."
