#!/bin/bash
COMMAND="${1:-build}"   # Command to execute (build by default)
LIBRARY_DIR="$2"        # Directory where "Libraries" exists
COMMAND_DIR="$3"        # Directory where SC-${COMMAND}.cpp file exists
OUTPUT_DIR="$4/_Build"  # Directory where output subdirectories must be placed

CUSTOM_PARAMETERS=${@:5}

OUTPUT_COMMANDS_DIR="${OUTPUT_DIR}/_Commands"   # Directory where the ${COMMAND} executable will be generated

OS=$(uname -s)

FINAL_COMMAND_PATH="$OUTPUT_COMMANDS_DIR/SC-${COMMAND}-${OS}"

# Rebuild the command if it doesn't exist or if no action has been specified
if [ -z "$5" ] || [ ! -e "${FINAL_COMMAND_PATH}" ]; then

mkdir -p "$OUTPUT_COMMANDS_DIR"
echo "Building SC-${COMMAND}.cpp..."
if [ "$OS" = "Darwin" ]; then
clang -std=c++14 -fno-exceptions -nostdlib++ -framework CoreServices "$COMMAND_DIR/SC-${COMMAND}.cpp" "$LIBRARY_DIR/Tools/Tools.cpp" -o "${FINAL_COMMAND_PATH}"
elif [ "$OS" = "Linux" ]; then
g++ -std=c++14 -fno-exceptions "$COMMAND_DIR/SC-${COMMAND}.cpp" "$LIBRARY_DIR/Tools/Tools.cpp" -o "${FINAL_COMMAND_PATH}"
else
    { echo "Unsupported operating system: $OS" ; exit 1; }
fi
if [ $? -ne 0 ]; then
    { echo "Error: Compilation failed." ; exit 1; }
fi

fi

"${FINAL_COMMAND_PATH}" "${LIBRARY_DIR}" "${OUTPUT_DIR}" ${CUSTOM_PARAMETERS[@]}
