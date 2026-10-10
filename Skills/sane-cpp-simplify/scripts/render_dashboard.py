#!/usr/bin/env python3
"""Render the ignored SC simplification board as a self-contained HTML file."""
import argparse
import json
import re
from pathlib import Path

skill = Path(__file__).resolve().parents[1]
root = skill.parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--data', type=Path, default=root / '_Plans/Simplification/opportunities.json')
parser.add_argument('--responses', type=Path, default=root / '_Plans/Simplification/simplification-responses.json')
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
responses = json.loads(args.responses.read_text()) if args.responses.exists() else {'schema': 1, 'project': data['project'], 'responses': {}}
if responses.get('schema') != 1 or responses.get('project') != data['project'] or not isinstance(responses.get('responses'), dict):
    raise ValueError('Response file must use schema 1 and match the dashboard project')
for response_id, response in responses['responses'].items():
    if (not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,63}', response_id)
            or response_id in {'__proto__', 'constructor', 'prototype'}
            or not isinstance(response, dict)
            or response.get('action', '') not in {'', 'implement', 'explore', 'opinion', 'defer'}
            or not isinstance(response.get('note', ''), str)):
        raise ValueError(f'Invalid response: {response_id}')
response_payload = json.dumps(responses, ensure_ascii=False).replace('<', '\\u003c').replace('&', '\\u0026')
assert template.count('__RESPONSES_JSON__') == 1
assert template.count('__RESPONSE_STORE_JS__') == 1
template = template.replace('__RESPONSES_JSON__', response_payload).replace('__RESPONSE_STORE_JS__', (skill / 'assets/response-store.js').read_text())
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(template.replace('__OPPORTUNITIES_JSON__', payload))
print(f'{args.output}: {len(seen)} opportunities')
