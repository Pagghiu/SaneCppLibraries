#!/usr/bin/env python3
"""Validate repository-local benchmark metadata and skill routing."""

from __future__ import annotations

import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
EVALUATION = ROOT / "Support" / "SkillEvaluation"
SKILLS = ROOT / "Skills"


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def main() -> int:
    failures: list[str] = []
    for skill in (SKILLS / "sane-cpp-libraries", SKILLS / "sane-cpp-style"):
        content = (skill / "SKILL.md").read_text()
        if not content.startswith("---\n") or "\n---\n" not in content[4:]:
            failures.append(f"missing skill frontmatter: {skill}")
        if f"name: {skill.name}\n" not in content or "description: " not in content:
            failures.append(f"missing skill name or description: {skill}")

    versions = json.loads((EVALUATION / "skill-versions.json").read_text())
    baseline = versions["baseline"]
    result = subprocess.run(
        ["git", "show", f"{baseline['git_revision']}:{baseline['path']}/SKILL.md"],
        cwd=ROOT,
        capture_output=True,
    )
    if result.returncode:
        failures.append("baseline skill cannot be recovered from its recorded Git revision")
    elif sha256_bytes(result.stdout) != baseline["skill_md_sha256"]:
        failures.append("baseline Git skill hash differs from skill-versions.json")

    for path in EVALUATION.glob("tasks/*/*/task.md"):
        if not path.with_name("review.md").is_file():
            failures.append(f"missing review rubric for {path.parent}")
    if not (EVALUATION / "fixtures" / "style-bounded-log-acceptance.cpp").is_file():
        failures.append("missing independent Style acceptance fixture")
    if not (EVALUATION / "fixtures" / "slot-protocol" / "slots.py").is_file():
        failures.append("missing timer-slot acceptance protocol fixture")
    if not (EVALUATION / "fixtures" / "file-copy-protocol" / "copy.py").is_file():
        failures.append("missing file-copy acceptance protocol fixture")
    for path in SKILLS.glob("*/SKILL.md"):
        text = path.read_text()
        for target in re.findall(r"\]\(([^)#]+)", text):
            if not (path.parent / target).is_file():
                failures.append(f"broken skill link: {path}: {target}")
    executable_magic = (b"\xcf\xfa\xed\xfe", b"\xfe\xed\xfa\xcf", b"\x7fELF", b"MZ")
    for path in EVALUATION.rglob("*"):
        if path.is_file() and path.read_bytes()[:4].startswith(executable_magic):
            failures.append(f"generated binary must be under _Build: {path}")

    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    print("skill evaluation validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
