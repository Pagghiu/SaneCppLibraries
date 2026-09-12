#!/usr/bin/env python3
"""Stage a condition without exposing evaluation evidence or unrelated skills."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
EVALUATION = ROOT / "Support" / "SkillEvaluation"
SKILLS = ROOT / "Skills"


def copytree(source: Path, destination: Path, ignore=None) -> None:
    shutil.copytree(source, destination, ignore=ignore, symlinks=True)


def run_git(*args: str, text: bool = True) -> subprocess.CompletedProcess:
    return subprocess.run(["git", *args], cwd=ROOT, check=True, capture_output=True, text=text)


def copy_skill_from_git(revision: str, skill_path: str, destination: Path) -> None:
    files = run_git("ls-tree", "-r", "--name-only", revision, "--", skill_path).stdout.splitlines()
    if not files:
        raise SystemExit(f"no files found for {skill_path} at {revision}")
    for repository_path in files:
        relative = Path(repository_path).relative_to(skill_path)
        output = destination / relative
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(run_git("show", f"{revision}:{repository_path}", text=False).stdout)


def tree_sha256(root: Path) -> str:
    digest = hashlib.sha256()
    for path in sorted(root.rglob("*")):
        if path.is_file():
            digest.update(str(path.relative_to(root)).encode() + b"\0")
            digest.update(path.read_bytes())
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--condition", choices=("no-skill", "original", "optimized", "no-style", "style"), required=True)
    parser.add_argument("--task", required=True)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    matches = list((EVALUATION / "tasks").glob(f"*/{args.task}"))
    if len(matches) != 1:
        raise SystemExit(f"expected one task named {args.task}, found {len(matches)}")

    run = args.output.resolve() / args.run_id
    if run.exists():
        raise SystemExit(f"run already exists: {run}")
    stage = run / "stage"
    run.mkdir(parents=True)

    # Ordinary source/docs and root guidance stay comparable. Evaluation evidence,
    # every skill, build products, and Git metadata are omitted from the staged tree.
    copytree(ROOT, stage, shutil.ignore_patterns("Skills", "SkillEvaluation", "_Build", ".git"))
    copytree(matches[0], run / "task")
    (run / "submission").mkdir()

    versions = json.loads((EVALUATION / "skill-versions.json").read_text())
    staged_skill = stage / "Skills"
    selected_skill = None
    if args.condition == "original":
        baseline = versions["baseline"]
        selected_skill = staged_skill / "sane-cpp-libraries"
        copy_skill_from_git(baseline["git_revision"], baseline["path"], selected_skill)
    elif args.condition == "optimized":
        selected_skill = staged_skill / "sane-cpp-libraries"
        copytree(SKILLS / "sane-cpp-libraries", selected_skill)
    elif args.condition == "style":
        selected_skill = staged_skill / "sane-cpp-style"
        copytree(SKILLS / "sane-cpp-style", selected_skill)

    revision = run_git("rev-parse", "HEAD").stdout.strip()
    dirty = bool(run_git("status", "--porcelain", "--", "Skills").stdout.strip())
    manifest = {
        "run_id": args.run_id,
        "condition": args.condition,
        "task": args.task,
        "source_revision": revision,
        "source_skills_dirty": dirty,
        "skill_tree_sha256": tree_sha256(selected_skill) if selected_skill else None,
        "stage": str(stage),
        "task_copy": str(run / "task"),
        "submission": str(run / "submission"),
        "build": str(run / "build"),
        "filesystem_isolation": "best_effort",
        "notes": "Stage excludes skills unless selected and excludes all evaluation evidence. Enforce host isolation separately if unrestricted host paths remain visible.",
    }
    (run / "stage-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps(manifest, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
