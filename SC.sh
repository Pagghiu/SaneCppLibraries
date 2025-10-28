#!/bin/sh
SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" 2>/dev/null && pwd)

# Detect platform
PLATFORM=$(uname)

BOOTSTRAP_EXE="${SCRIPT_DIR}/_Build/_Tools/${PLATFORM}/ToolsBootstrap"

# Create platform Tools directory if needed
mkdir -p "${SCRIPT_DIR}/_Build/_Tools/${PLATFORM}"

# Check if ToolsBootstrap needs to be built
if [ ! -f "$BOOTSTRAP_EXE" ] || [ "${SCRIPT_DIR}/Tools/ToolsBootstrap.cpp" -nt "$BOOTSTRAP_EXE" ]; then
    if command -v clang++ >/dev/null 2>&1; then
        clang++ -o "$BOOTSTRAP_EXE" "${SCRIPT_DIR}/Tools/ToolsBootstrap.cpp" -std=c++14 -pthread
    else
        g++ -o "$BOOTSTRAP_EXE" "${SCRIPT_DIR}/Tools/ToolsBootstrap.cpp" -std=c++14 -pthread
    fi
    if [ $? -ne 0 ]; then
        echo "Failed to build ToolsBootstrap"
        exit 1
    fi
fi

# Execute ToolsBootstrap with original args
"$BOOTSTRAP_EXE" "$SCRIPT_DIR" "$SCRIPT_DIR/Tools" "$SCRIPT_DIR/_Build" "$@"
