#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd -P)
readonly SCRIPT_DIR
readonly REPOSITORY_URL="https://github.com/Pagghiu/SaneCppLibraries.git"

die() {
    printf '%s\n' "Error: $*" >&2
    exit 1
}

warn() {
    printf '%s\n' "Warning: $*" >&2
}

has_local_libraries_root() {
    local root=$1
    [[ -f "$root/SC.cpp" && -f "$root/SC.sh" && -f "$root/Tools/ToolsBootstrap.c" ]]
}

canonicalize_dir() {
    local directory=$1
    [[ -d "$directory" ]] || die "Directory \"$directory\" does not exist"
    (cd "$directory" >/dev/null 2>&1 && pwd -P)
}

canonicalize_file() {
    local path=$1
    local directory
    directory=$(dirname -- "$path")
    local name
    name=$(basename -- "$path")
    directory=$(canonicalize_dir "$directory")
    printf '%s/%s\n' "$directory" "$name"
}

resolve_project_root() {
    local start_dir=$1
    local search_dir
    search_dir=$(canonicalize_dir "$start_dir")
    while true; do
        if [[ -f "$search_dir/SC-build.cpp" ]]; then
            printf '%s\n' "$search_dir"
            return 0
        fi
        local parent
        parent=$(dirname -- "$search_dir")
        [[ "$parent" != "$search_dir" ]] || break
        search_dir=$parent
    done
    return 1
}

read_requested_git_ref() {
    local project_file=$1
    awk '
        /^[[:space:]]*\/\/[[:space:]]*sc-build-version:[[:space:]]*/ {
            sub(/^[[:space:]]*\/\/[[:space:]]*sc-build-version:[[:space:]]*/, "", $0);
            sub(/[[:space:]]+$/, "", $0);
            print;
            exit;
        }
    ' "$project_file"
}

default_cache_base() {
    if [[ -n "${SC_BUILD_CACHE_DIR:-}" ]]; then
        printf '%s\n' "$SC_BUILD_CACHE_DIR"
        return 0
    fi
    if [[ -n "${XDG_CACHE_HOME:-}" ]]; then
        printf '%s/sc-build\n' "$XDG_CACHE_HOME"
        return 0
    fi
    printf '%s/.cache/sc-build\n' "$HOME"
}

ensure_git_available() {
    command -v git >/dev/null 2>&1 || die "\"git\" is required to resolve the shared SaneCppLibraries installation"
}

ensure_shared_repository_checkout() {
    local cache_base=$1
    local repository_root=$cache_base/repository
    mkdir -p "$cache_base"
    ensure_git_available
    if [[ ! -d "$repository_root/.git" ]]; then
        printf '%s\n' "Cloning SaneCppLibraries into $repository_root" >&2
        git clone "$REPOSITORY_URL" "$repository_root" >/dev/null
    else
        git -C "$repository_root" fetch --prune --tags origin >/dev/null
    fi
    git -C "$repository_root" remote set-head origin -a >/dev/null 2>&1 || true
    printf '%s\n' "$repository_root"
}

resolve_requested_commit() {
    local repository_root=$1
    local requested_ref=$2
    local commit

    commit=$(git -C "$repository_root" rev-parse --verify "${requested_ref}^{commit}" 2>/dev/null) && {
        printf '%s\n' "$commit"
        return 0
    }
    commit=$(git -C "$repository_root" rev-parse --verify "refs/remotes/origin/${requested_ref}^{commit}" 2>/dev/null) && {
        printf '%s\n' "$commit"
        return 0
    }
    commit=$(git -C "$repository_root" rev-parse --verify "refs/tags/${requested_ref}^{commit}" 2>/dev/null) && {
        printf '%s\n' "$commit"
        return 0
    }
    return 1
}

