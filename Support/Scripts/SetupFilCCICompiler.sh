#!/usr/bin/env bash
set -euo pipefail

expected_version=${1:?Expected Fil-C version}
if [[ ! "$expected_version" =~ ^[0-9]+(\.[0-9]+)+$ ]]; then
    echo "Invalid Fil-C version" >&2
    exit 2
fi
case "$(uname -s):$(uname -m)" in
    Linux:aarch64|Linux:x86_64) architecture=$(uname -m) ;;
    *) echo "Fil-C CI requires native Linux ARM64 or x86_64" >&2; exit 2 ;;
esac
repo_root=$(cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$repo_root"
compiler="_Build/_Packages/filc_linux_${architecture}/sc-filc/bin/clang"
# A restored, prepared compiler needs no system package installation.
if ! command -v patchelf >/dev/null &&
   ! "$compiler" --version 2>/dev/null | grep -F "Fil-C ${expected_version} " >/dev/null; then
    sudo apt-get update
    sudo apt-get install -y patchelf
fi
./SC.sh package install filc
python3 - "_Build/_Packages/filc_linux_${architecture}/sc-filc-package.json" "$expected_version" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as source:
    metadata = json.load(source)
if metadata["version"] != sys.argv[2] or metadata["flavor"] != "pizfix":
    raise SystemExit("The installed Fil-C compiler does not match the CI version/flavor")
print("Fil-C CI compiler:", metadata["version"], metadata["targetTriple"])
PY
