#!/bin/sh
SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" 2>/dev/null && pwd)

# Detect platform
PLATFORM=$(uname)

case "$PLATFORM" in
MINGW*|MSYS*|CYGWIN*)
    if ! command -v cygpath >/dev/null 2>&1; then
        echo "cygpath is required to run SC.sh from Windows Bash"
        exit 1
    fi

    SCRIPT_DIR_WIN=$(cygpath -w "$SCRIPT_DIR")
    export SC_SH_SCRIPT_DIR_WIN="$SCRIPT_DIR_WIN"

    FORWARD_BAT=$(mktemp "${TMPDIR:-/tmp}/sc-sh-forward.XXXXXX.bat") || exit 1
    trap 'rm -f "$FORWARD_BAT"' EXIT HUP INT TERM

    CALL_ARGS=
    ARG_INDEX=0
    for ARG in "$@"; do
        eval "SC_SH_ARG_${ARG_INDEX}=\$ARG"
        eval "export SC_SH_ARG_${ARG_INDEX}"
        CALL_ARGS="${CALL_ARGS} \"%SC_SH_ARG_${ARG_INDEX}%\""
        ARG_INDEX=$((ARG_INDEX + 1))
    done

    {
        printf '@echo off\r\n'
        printf 'setlocal DisableDelayedExpansion\r\n'
        printf 'pushd "%%SC_SH_SCRIPT_DIR_WIN%%"\r\n'
        printf 'if errorlevel 1 exit /b 1\r\n'
        printf 'call SC.bat%s\r\n' "$CALL_ARGS"
        printf 'set "SC_SH_EXIT_CODE=%%ERRORLEVEL%%"\r\n'
        printf 'popd\r\n'
        printf 'exit /b %%SC_SH_EXIT_CODE%%\r\n'
    } > "$FORWARD_BAT"

    SC_SH_FORWARD_BAT_WIN=$(cygpath -w "$FORWARD_BAT")
    export SC_SH_FORWARD_BAT_WIN
    (
        cd "$(dirname "$FORWARD_BAT")" || exit 1
        cmd //c "%SC_SH_FORWARD_BAT_WIN%"
    )
    exit $?
    ;;
esac

BOOTSTRAP_EXE="${SCRIPT_DIR}/_Build/_Tools/${PLATFORM}/ToolsBootstrap"

# Create platform Tools directory if needed
mkdir -p "${SCRIPT_DIR}/_Build/_Tools/${PLATFORM}"

# Check if ToolsBootstrap needs to be built
if [ ! -f "$BOOTSTRAP_EXE" ] || [ "${SCRIPT_DIR}/Tools/ToolsBootstrap.c" -nt "$BOOTSTRAP_EXE" ]; then
    echo "ToolsBootstrap.c"
    if command -v cc >/dev/null 2>&1; then
        cc -o "$BOOTSTRAP_EXE" "${SCRIPT_DIR}/Tools/ToolsBootstrap.c" -std=c99 -D_DEBUG=1 -g -ggdb -O0
    else
        gcc -o "$BOOTSTRAP_EXE" "${SCRIPT_DIR}/Tools/ToolsBootstrap.c" -std=c99 -D_DEBUG=1 -g -ggdb -O0
    fi
    if [ $? -ne 0 ]; then
        echo "Failed to build ToolsBootstrap"
        exit 1
    fi
fi

# Execute ToolsBootstrap with original args
"$BOOTSTRAP_EXE" "$SCRIPT_DIR" "$SCRIPT_DIR/Tools" "$SCRIPT_DIR/_Build" "$SCRIPT_DIR" "$@"
