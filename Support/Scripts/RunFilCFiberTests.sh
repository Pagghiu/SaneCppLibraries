#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(cd -- "$SCRIPT_DIR/../.." && pwd)
CONFIGURATION=${1:-Release}
if [[ $# -gt 1 ]]; then
    echo "Usage: $0 [Debug|Release]" >&2
    exit 2
fi
case "$CONFIGURATION" in
    Debug|Release) ;;
    *) echo "Expected Debug or Release" >&2; exit 2 ;;
esac
case "$(uname -s):$(uname -m)" in
    Linux:aarch64|Linux:arm64|Linux:x86_64) ;;
    *) echo "The glibc Fil-C fiber tests require native Linux" >&2; exit 2 ;;
esac

cd "$REPO_ROOT"
./SC.sh build compile SCTest "$CONFIGURATION" native --toolchain filc-glibc
# Fail closed if the compiler/runtime ever falls back to unsupported-path tests.
env SC_FIBERS_TEST_REQUIRE_RUNTIME_STACKS=1 \
    timeout 120 ./SC.sh build run SCTest "$CONFIGURATION" native --toolchain filc-glibc -- --test FibersTest
env SC_FIBERS_TEST_REQUIRE_RUNTIME_STACKS=1 \
    timeout 120 ./SC.sh build run SCTest "$CONFIGURATION" native --toolchain filc-glibc -- --test AsyncFibersTest
