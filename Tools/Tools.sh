#!/bin/sh
LIBRARY_DIR="$1"        # Directory where "Libraries" exists
TOOL_SOURCE_DIR="$2"    # Directory where SC-${TOOL}.cpp file exists
BUILD_DIR="$3"          # Directory where output subdirectories must be placed
TOOL="${4:-build}"      # Tool to execute (build by default)

TOOL_OUTPUT_DIR="${BUILD_DIR}/_Tools"   # Directory where the ${TOOL} executable will be generated

OS=$(uname -s)

TOOL_COMMAND_LINE="$TOOL_OUTPUT_DIR/SC-${TOOL}-${OS}"

# Rebuild the command if it doesn't exist or if no action has been specified
if [ -z "$5" ] || [ ! -e "${TOOL_COMMAND_LINE}" ]; then

mkdir -p "$TOOL_OUTPUT_DIR"
echo "Building SC-${TOOL}.cpp..."
if [ "$OS" = "Darwin" ]; then
clang -std=c++14 -fno-exceptions -nostdlib++ -framework CoreServices "$TOOL_SOURCE_DIR/SC-${TOOL}.cpp" "$LIBRARY_DIR/Tools/Tools.cpp" -o "${TOOL_COMMAND_LINE}"
elif [ "$OS" = "Linux" ]; then
g++ -std=c++14 -fno-exceptions "$TOOL_SOURCE_DIR/SC-${TOOL}.cpp" "$LIBRARY_DIR/Tools/Tools.cpp" -o "${TOOL_COMMAND_LINE}"
else
    { echo "Unsupported operating system: $OS" ; exit 1; }
fi
if [ $? -ne 0 ]; then
    { echo "Error: Compilation failed." ; exit 1; }
fi

fi

"${TOOL_COMMAND_LINE}" $*