#!/bin/sh
set -eu
: "${SC_SKILL_EVAL_BUILD_DIR:?SC_SKILL_EVAL_BUILD_DIR must point below _Build}"
mkdir -p "$SC_SKILL_EVAL_BUILD_DIR"
cp slots.py "$SC_SKILL_EVAL_BUILD_DIR/slots"
chmod +x "$SC_SKILL_EVAL_BUILD_DIR/slots"
