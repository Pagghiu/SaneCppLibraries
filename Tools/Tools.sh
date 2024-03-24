#!/bin/sh
LIBRARY_DIR="$1"        # Directory where "Libraries" exists
TOOL_SOURCE_DIR="$2"    # Directory where SC-${TOOL}.cpp file exists
BUILD_DIR="$3"          # Directory where output subdirectories must be placed
TOOL="${4:-build}"      # Tool to execute (build by default)

TOOL_OUTPUT_DIR="${BUILD_DIR}/_Tools"   # Directory where the ${TOOL} executable will be generated

make -s -j --no-print-directory -C "${LIBRARY_DIR}/Tools/Build/Posix" CONFIG=Debug "TOOL=SC-${TOOL}" "TOOL_SOURCE_DIR=${TOOL_SOURCE_DIR}" "TOOL_OUTPUT_DIR=${TOOL_OUTPUT_DIR}"

echo "Starting SC-${TOOL}..."
OS=$(uname -s)
"$TOOL_OUTPUT_DIR/${OS}/SC-${TOOL}" $*
