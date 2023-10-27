#!/bin/sh

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" 2>/dev/null && pwd)
PROJECTS_DIR="${SCRIPT_DIR}/_Build/Projects"
GENERATOR_DIR="${SCRIPT_DIR}/_Build/Generator"
SOURCES_DIR="$SCRIPT_DIR"
"${SCRIPT_DIR}/Support/Build/Build.sh" "$GENERATOR_DIR" "$PROJECTS_DIR" "$SCRIPT_DIR" "$SOURCES_DIR"
