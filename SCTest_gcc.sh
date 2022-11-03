#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PATH=$PATH:/opt/homebrew/bin/
cd "${SCRIPT_DIR}"

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
rm -rf "_build/${GCC_DIRECTORY}/${GCC_CONFIGURATION}"
mkdir -p "_build/${GCC_DIRECTORY}/${GCC_CONFIGURATION}"

set -- "${POSITIONAL_ARGS[@]}" # restore positional parameters
time g++-12 -std=c++14 -nostdinc++ ${GCC_DEBUG_FLAG} \
-o _build/${GCC_DIRECTORY}/${GCC_CONFIGURATION}/SCTest \
libraries/foundation/Assert.cpp         \
libraries/foundation/Console.cpp        \
libraries/foundation/Memory.cpp         \
libraries/foundation/OSDarwin.cpp       \
libraries/foundation/OSPosix.cpp        \
libraries/foundation/StaticAsserts.cpp  \
libraries/foundation/String.cpp         \
libraries/foundation/StringBuilder.cpp  \
libraries/foundation/StringFormat.cpp   \
libraries/foundation/StringUtility.cpp  \
libraries/foundation/StringView.cpp     \
libraries/foundation/Test.cpp           \
main.cpp