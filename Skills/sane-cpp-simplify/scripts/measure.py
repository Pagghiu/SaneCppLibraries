#!/usr/bin/env python3
"""Read-only, fixed-denominator simplification metrics for a Git working tree."""
import json
from pathlib import Path
import subprocess

root = Path(subprocess.check_output(['git', 'rev-parse', '--show-toplevel'], text=True).strip())
tracked = set(filter(None, subprocess.check_output(['git', 'ls-files', '-z'], cwd=root).decode().split('\0')))
untracked = set(filter(None, subprocess.check_output(['git', 'ls-files', '--others', '--exclude-standard', '-z'], cwd=root).decode().split('\0')))
paths = sorted(tracked | untracked)
source_roots = {'Libraries', 'Tests', 'Examples', 'Tools'}
source_suffixes = {'.h', '.hpp', '.c', '.cpp', '.inl', '.m', '.mm'}
totals = {key: {'files': 0, 'physical_lines': 0, 'nonblank_lines': 0}
          for key in ('maintained_source', 'tracked_utf8_text')}
missing = []
for name in filter(None, paths):
    path = root / name
    if path.is_symlink():
        continue
    if not path.is_file():
        if name in tracked:
            missing.append(name)
        continue
    data = path.read_bytes()
    if b'\0' in data:
        continue
    try:
        lines = data.decode('utf-8').splitlines()
    except UnicodeDecodeError:
        continue
    groups = ['tracked_utf8_text'] if name in tracked else []
    if Path(name).parts[0] in source_roots and path.suffix in source_suffixes:
        groups.append('maintained_source')
    for group in groups:
        totals[group]['files'] += 1
        totals[group]['physical_lines'] += len(lines)
        totals[group]['nonblank_lines'] += sum(bool(line.strip()) for line in lines)
print(json.dumps({
    'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
    'status': subprocess.check_output(['git', 'status', '--porcelain'], cwd=root, text=True).splitlines(),
    'definition': 'Source includes tracked and nonignored untracked working-tree files; repository text includes tracked only; source roots Libraries/Tests/Examples/Tools; suffixes ' + ','.join(sorted(source_suffixes)),
    'totals': totals,
    'missing_tracked_paths': missing,
    'untracked_paths': sorted(untracked),
}, indent=2))
