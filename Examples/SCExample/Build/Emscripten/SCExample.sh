#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PATH=$PATH:/opt/homebrew/bin/
ROOT_DIR="${SCRIPT_DIR}/../../../.."
cd "${ROOT_DIR}"

source _Build/HostTools/emsdk/emsdk_env.sh

POSITIONAL_ARGS=()

GCC_DEBUG_FLAG=-g
GCC_CONFIGURATION=Debug
GCC_DIRECTORY=gcc-generic

while [[ $# -gt 0 ]]; do
  case $1 in
    -r|--release)
      GCC_DEBUG_FLAG="-O2"
      GCC_CONFIGURATION="Release"
      shift # past argument
      ;;
    -d|--debug)
      GCC_DEBUG_FLAG="-g"
      GCC_CONFIGURATION="Debug"
      shift # past argument
      ;;
    -*|--*)
      echo "Unknown option $1"
      exit 1
      ;;
    *)
      POSITIONAL_ARGS+=("$1") # save positional arg
      GCC_DIRECTORY="$1"
      shift # past argument
      ;;
  esac
done
rm -rf "_Build/Output/${GCC_DIRECTORY}-${GCC_CONFIGURATION}/SCExample"
mkdir -p "_Build/Output/${GCC_DIRECTORY}-${GCC_CONFIGURATION}/SCExample"

set -- "${POSITIONAL_ARGS[@]}" # restore positional parameters

time em++ -std=c++20 -nostdinc++  -fno-rtti -fno-exceptions ${GCC_DEBUG_FLAG} \
-I${SCRIPT_DIR}/../../ \
-o _Build/Output/${GCC_DIRECTORY}-${GCC_CONFIGURATION}/SCExample/SCExample.html \
-s DISABLE_EXCEPTION_CATCHING=1 -s STACK_SIZE=512KB --memory-init-file 0 \
-s ERROR_ON_UNDEFINED_SYMBOLS=1 -s NO_EXIT_RUNTIME=1 \
-s ALLOW_MEMORY_GROWTH=1 -s USE_WEBGL2=1 -s "MALLOC='emmalloc'" -s NO_FILESYSTEM=1 -s WASM=1  \
-s USE_GLFW=3 \
--shell-file "${ROOT_DIR}/Libraries/UserInterface/Platform.html" \
${ROOT_DIR}/Dependencies/imgui/DependencyImgui.cpp \
${ROOT_DIR}/Libraries/UserInterface/Platform.cpp \
${SCRIPT_DIR}/../../SCExample.cpp

