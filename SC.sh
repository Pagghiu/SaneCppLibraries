#!/bin/sh
SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" 2>/dev/null && pwd)

# Detect platform
PLATFORM=$(uname)

BOOTSTRAP_EXE="${SCRIPT_DIR}/_Build/_Tools/${PLATFORM}/ToolsBootstrap"

# Create platform Tools directory if needed
mkdir -p "${SCRIPT_DIR}/_Build/_Tools/${PLATFORM}"

# Check if ToolsBootstrap needs to be built
if [ ! -f "$BOOTSTRAP_EXE" ] || [ "${SCRIPT_DIR}/Tools/ToolsBootstrap.c" -nt "$BOOTSTRAP_EXE" ]; then
    echo "ToolsBootstrap.c"
    if command -v cc >/dev/null 2>&1; then
        cc -o "$BOOTSTRAP_EXE" "${SCRIPT_DIR}/Tools/ToolsBootstrap.c" -std=c99
    else
        gcc -o "$BOOTSTRAP_EXE" "${SCRIPT_DIR}/Tools/ToolsBootstrap.c" -std=c99
    fi
    if [ $? -ne 0 ]; then
        echo "Failed to build ToolsBootstrap"
        exit 1
    fi
fi

# Execute ToolsBootstrap with original args
"$BOOTSTRAP_EXE" "$SCRIPT_DIR" "$SCRIPT_DIR/Tools" "$SCRIPT_DIR/_Build" "$@"
