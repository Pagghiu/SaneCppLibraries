#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PATH=$PATH:/opt/homebrew/bin/
cd ${SCRIPT_DIR}
mkdir _build/gcc
time g++-12 -std=c++14 -nostdinc++ -g -o _build/gcc/SCTest main.cpp libraries/foundation/Assert.cpp  libraries/foundation/Console.cpp  libraries/foundation/Memory.cpp  libraries/foundation/OSDarwin.cpp  libraries/foundation/OSPosix.cpp  libraries/foundation/StaticAsserts.cpp  libraries/foundation/String.cpp  libraries/foundation/StringBuilder.cpp  libraries/foundation/StringFormat.cpp  libraries/foundation/StringUtility.cpp  libraries/foundation/StringView.cpp  libraries/foundation/Test.cpp 