#!/bin/sh
SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" 2>/dev/null && pwd)
"${SCRIPT_DIR}/Tools/Tools.sh" "$SCRIPT_DIR" "$SCRIPT_DIR" "$SCRIPT_DIR/_Build" $*