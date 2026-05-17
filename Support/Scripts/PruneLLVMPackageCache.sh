#!/usr/bin/env bash
set -euo pipefail

repo_root="${1:-.}"

prune_root() {
  local root="$1"

  if [[ ! -d "${root}" ]]; then
    return 0
  fi

  echo "Pruning LLVM package root: ${root}"

  if [[ -d "${root}/bin" ]]; then
    find "${root}/bin" -mindepth 1 -maxdepth 1 | while IFS= read -r path; do
      local name
      name="$(basename "${path}")"
      case "${name}" in
        clang|clang++|clang-20|ld.lld|lld|llvm-ar|llvm-lib|llvm-ranlib)
          ;;
        *)
          rm -rf "${path}"
          ;;
      esac
    done
  fi

  if [[ -d "${root}/lib" ]]; then
    find "${root}/lib" -mindepth 1 -maxdepth 1 ! -name clang -exec rm -rf {} +
  fi

  rm -rf \
    "${root}/include" \
    "${root}/libexec" \
    "${root}/share"

  du -sh "${root}" || true
}

for root in \
  "${repo_root}"/_Build/_PackagesCache/llvm/*_extracted \
  "${repo_root}"/_Build/_Packages/llvm_*
do
  if [[ -e "${root}" ]]; then
    prune_root "${root}"
  fi
done
