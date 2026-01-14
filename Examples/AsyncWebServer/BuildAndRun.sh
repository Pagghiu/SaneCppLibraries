#!/bin/sh
SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" 2>/dev/null && pwd)
cd "$SCRIPT_DIR"
if [ "$(uname)" = "Darwin" ]; then
    EXTRA_LIBS="-framework CoreFoundation -framework Cocoa"
else
    EXTRA_LIBS="-lc -lm"
fi
mkdir -p _Build && cd _Build
echo "Building and running AsyncWebServer example (DEBUG)..."
c++ ../AsyncWebServer.cpp ../../../SC.cpp -o AsyncWebServer -nostdlib++ -fno-rtti -fno-exceptions -std=c++14 -D_DEBUG=1 -g -ggdb -O0 $EXTRA_LIBS && cd .. && ./_Build/AsyncWebServer "$@"
