#!/usr/bin/env python3
"""Run executable acceptance checks without consulting condition identity."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import tempfile
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
EVALUATION = ROOT / "Support" / "SkillEvaluation"


def command(args: list[str], cwd: Path, environment=None, timeout_seconds: float = 60) -> dict:
    started = time.monotonic()
    try:
        result = subprocess.run(args, cwd=cwd, env=environment, capture_output=True, text=True, timeout=timeout_seconds)
        return {
            "argv": args,
            "returncode": result.returncode,
            "stdout": result.stdout,
            "stderr": result.stderr,
            "elapsed_seconds": time.monotonic() - started,
            "timed_out": False,
        }
    except subprocess.TimeoutExpired as error:
        def output_text(value: bytes | str | None) -> str:
            return value.decode(errors="replace") if isinstance(value, bytes) else (value or "")

        return {
            "argv": args,
            "returncode": None,
            "stdout": output_text(error.stdout),
            "stderr": output_text(error.stderr),
            "elapsed_seconds": time.monotonic() - started,
            "timed_out": True,
        }


def build(submission: Path, build_directory: Path) -> tuple[bool, dict]:
    script = submission / "build.sh"
    if not script.is_file():
        return False, {"error": "missing build.sh"}
    build_directory.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment["SC_SKILL_EVAL_BUILD_DIR"] = str(build_directory)
    result = command(["/bin/sh", str(script.resolve())], submission, environment)
    return result["returncode"] == 0, result


def bounded_traversal(submission: Path, build_directory: Path) -> list[dict]:
    executable = build_directory / "manifest"
    if not executable.is_file():
        return [{"name": "manifest-produced", "passed": False, "evidence": f"build did not produce {executable}"}]
    with tempfile.TemporaryDirectory(dir=build_directory, prefix="traversal-input-") as temporary:
        root = Path(temporary)
        (root / "a").mkdir()
        (root / "a" / "one.txt").write_bytes(b"one")
        (root / "two.bin").write_bytes(b"hello")
        outcome = command([str(executable), str(root), "8"], submission)
        try:
            records = [json.loads(line) for line in outcome["stdout"].splitlines() if line]
        except json.JSONDecodeError as error:
            return [{"name": "json-output", "passed": False, "evidence": str(error)}]
        expected = [(str(root / "a" / "one.txt"), 3), (str(root / "two.bin"), 5)]
        actual = [(record.get("path"), record.get("size")) for record in records]
        return [
            {
                "name": "valid-tree",
                "passed": outcome["returncode"] == 0 and actual == expected,
                "evidence": {"returncode": outcome["returncode"], "actual": actual},
            },
            {
                "name": "invalid-depth",
                "passed": command([str(executable), str(root), "9"], submission)["returncode"] != 0,
                "evidence": "depth 9 must fail",
            },
        ]


def slot_reuse(submission: Path, build_directory: Path) -> list[dict]:
    executable = build_directory / "slots"
    if not executable.is_file():
        return [{"name": "slots-produced", "passed": False, "evidence": f"build did not produce {executable}"}]
    outcome = command([str(executable), "9", "2", "5", "2000", "--fail-index", "2"], submission, timeout_seconds=3)
    quiet = command([str(executable), "4", "2", "1500", "50"], submission, timeout_seconds=3)

    def parse_records(result: dict) -> list[dict] | None:
        try:
            records = [json.loads(line) for line in result["stdout"].splitlines() if line]
            return records if all(isinstance(record, dict) for record in records) else None
        except json.JSONDecodeError:
            return None

    records = parse_records(outcome)
    quiet_records = parse_records(quiet)
    valid = (
        outcome["returncode"] == 1
        and records is not None
        and len(records) == 9
        and [record.get("item") for record in records] == list(range(9))
        and [record.get("status") for record in records] == ["failed" if index == 2 else "ok" for index in range(9)]
    )
    generations: dict[int, list[int]] = {}
    if records is not None:
        for record in records:
            slot = record.get("slot")
            generation = record.get("generation")
            if type(slot) is int and slot in (0, 1) and type(generation) is int and generation >= 0:
                generations.setdefault(slot, []).append(generation)
    reused = (
        valid
        and sum(len(values) for values in generations.values()) == 9
        and len(generations) == 2
        and all(values == list(range(values[0], values[0] + len(values))) for values in generations.values())
        and any(len(values) > 1 for values in generations.values())
    )
    deadline = (
        quiet["returncode"] == 1
        and quiet["elapsed_seconds"] < 2
        and quiet_records is not None
        and len(quiet_records) == 4
        and [record.get("item") for record in quiet_records] == list(range(4))
        and [record.get("status") for record in quiet_records] == ["timed_out", "timed_out", "cancelled", "cancelled"]
        and all(record.get("slot") is None and record.get("generation") is None for record in quiet_records[2:])
    )
    invalid = command([str(executable), "2", "0", "5", "50"], submission, timeout_seconds=3)
    return [
        {"name": "failure-continues", "passed": valid, "evidence": {"run": outcome, "records": records}},
        {"name": "physical-slot-reuse", "passed": reused, "evidence": {"generations": generations}},
        {"name": "quiet-deadline", "passed": deadline, "evidence": {"run": quiet, "records": quiet_records}},
        {
            "name": "invalid-slots",
            "passed": invalid["returncode"] == 2 and bool(invalid["stderr"].strip()),
            "evidence": invalid,
        },
    ]


def bounded_file_copy(submission: Path, build_directory: Path) -> list[dict]:
    executable = build_directory / "copy"
    if not executable.is_file():
        return [{"name": "copy-produced", "passed": False, "evidence": f"build did not produce {executable}"}]
    with tempfile.TemporaryDirectory(dir=build_directory, prefix="file-copy-input-") as temporary:
        root = Path(temporary)
        source = root / "source binary.bin"
        destination = root / "destination binary.bin"
        payload = bytes(range(256)) * 257 + b"\x00\xff\x00"
        source.write_bytes(payload)
        checks = []
        for capacity in (1, 127, 4096):
            destination.write_bytes(b"stale" * (len(payload) + 1) if capacity == 4096 else b"stale")
            outcome = command([str(executable), str(source), str(destination), str(capacity)], submission, timeout_seconds=15)
            copied = destination.read_bytes()
            checks.append({
                "name": f"exact-binary-copy-{capacity}",
                "passed": outcome["returncode"] == 0 and copied == payload,
                "evidence": {"run": outcome, "bytes_copied": len(copied), "matches": copied == payload},
            })
        for capacity in ("0", "4097", "abc"):
            outcome = command([str(executable), str(source), str(destination), capacity], submission)
            checks.append({
                "name": f"invalid-capacity-{capacity}",
                "passed": outcome["returncode"] == 2 and bool(outcome["stderr"].strip()),
                "evidence": outcome,
            })
        destination.write_bytes(b"preserve me")
        missing = command([str(executable), str(root / "missing.bin"), str(destination), "127"], submission)
        checks.append({
            "name": "missing-source-preserves-destination",
            "passed": missing["returncode"] == 1 and bool(missing["stderr"].strip()) and destination.read_bytes() == b"preserve me",
            "evidence": missing,
        })
        return checks


def style_log(submission: Path, build_directory: Path) -> list[dict]:
    required = [submission / "event_log.h", submission / "event_log.cpp"]
    if not all(path.is_file() for path in required):
        return [{"name": "public-artifacts", "passed": False, "evidence": [str(path) for path in required]}]
    executable = build_directory / "style-acceptance"
    compiler = os.environ.get("CXX", "c++")
    outcome = command(
        [
            compiler,
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Werror",
            "-fno-exceptions",
            "-fno-rtti",
            "-I",
            str(submission),
            str(EVALUATION / "fixtures" / "style-bounded-log-acceptance.cpp"),
            str(submission / "event_log.cpp"),
            "-o",
            str(executable),
        ],
        submission,
    )
    if outcome["returncode"] == 0:
        outcome["execution"] = command([str(executable)], submission)
    return [
        {
            "name": "independent-library-contract",
            "passed": outcome["returncode"] == 0 and outcome.get("execution", {}).get("returncode") == 0,
            "evidence": outcome,
        },
        {"name": "public-artifacts", "passed": True, "evidence": [str(path) for path in required]},
    ]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--task", required=True)
    parser.add_argument("--submission", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    submission = args.submission.resolve()
    build_directory = args.build_dir.resolve()
    output = args.out.resolve()
    if ROOT / "_Build" not in build_directory.parents:
        raise SystemExit(f"--build-dir must be below {ROOT / '_Build'}")
    if ROOT / "_Build" not in output.parents:
        raise SystemExit(f"--out must be below {ROOT / '_Build'}")

    ok, build_result = build(submission, build_directory)
    checks = [{"name": "build", "passed": ok, "evidence": build_result}]
    if ok:
        dispatch = {
            "api-bounded-traversal": bounded_traversal,
            "api-bounded-file-copy": bounded_file_copy,
            "api-request-slot-reuse": slot_reuse,
            "style-bounded-log": style_log,
        }
        checks += dispatch[args.task](submission, build_directory)
    result = {"task": args.task, "checks": checks, "full_task_success": all(check["passed"] for check in checks)}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return 0 if result["full_task_success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
