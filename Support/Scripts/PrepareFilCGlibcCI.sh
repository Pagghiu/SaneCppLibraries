#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(cd -- "$SCRIPT_DIR/../.." && pwd)
MODE=${1:---install}
if [[ $# -gt 1 || "$MODE" != --install && "$MODE" != --download-only ]]; then
    echo "Usage: $0 [--install|--download-only]" >&2
    exit 2
fi

case "$(uname -s):$(uname -m)" in
    Linux:aarch64|Linux:arm64)
        ARCH=aarch64
        SHA256=e71ea1cffe28c63f7947c9bf2a0d9f39b70c742473561b317b3ebad81b7f980b
        ;;
    Linux:x86_64)
        ARCH=x86_64
        SHA256=f7d2d73b17ee9bfff0859ca15b3ca9c10e8501f8d891b0dfa961e81f0390e788
        ;;
    *) echo "Expected native Linux ARM64 or x86_64" >&2; exit 2 ;;
esac

if [[ "$MODE" == --install ]]; then
    if [[ "${GITHUB_ACTIONS:-}" != true || "${RUNNER_ENVIRONMENT:-}" != github-hosted ]]; then
        echo "Installation is limited to disposable GitHub-hosted runners" >&2
        exit 2
    fi
    if [[ -e /opt/fil || -L /opt/fil ]]; then
        echo "Refusing to overwrite an existing /opt/fil installation" >&2
        exit 2
    fi
fi

VERSION=0.685
NAME="optfil-$VERSION-linux-$ARCH"
CACHE="$REPO_ROOT/_Build/_PackagesCache/filc-glibc"
ARCHIVE="$CACHE/$NAME.tar.xz"
mkdir -p "$CACHE"
if [[ ! -f "$ARCHIVE" ]]; then
    curl --fail --location --output "$ARCHIVE.part" \
        "https://github.com/pizlonator/fil-c/releases/download/v$VERSION/$NAME.tar.xz"
    mv "$ARCHIVE.part" "$ARCHIVE"
fi
printf '%s  %s\n' "$SHA256" "$ARCHIVE" | sha256sum --check --strict
if [[ "$MODE" == --download-only ]]; then
    exit 0
fi

PAYLOAD=$(mktemp -d)
trap 'rm -rf "$PAYLOAD"' EXIT
# Only extract the payload, never run upstream SSH/system setup or change PATH.
tar -xJf "$ARCHIVE" -C "$PAYLOAD" "$NAME/fil.tar.xz"
sudo tar -xJf "$PAYLOAD/$NAME/fil.tar.xz" -C /opt --no-same-owner
/opt/fil/bin/fil++ --version
test -f /opt/fil/lib/libc++abi.so.1
