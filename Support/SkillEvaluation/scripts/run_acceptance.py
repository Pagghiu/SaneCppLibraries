#!/usr/bin/env python3
"""Run executable acceptance checks without consulting condition identity."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
EVALUATION = ROOT / "Support" / "SkillEvaluation"


def command(args: list[str], cwd: Path, environment=None) -> dict:
    result = subprocess.run(args, cwd=cwd, env=environment, capture_output=True, text=True, timeout=60)
    return {"argv": args, "returncode": result.returncode, "stdout": result.stdout, "stderr": result.stderr}


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
    outcome = command([str(executable), "9", "2"], submission)
    try:
        records = [json.loads(line) for line in outcome["stdout"].splitlines() if line]
    except json.JSONDecodeError as error:
        return [{"name": "json-output", "passed": False, "evidence": str(error)}]
    valid = outcome["returncode"] == 0 and len(records) == 9 and [record.get("item") for record in records] == list(range(9))
    slots = {record.get("slot") for record in records}
    reused = len(slots) <= 2 and len(slots) > 0 and max(record.get("generation", -1) for record in records) >= 2
    return [
        {"name": "all-items-terminal", "passed": valid, "evidence": {"returncode": outcome["returncode"], "records": records}},
        {"name": "physical-slot-reuse", "passed": reused, "evidence": {"slots": sorted(slots)}},
        {
            "name": "invalid-slots",
            "passed": command([str(executable), "2", "0"], submission)["returncode"] != 0,
            "evidence": "SLOTS=0 must fail",
        },
    ]


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
