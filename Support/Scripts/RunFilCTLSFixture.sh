#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(cd -- "$SCRIPT_DIR/../.." && pwd)
CONFIGURATION=${1:-Debug}
if [[ $# -gt 0 ]]; then shift; fi
case "$CONFIGURATION" in
    Debug|Release) ;;
    *) echo "Expected Debug or Release" >&2; exit 2 ;;
esac
case "$(uname -s):$(uname -m)" in
    Linux:aarch64|Linux:arm64) FILC_VARIANT=linux_aarch64 ;;
    Linux:x86_64) FILC_VARIANT=linux_x86_64 ;;
    *) echo "The Fil-C TLS fixture requires native Linux ARM64 or x86_64" >&2; exit 2 ;;
esac

cd "$REPO_ROOT"
./SC.sh package install curl-filc
./SC.sh build compile SCTest "$CONFIGURATION" native --toolchain filc --output quiet

PACKAGES="$REPO_ROOT/_Build/_Packages"
COMPILER="$PACKAGES/filc_$FILC_VARIANT/build/bin/clang"
FIXTURES="$REPO_ROOT/Tools/Support/FilCTLSFixture"
mkdir -p "$REPO_ROOT/_Build/_FilCTLSFixture"
RUN_DIR=$(mktemp -d "$REPO_ROOT/_Build/_FilCTLSFixture/run.XXXXXXXX")
mkdir "$RUN_DIR/lib"
# Isolate the server's libraries from any package repair performed by the SC runner.
cp -L "$PACKAGES/openssl_filc/lib/libssl.so.3" "$PACKAGES/openssl_filc/lib/libcrypto.so.3" \
    "$PACKAGES/nghttp2_filc/lib/libnghttp2.so.14" "$PACKAGES/zlib_filc/lib/libz.so" \
    "$PACKAGES/zlib_filc/lib/libz.so.1" "$RUN_DIR/lib/"
RUNTIME_PATH="$RUN_DIR/lib"
SERVER_PID=
cleanup()
{
    local status=$?
    if [[ -n "$SERVER_PID" ]]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    echo "Fil-C TLS fixture evidence: $RUN_DIR"
    if [[ $status -ne 0 && -f "$RUN_DIR/server.log" ]]; then
        cat "$RUN_DIR/server.log"
    fi
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM HUP

env -u LD_LIBRARY_PATH "$COMPILER" -std=c11 -O2 -Wall -Wextra -Werror \
    Tools/Support/FilCTLSFixture.c \
    -I"$PACKAGES/openssl_filc/include" -I"$PACKAGES/nghttp2_filc/include" \
    -L"$PACKAGES/openssl_filc/lib" -L"$PACKAGES/nghttp2_filc/lib" -L"$PACKAGES/zlib_filc/lib" \
    -lssl -lcrypto -lnghttp2 -lz -o "$RUN_DIR/server"
env LD_LIBRARY_PATH="$RUNTIME_PATH" "$RUN_DIR/server" 0 3 120000 \
    "$FIXTURES/server.pem" "$FIXTURES/server-key.pem" >"$RUN_DIR/server.log" 2>&1 &
SERVER_PID=$!

for ((attempt = 0; attempt < 100; ++attempt)); do
    if grep -q '^READY port=' "$RUN_DIR/server.log"; then break; fi
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        echo "Fil-C TLS fixture exited before becoming ready" >&2
        exit 1
    fi
    sleep 0.1
done
PORT=$(sed -n 's/^READY port=\([0-9][0-9]*\) .*/\1/p' "$RUN_DIR/server.log")
if [[ ! "$PORT" =~ ^[0-9]+$ ]]; then
    echo "Fil-C TLS fixture did not publish a loopback port" >&2
    exit 1
fi

env SC_TEST_HTTP_TLS_URL="https://localhost:$PORT/" \
    SC_TEST_HTTP_TLS_CA="$FIXTURES/test-ca.pem" \
    SC_TEST_HTTP_TLS_BAD_CA="$FIXTURES/bad-ca.pem" \
    SC_TEST_HTTP_TLS_WRONG_HOST_URL="https://127.0.0.1:$PORT/" \
    SC_CRYPTOGRAPHY_TEST_REQUIRE_OPENSSL=1 \
    timeout 180 ./SC.sh build run SCTest "$CONFIGURATION" native --toolchain filc -- "$@"
wait "$SERVER_PID"
SERVER_PID=
grep -q '^SUMMARY accepted=3 requests=1 failures=0$' "$RUN_DIR/server.log"
cat "$RUN_DIR/server.log"
