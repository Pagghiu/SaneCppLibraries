#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
ROOT_DIR="${SCRIPT_DIR}/.."
HOST_TOOLS="${ROOT_DIR}/_Build/HostTools"
CLANG_FORMAT="$HOST_TOOLS/clang/bin/clang-format"
cd "${SCRIPT_DIR}/.." && find . -iname \*.h -o -iname \*.cpp -o -iname \*.mm -o -iname \*.m | xargs "${CLANG_FORMAT}" -i
