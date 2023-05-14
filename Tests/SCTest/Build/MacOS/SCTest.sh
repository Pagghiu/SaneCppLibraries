#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PATH=$PATH:/opt/homebrew/bin/
cd "${SCRIPT_DIR}/../../../.."


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
rm -rf "_Build/Output/${GCC_DIRECTORY}-${GCC_CONFIGURATION}"
mkdir -p "_Build/Output/${GCC_DIRECTORY}-${GCC_CONFIGURATION}"

set -- "${POSITIONAL_ARGS[@]}" # restore positional parameters
time g++-12 -std=c++14 -nostdinc++ -fno-rtti -fno-exceptions ${GCC_DEBUG_FLAG} \
-I${SCRIPT_DIR}/../../ \
-framework CoreFoundation \
-framework CoreServices \
-o _Build/Output/${GCC_DIRECTORY}-${GCC_CONFIGURATION}/SCTest \
Libraries/Async/EventLoop.cpp               \
Libraries/File/FileDescriptor.cpp           \
Libraries/FileSystem/FileSystem.cpp         \
Libraries/FileSystem/FileSystemWalker.cpp   \
Libraries/FileSystem/FileSystemWatcher.cpp  \
Libraries/FileSystem/Path.cpp               \
Libraries/Foundation/Memory.cpp             \
Libraries/Foundation/String.cpp             \
Libraries/Foundation/StringConverter.cpp    \
Libraries/Foundation/StringFormat.cpp       \
Libraries/Foundation/StringView.cpp         \
Libraries/Process/Process.cpp               \
Libraries/Socket/SocketDescriptor.cpp       \
Libraries/System/Console.cpp                \
Libraries/System/System.cpp                 \
Libraries/System/Time.cpp                   \
Libraries/Threading/Threading.cpp           \
Libraries/Testing/Test.cpp                  \
${SCRIPT_DIR}/../../SCTest.cpp