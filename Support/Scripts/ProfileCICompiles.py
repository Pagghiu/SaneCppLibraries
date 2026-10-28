#!/usr/bin/env python3
# Copyright (c) Stefano Cristiano
# SPDX-License-Identifier: MIT

"""Replay selected CI compiles into private outputs with Clang phase diagnostics.

Opt-in diagnostics, not a build cache or a substitute for the normal test build.
The Xcode mode uses emitted build commands, including flags outside response files;
the Fil-C mode uses the native compilation database.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import time


SOURCES = [
    "Tests/Libraries/Async/AsyncTest.cpp",
    "Tests/Libraries/Http/HttpAsyncClientTest.cpp",
    "Tests/Libraries/HttpClient/HttpClientTest.cpp",
    "Tests/Libraries/Async/AsyncContractTest.cpp",
    "Libraries/Async/Async.cpp",
    "Libraries/Strings/Strings.cpp",
]


def redirected_command(entry, output):
    """Keep the emitted compile contract while redirecting its side effects."""
    args = list(entry["arguments"]) if "arguments" in entry else shlex.split(entry["command"])
    for flag in ("-o", "-MF", "-MT"):
        if flag not in args:
            raise ValueError(f"Compilation database entry is missing {flag}")
        args[args.index(flag) + 1] = str(output) + (".d" if flag == "-MF" else "")
    for flag in ("-MJ", "-gen-cdb-fragment-path", "--serialize-diagnostics"):
        if flag in args:
            args[args.index(flag) + 1] = str(output) + "." + flag.lstrip("-")
    return args


def xcode_commands(root, build_log, output):
    commands = []
    for line in build_log.read_text().splitlines():
        # Xcode prints the fully quoted compiler invocation on its own line.
        # Ignore scan wrappers and progress descriptions.
        line = line.strip()
        if "/usr/bin/clang -x c++ " not in line or not line.startswith("/"):
            continue
        args = shlex.split(line)
        expanded = []
        for arg in args:
            if arg.startswith("@"):
                expanded.extend(shlex.split(Path(arg[1:]).read_text()))
            else:
                expanded.append(arg)
        if "-c" in expanded and "-target" in expanded:
            commands.append(expanded)
    for arch in ("arm64", "x86_64"):
        for source in SOURCES:
            matches = [args for args in commands if str(root / source) in args
                       and args[args.index("-target") + 1].startswith(arch + "-apple-")]
            if len(matches) != 1:
                raise ValueError(f"Expected one emitted compile for {arch} {source}, found {len(matches)}")
            obj = output / (Path(source).stem + "-" + arch + ".o")
            yield arch, source, redirected_command({"arguments": matches[0]}, obj), root


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("xcode", "filc"))
    parser.add_argument("--intermediate", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--build-log", type=Path, help="Xcode output from the normal compile step")
    options = parser.parse_args()
    root = Path.cwd().resolve()
    output = options.output.resolve()
    # Refuse existing directories so profiles never overwrite prior evidence or
    # any objects from the normal build. All compiler outputs live below here.
    output.mkdir(parents=True, exist_ok=False)
    rows = []
    if options.mode == "xcode":
        if options.build_log is None:
            raise ValueError("Xcode profiles require --build-log")
        commands = xcode_commands(root, options.build_log, output)
    else:
        database = json.loads((options.intermediate / "compile_commands.json").read_text())
        commands = []
        for source in SOURCES:
            matches = [e for e in database if Path(e["file"]).resolve() == root / source]
            if len(matches) != 1:
                raise ValueError(f"Expected one compile command for {source}")
            obj = output / (Path(source).stem + "-filc.o")
            commands.append(("filc", source, redirected_command(matches[0], obj), Path(matches[0]["directory"])))
    identity = {
        "revision": os.environ.get("GITHUB_SHA") or subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True).strip(),
        "mode": options.mode,
        "diagnostic_only": True,
        "sequential": True,
        "source_hashes": {s: hashlib.sha256((root / s).read_bytes()).hexdigest() for s in SOURCES},
    }
    (output / "identity.json").write_text(json.dumps(identity, indent=2) + "\n")
    compiler_versions = {}
    for arch, source, args, directory in commands:
        if args[0] not in compiler_versions:
            compiler_versions[args[0]] = subprocess.check_output([args[0], "--version"], text=True)
            identity["compilers"] = compiler_versions
            (output / "identity.json").write_text(json.dumps(identity, indent=2) + "\n")
        name = Path(source).stem + "-" + arch
        obj = output / (name + ".o")
        args += ["-ftime-trace=" + str(output / (name + ".json")), "-ftime-report"]
        (output / (name + "-command.json")).write_text(json.dumps(args, indent=2) + "\n")
        source_hash = hashlib.sha256((root / source).read_bytes()).hexdigest()
        start = time.monotonic()
        with (output / (name + ".log")).open("w") as log:
            result = subprocess.run(args, cwd=directory, stdout=log, stderr=log,
                                    env=dict(os.environ, CCACHE_DISABLE="1"))
        row = dict(source=source, architecture=arch, seconds=time.monotonic() - start,
                   exit=result.returncode, source_sha256=source_hash)
        if result.returncode == 0:
            events = json.loads((output / (name + ".json")).read_text())["traceEvents"]
            row["totals"] = {e["name"]: e["dur"] / 1e6 for e in events if e["name"].startswith("Total ")}
            row["top_functions"] = [
                dict(name=e.get("args", {}).get("detail"), seconds=e["dur"] / 1e6)
                for e in sorted(events, key=lambda e: -e.get("dur", 0)) if e["name"] == "OptFunction"
            ][:20]
            # A cliff can occur in template/constant evaluation or an optimizer
            # pass as well as code generation. Keep non-envelope events too.
            envelopes = {"ExecuteCompiler", "Frontend", "Backend", "Optimizer", "CodeGenPasses", "OptModule"}
            row["top_events"] = [
                dict(name=e["name"], detail=e.get("args", {}).get("detail"), seconds=e["dur"] / 1e6)
                for e in sorted(events, key=lambda e: -e.get("dur", 0))
                if "dur" in e and not e["name"].startswith("Total ") and e["name"] not in envelopes
            ][:40]
        rows.append(row)
        (output / "results.json").write_text(json.dumps(rows, indent=2) + "\n")
        print(f"{arch} {source}: {row['seconds']:.3f}s, exit {result.returncode}", flush=True)
        if result.returncode:
            raise SystemExit(result.returncode)
        obj.unlink()
        if hashlib.sha256((root / source).read_bytes()).hexdigest() != source_hash:
            raise RuntimeError(f"Source changed during profiling: {source}")


if __name__ == "__main__":
    main()
