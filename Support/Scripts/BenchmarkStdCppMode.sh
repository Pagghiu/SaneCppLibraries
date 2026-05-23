#!/bin/sh
set -eu

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" 2>/dev/null && pwd)
REPO_ROOT=$(cd -- "$SCRIPT_DIR/../.." 2>/dev/null && pwd)

CONFIGURATION=${CONFIGURATION:-Release}
ITERATIONS=${ITERATIONS:-3}
NORMAL_TARGET=${NORMAL_TARGET:-SCStdCppHeaderNoLinkTest}
STRICT_TARGET=${STRICT_TARGET:-SCTest}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --config)
            CONFIGURATION="$2"
            shift 2
            ;;
        --iterations)
            ITERATIONS="$2"
            shift 2
            ;;
        --normal-target)
            NORMAL_TARGET="$2"
            shift 2
            ;;
        --strict-target)
            STRICT_TARGET="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

time_command()
{
    target="$1"
    iteration="$2"
    tmp="$3"
    echo "build target=$target config=$CONFIGURATION iteration=$iteration"
    if command -v /usr/bin/time >/dev/null 2>&1; then
        /usr/bin/time -p "$REPO_ROOT/SC.sh" build compile "$target" "$CONFIGURATION" >"$tmp.out" 2>"$tmp.err"
    else
        "$REPO_ROOT/SC.sh" build compile "$target" "$CONFIGURATION" >"$tmp.out" 2>"$tmp.err"
    fi
    cat "$tmp.out"
    cat "$tmp.err" >&2
}

print_binary()
{
    target="$1"
    case "$(uname -s)" in
        Darwin)
            host=macOS
            ;;
        Linux)
            host=linux
            ;;
        *)
            host=
            ;;
    esac

    if [ -n "$host" ]; then
        find "$REPO_ROOT/_Build/_Outputs" -type f -path "*/$host-*-$CONFIGURATION/$target" -print | sort | tail -n 1
    else
        find "$REPO_ROOT/_Build/_Outputs" -type f \( -name "$target" -o -name "$target.exe" \) -path "*/$CONFIGURATION/*" \
            -print | sort | tail -n 1
    fi
}

report_binary()
{
    label="$1"
    binary="$2"
    echo "binary[$label]=$binary"
    if [ -n "$binary" ] && [ -f "$binary" ]; then
        python3 "$REPO_ROOT/Support/SizeAnalysis/size_report.py" "$binary" || true
        if command -v otool >/dev/null 2>&1; then
            otool -L "$binary" || true
        elif command -v ldd >/dev/null 2>&1; then
            ldd "$binary" || true
        fi
    else
        echo "Cannot find binary for $label" >&2
    fi
}

cd "$REPO_ROOT"
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

echo "Benchmarking normal target: $NORMAL_TARGET"
i=1
while [ "$i" -le "$ITERATIONS" ]; do
    time_command "$NORMAL_TARGET" "$i" "$TMP_DIR/normal-$i"
    i=$((i + 1))
done

echo "Benchmarking strict target: $STRICT_TARGET"
i=1
while [ "$i" -le "$ITERATIONS" ]; do
    time_command "$STRICT_TARGET" "$i" "$TMP_DIR/strict-$i"
    i=$((i + 1))
done

report_binary normal "$(print_binary "$NORMAL_TARGET")"
report_binary strict "$(print_binary "$STRICT_TARGET")"
