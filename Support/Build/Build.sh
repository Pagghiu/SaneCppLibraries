#!/bin/sh

GENERATOR_DIR="$1"  # Directory where the build system executable will be generated
PROJECTS_DIR="$2"   # Directory where to generate projects
SCRIPT_DIR="$3"     # Directory where SCBuild.cpp file exists
SOURCES_DIR="$4"    # Directory where "Libraries" exists

mkdir -p "$GENERATOR_DIR"

echo "Building SCBuild.cpp..."

clang -std=c++14 -nostdlib++ -fno-exceptions -framework CoreServices "$SCRIPT_DIR/SCBuild.cpp" "$SOURCES_DIR/Support/Build/BuildBootstrap.cpp" -o "$GENERATOR_DIR/SCBuild"

if [ $? -ne 0 ]; then
    echo "Error: Compilation failed."
    exit 1
fi

"$GENERATOR_DIR/SCBuild" --target "$PROJECTS_DIR" --sources "$SOURCES_DIR"
