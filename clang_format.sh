#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PATH=$PATH:/opt/homebrew/bin/
cd ${SCRIPT_DIR} && find . -iname \*.h -o -iname \*.cpp -o -iname \*.mm -o -iname \*.m | xargs clang-format -i
