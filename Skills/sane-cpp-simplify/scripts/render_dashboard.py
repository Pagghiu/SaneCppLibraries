#!/usr/bin/env python3
"""Render the ignored SC simplification board as a self-contained HTML file."""
import argparse
import json
from pathlib import Path

skill = Path(__file__).resolve().parents[1]
root = skill.parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--data', type=Path, default=root / '_Plans/Simplification/opportunities.json')
parser.add_argument('--output', type=Path, default=root / '_Plans/Simplification/index.html')
args = parser.parse_args()
data = json.loads(args.data.read_text())
statuses = {'ready', 'explore', 'opinion', 'blocked', 'completed', 'deferred', 'rejected'}
seen = set()
for item in data['items']:
    for field in ('id', 'area', 'title', 'purpose', 'change', 'why', 'benefit', 'saving', 'status', 'priority', 'confidence', 'risk', 'sources', 'validation'):
        if field not in item:
            raise ValueError(f"Missing {field} in {item.get('id', 'item')}")
    if item['id'] in seen or item['status'] not in statuses:
        raise ValueError(f"Duplicate ID or invalid status: {item['id']}")
    if item['status'] == 'opinion' and not item.get('question'):
        raise ValueError(f"Opinion item needs a concrete question: {item['id']}")
    seen.add(item['id'])
payload = json.dumps(data, ensure_ascii=False).replace('<', '\\u003c').replace('&', '\\u0026')
template = (skill / 'assets/dashboard.html').read_text()
assert template.count('__OPPORTUNITIES_JSON__') == 1
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(template.replace('__OPPORTUNITIES_JSON__', payload))
print(f'{args.output}: {len(seen)} opportunities')
