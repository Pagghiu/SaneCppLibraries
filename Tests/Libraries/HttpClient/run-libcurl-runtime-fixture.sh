#!/bin/sh
# Run in a guest-local checkout with the usual SCTest build environment.
set -eu
cd "$(dirname "$0")/../../.."
case "$(uname -s)" in
    Linux) dl_lib=-ldl ;;
    *) echo "This fixture requires a libcurl HttpClient host" >&2; exit 1 ;;
esac
work=$(mktemp -d "${TMPDIR:-/tmp}/sc-libcurl-fixture.XXXXXXXX")
trap 'rm -rf "$work"' EXIT
fixture=Tests/Fixtures/HttpClient/LibCurlRuntimeFixture.c

cc -fPIC -shared -DSC_TEST_CURL_MISSING "$fixture" $dl_lib -o "$work/missing.so"
cc -fPIC -shared -DSC_TEST_CURL_INCOMPLETE -DSC_TEST_CURL_FEATURES=4 "$fixture" -o "$work/incomplete.so"

for config in Debug Release; do
    ./SC.sh build compile SCTest "$config"
    for scenario in missing incomplete basic tls http2; do
        preload=
        if [ "$scenario" = missing ]; then
            preload=$work/missing.so
        else
            cp "$work/incomplete.so" "$work/libcurl.so.4"
            case "$scenario" in
                incomplete) cp "$work/incomplete.so" "$work/libcurl.so" ;;
                basic) features=0 ;;
                tls) features=4 ;;
                http2) features=65536 ;;
            esac
            if [ "$scenario" != incomplete ]; then
                cc -fPIC -shared -DSC_TEST_CURL_FEATURES="$features" "$fixture" -o "$work/libcurl.so"
            fi
        fi
        LD_PRELOAD="$preload" LD_LIBRARY_PATH="$work${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
            SC_TEST_HTTP_LIBCURL_FIXTURE="$scenario" ./SC.sh build run SCTest "$config" -- \
            --test HttpClientTest --test-section "libcurl runtime fixture"
        printf '%s %s passed\n' "$config" "$scenario"
    done
done
