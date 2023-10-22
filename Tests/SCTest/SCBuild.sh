#!/bin/bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
BUILD_DIR=${SCRIPT_DIR}/../../_Build/Build
echo "Building SCBuild.cpp..."
mkdir -p "${BUILD_DIR}" && \
clang -std=c++14 -nostdlib++ -fno-exceptions -framework CoreServices "${SCRIPT_DIR}/SCBuild.cpp" "${SCRIPT_DIR}/../../Bindings/cpp/SC.cpp"  -o "${BUILD_DIR}/scbuild" && \
"${BUILD_DIR}/scbuild" --target ${SCRIPT_DIR}/Build --sources ${SCRIPT_DIR}/../..