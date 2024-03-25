#!/bin/sh
LIBRARY_DIR="$1"        # Directory where "Libraries" exists
TOOL_SOURCE_DIR="$2"    # Directory where SC-${TOOL}.cpp file exists
BUILD_DIR="$3"          # Directory where output subdirectories must be placed
TOOL_NAME="${4:-build}"      # Tool to execute (build by default)

TOOL_OUTPUT_DIR="${BUILD_DIR}/_Tools"   # Directory where the ${TOOL} executable will be generated

TOOL=SC-${TOOL_NAME}
if [ ! -e "${TOOL_SOURCE_DIR}/${TOOL}.cpp" ]; then # Try looking in the TOOL_SOURCE_DIR
    if [ ! -e "${TOOL_NAME}" ]; then
        { echo "Error: Tool \"${TOOL_NAME}\" doesn't exist" ; exit 1; }
    fi
    TOOL_NAME=$(readlink -f "${TOOL_NAME}")
    filename=$(basename -- "$TOOL_NAME")
    TOOL="${filename%.*}" # just name without extension
    TOOL_SOURCE_DIR=$(dirname -- "$TOOL_NAME")
    extension="${filename##*.}"
    if [ "$extension" != "cpp" ]; then
        { echo "Error: ${extension} Tool \"${TOOL_NAME}\" doesn't end with .cpp" ; exit 1; }
    fi
fi
make -s -j --no-print-directory -C "${LIBRARY_DIR}/Tools/Build/Posix" CONFIG=Debug "TOOL=${TOOL}" "TOOL_SOURCE_DIR=${TOOL_SOURCE_DIR}" "TOOL_OUTPUT_DIR=${TOOL_OUTPUT_DIR}"

if [ $? -eq 0 ]; then
echo "Starting ${TOOL}"
OS=$(uname -s)
"$TOOL_OUTPUT_DIR/${OS}/${TOOL}" $*
else
exit $?
fi