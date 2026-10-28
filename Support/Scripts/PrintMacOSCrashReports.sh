#!/usr/bin/env bash
# Copyright (c) Stefano Cristiano
# SPDX-License-Identifier: MIT
set -euo pipefail

report_directory="${1:-$HOME/Library/Logs/DiagnosticReports}"
shopt -s nullglob
reports=("$report_directory"/SC*.ips "$report_directory"/SC*.crash)
if [[ ${#reports[@]} -eq 0 ]]; then
    printf 'No SC crash reports were available when diagnostics were collected.\n'
    exit 0
fi
for report in "${reports[@]}"; do
    printf '::group::macOS crash report: %s\n' "$(basename "$report")"
    cat "$report"
    printf '\n::endgroup::\n'
done
