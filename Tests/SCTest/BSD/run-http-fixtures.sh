#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
cd "$repo"
if [ "$(uname -s)" != OpenBSD ]; then
    printf '%s\n' 'Run this validation on OpenBSD.' >&2
    exit 1
fi
if [ "$#" -eq 0 ]; then
    printf 'Usage: %s SCTest [SCTest ...]\n' "$0" >&2
    exit 1
fi

work=$(mktemp -d "${TMPDIR:-/tmp}/sc-openbsd-http.XXXXXXXX")
server_pid=
cleanup()
{
    status=$?
    if [ -n "$server_pid" ]; then
        kill "$server_pid" 2>/dev/null || :
        wait "$server_pid" 2>/dev/null || :
    fi
    if [ "$status" -ne 0 ] && [ -f "$work/server.log" ]; then
        cat "$work/server.log"
    fi
    rm -rf "$work"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

runtime=/usr/local/lib/eopenssl35
ssl=
for library in "$runtime"/libssl.so.[0-9]*; do
    test -f "$library" || continue
    test -z "$ssl"
    ssl=$library
done
test -n "$ssl"
crypto=
for library in "$runtime"/libcrypto.so.[0-9]*; do
    test -f "$library" || continue
    test -z "$crypto"
    crypto=$library
done
test -n "$crypto"

cc -std=c11 -O2 -Wall -Wextra -Werror -D_BSD_SOURCE \
    '-DSC_TLS_FIXTURE_OPENSSL_VERSION_PREFIX="OpenSSL 3.5."' \
    '-DSC_TLS_FIXTURE_NGHTTP2_VERSION="1.70.0"' \
    -I/usr/local/include/eopenssl35 -I/usr/local/include Tools/Support/FilCTLSFixture.c \
    "$ssl" "$crypto" -L/usr/local/lib -lnghttp2 -lz -o "$work/server"

expected_requests=$#
expected_connections=$((expected_requests * 3))
fixtures=$repo/Tools/Support/FilCTLSFixture
: >"$work/server.log"
LD_LIBRARY_PATH="$runtime" "$work/server" 0 "$expected_connections" 120000 \
    "$fixtures/server.pem" "$fixtures/server-key.pem" >"$work/server.log" 2>&1 &
server_pid=$!

port=
attempt=0
while [ "$attempt" -lt 30 ]; do
    port=$(sed -n 's/^READY port=\([0-9][0-9]*\) .*/\1/p' "$work/server.log")
    if [ -n "$port" ]; then
        break
    fi
    if ! kill -0 "$server_pid" 2>/dev/null; then
        printf '%s\n' 'The OpenBSD HTTP fixture exited before becoming ready.' >&2
        exit 1
    fi
    attempt=$((attempt + 1))
    sleep 1
done
if [ -z "$port" ]; then
    printf '%s\n' 'The OpenBSD HTTP fixture did not publish a loopback port.' >&2
    exit 1
fi

for executable do
    SC_TEST_HTTP_TLS_URL="https://localhost:$port/" \
        SC_TEST_HTTP_TLS_CA="$fixtures/test-ca.pem" \
        SC_TEST_HTTP_TLS_BAD_CA="$fixtures/bad-ca.pem" \
        SC_TEST_HTTP_TLS_WRONG_HOST_URL="https://127.0.0.1:$port/" \
        sh Tests/SCTest/BSD/run.sh prebuilt "$executable" \
        --test HttpClientTest --test-section 'https and http2 local fixture'
done

wait "$server_pid"
server_pid=
grep -q "^SUMMARY accepted=$expected_connections requests=$expected_requests failures=0$" "$work/server.log"
cat "$work/server.log"
