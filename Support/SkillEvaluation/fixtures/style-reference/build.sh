#!/bin/sh
set -eu
: "${SC_SKILL_EVAL_BUILD_DIR:?SC_SKILL_EVAL_BUILD_DIR must point below _Build}"
mkdir -p "$SC_SKILL_EVAL_BUILD_DIR"
c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -fno-exceptions -fno-rtti \
    event_log.cpp event_log_test.cpp -o "$SC_SKILL_EVAL_BUILD_DIR/event_log_test"
