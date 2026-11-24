#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
cd "$repo"
case "$(uname -s)" in
    NetBSD|FreeBSD|OpenBSD) ;;
    *) printf '%s\n' 'Run this validation in a NetBSD, FreeBSD, or OpenBSD checkout.' >&2; exit 1 ;;
esac

require_optional_backends()
{
    case "$(uname -s)" in
        NetBSD|FreeBSD)
            export SC_CRYPTOGRAPHY_TEST_REQUIRE_OPENSSL=1 SC_FIBERS_TEST_REQUIRE_STACK_GROWTH=1
            ;;
    esac
}

run_suite()
{
    configuration=$1
    shift
    test_root=$(mktemp -d "$scratch/$configuration.XXXXXXXX")
    ./SC.sh build compile SCTest "$configuration"
    ./SC.sh build run SCTest "$configuration" -- --test-root "$test_root" "$@"
}

case "${1:-all}" in
    all|prebuilt-headers|prebuilt|Debug|Release) mode=${1:-all} ;;
    *) printf 'Usage: %s [all | prebuilt-headers | prebuilt SCTest [SCTest options...] | Debug [SCTest options...] | Release [SCTest options...]]\n' "$0" >&2; exit 1 ;;
esac
if [ "$#" -gt 0 ]; then shift; fi
if { [ "$mode" = all ] || [ "$mode" = prebuilt-headers ]; } && [ "$#" -ne 0 ]; then
    printf '%s\n' 'Set SC_TEST_PORT_OFFSET to configure ports for a complete run.' >&2
    exit 1
fi

scratch=$(mktemp -d "${TMPDIR:-/tmp}/sc-bsd-test.XXXXXXXX")
trap 'rm -rf "$scratch"' EXIT
trap 'exit 1' HUP INT TERM
scratch=$(CDPATH= cd -- "$scratch" && pwd -P)

if [ "$mode" = prebuilt ]; then
    executable=${1:?Pass the cross-built SCTest path under _Build/_Outputs.}
    shift
    test_root=$(mktemp -d "$scratch/prebuilt.XXXXXXXX")
    require_optional_backends
    "$executable" --library-root "$repo" --test-root "$test_root" "$@"
elif [ "$mode" = all ] || [ "$mode" = prebuilt-headers ]; then
    if [ "$mode" = all ]; then
        python3 Support/SingleFileLibs/python/amalgamate_single_file_libs.py
    elif [ ! -f _Build/_SingleFileLibrariesTest/Test_SaneCppFoundationStandalone.cpp ] ||
         [ ! -f _Build/_SingleFileLibraries/SaneCppFoundationStandalone.h ]; then
        printf '%s\n' 'Copy the generated headers and test sources from the matching checkout first.' >&2
        exit 1
    fi
    ./SC.sh build configure SCTest
    require_optional_backends
    run_suite Debug --all-tests
    run_suite Release --all-tests
    ./SC.sh build compile "SCSingleFileLibs:" Debug
    ./SC.sh build compile "SCSingleFileLibs:" Release
else
    run_suite "$mode" "$@"
fi
