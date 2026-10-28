#!/usr/bin/env python3
"""Remove known obsolete Fil-C copies after validating a cache-backed installation."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil


def tree_bytes(root):
    total = 0
    seen = set()
    for directory, _, files in os.walk(root, followlinks=False):
        for name in files:
            info = (Path(directory) / name).lstat()
            identity = (info.st_dev, info.st_ino)
            if identity not in seen:
                seen.add(identity)
                total += info.st_size
    return total


def prune(repo, architecture):
    cache = repo / "_Build/_PackagesCache/filc"
    installed = repo / "_Build/_Packages" / ("filc_linux_" + architecture)
    with (installed / "sc-filc-package.json").open(encoding="utf-8") as source:
        metadata = json.load(source)
    version = metadata["version"]
    if not re.fullmatch(r"\d+(?:\.\d+)+", version) or metadata["flavor"] != "pizfix":
        raise ValueError("Unexpected Fil-C version/flavor; cache was not pruned")
    active_name = "pizfix-" + version + "-linux-" + architecture
    active = cache / active_name
    if not active.is_dir() or installed.resolve() != active.resolve():
        return {"skipped": "Installation is not backed by the expected Fil-C cache"}
    if not (active / "sc-filc/bin/clang").is_file():
        raise ValueError("Prepared compiler is missing; cache was not pruned")
    before = tree_bytes(cache)
    removed = []
    for path in sorted(cache.iterdir()):
        old_package = re.fullmatch(r"pizfix-\d+(?:\.\d+)+-linux-" + architecture + r"(?:\.txt)?", path.name)
        archive = re.fullmatch(r"filc-\d+(?:\.\d+)+-linux-" + architecture + r"\.tar\.xz", path.name)
        if path.name in (active_name, active_name + ".txt"):
            continue
        if old_package or archive:
            if path.is_dir() and not path.is_symlink():
                shutil.rmtree(path)
            else:
                path.unlink()
            removed.append(path.name)
    return {"version": version, "architecture": architecture, "removed": removed,
            "logical_bytes_before": before, "logical_bytes_after": tree_bytes(cache)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--architecture", choices=("aarch64", "x86_64"), required=True)
    arguments = parser.parse_args()
    print(json.dumps(prune(arguments.repo, arguments.architecture), indent=2))


if __name__ == "__main__":
    main()