resolve_shared_libraries_root() {
    local project_root=$1
    local cache_base
    cache_base=$(default_cache_base)
    cache_base=$(canonicalize_dir "$(dirname "$cache_base")")/$(basename "$cache_base")
    mkdir -p "$cache_base"

    local repository_root
    repository_root=$(ensure_shared_repository_checkout "$cache_base")

    local project_file=$project_root/SC-build.cpp
    local requested_ref
    requested_ref=$(read_requested_git_ref "$project_file" || true)
    if [[ -z "$requested_ref" ]]; then
        requested_ref=$(git -C "$repository_root" symbolic-ref --quiet --short refs/remotes/origin/HEAD 2>/dev/null || printf '%s\n' origin/main)
        warn "Missing \"// sc-build-version: <git-ref>\" in $project_file, using $requested_ref. Add the pragma near the top of SC-build.cpp for reproducible shared-cache builds, or pass --libraries-root <path> while developing locally."
    fi

    local commit
    commit=$(resolve_requested_commit "$repository_root" "$requested_ref") ||
        die "Cannot resolve SaneCppLibraries revision \"$requested_ref\""

    local worktree_root=$cache_base/worktrees/$commit
    if [[ -d "$worktree_root" && ! -f "$worktree_root/SC.sh" ]]; then
        rm -rf "$worktree_root"
    fi
    if [[ ! -d "$worktree_root" ]]; then
        mkdir -p "$cache_base/worktrees"
        git -C "$repository_root" worktree add --detach "$worktree_root" "$commit" >/dev/null
    fi
    printf '%s\n' "$worktree_root"
}

ensure_bootstrap_executable() {
    local libraries_root=$1
    has_local_libraries_root "$libraries_root" || die "\"$libraries_root\" is not a valid SaneCppLibraries checkout"

    local platform
    platform=$(uname)
    local bootstrap_exe=$libraries_root/_Build/_Tools/$platform/ToolsBootstrap
    mkdir -p "$libraries_root/_Build/_Tools/$platform"
    if [[ ! -f "$bootstrap_exe" || "$libraries_root/Tools/ToolsBootstrap.c" -nt "$bootstrap_exe" ]]; then
        printf '%s\n' "ToolsBootstrap.c" >&2
        if command -v cc >/dev/null 2>&1; then
            cc -o "$bootstrap_exe" "$libraries_root/Tools/ToolsBootstrap.c" -std=c99 -D_DEBUG=1 -g -ggdb -O0
        else
            gcc -o "$bootstrap_exe" "$libraries_root/Tools/ToolsBootstrap.c" -std=c99 -D_DEBUG=1 -g -ggdb -O0
        fi
    fi
    printf '%s\n' "$bootstrap_exe"
}

project_dir_override=
libraries_root_override=${SC_BUILD_LIBRARIES_ROOT:-}
forwarded_args=()

while (($# > 0)); do
    case $1 in
    --project-dir)
        (($# >= 2)) || die "Missing value after --project-dir"
        project_dir_override=$2
        shift 2
        ;;
    --project-dir=*)
        project_dir_override=${1#*=}
        shift
        ;;
    --libraries-root)
        (($# >= 2)) || die "Missing value after --libraries-root"
        libraries_root_override=$2
        shift 2
        ;;
    --libraries-root=*)
        libraries_root_override=${1#*=}
        shift
        ;;
    *)
        forwarded_args+=("$1")
        shift
        ;;
    esac
done

if [[ -n "$project_dir_override" ]]; then
    project_root=$(canonicalize_dir "$project_dir_override")
else
    project_root=$(resolve_project_root "$PWD") ||
        die "Cannot find SC-build.cpp by searching upward from \"$PWD\". Use --project-dir <path> to select a project."
fi

project_file=$(canonicalize_file "$project_root/SC-build.cpp")

if [[ -n "$libraries_root_override" ]]; then
    libraries_root=$(canonicalize_dir "$libraries_root_override")
elif has_local_libraries_root "$SCRIPT_DIR"; then
    libraries_root=$SCRIPT_DIR
else
    libraries_root=$(resolve_shared_libraries_root "$project_root")
fi

bootstrap_exe=$(ensure_bootstrap_executable "$libraries_root")
build_root=$project_root/_Build
mkdir -p "$build_root"

bootstrap_args=(
    "$libraries_root"
    "$libraries_root/Tools"
    "$build_root"
    "$project_root"
    "$project_file"
)
if ((${#forwarded_args[@]} > 0)); then
    bootstrap_args+=("${forwarded_args[@]}")
fi

"$bootstrap_exe" "${bootstrap_args[@]}"
