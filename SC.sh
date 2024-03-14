#!/bin/bash
SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" 2>/dev/null && pwd)
"${SCRIPT_DIR}/Tools/Tools.sh" "$1" "$SCRIPT_DIR" "$SCRIPT_DIR" "$SCRIPT_DIR" ${@:2}