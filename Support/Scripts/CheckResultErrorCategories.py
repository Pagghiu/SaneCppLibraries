#!/usr/bin/env python3
import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
REGISTRY_PATH = ROOT / "Architecture/Common/result-error-categories.json"
CATEGORY_PATTERN = re.compile(
    r"static\s+constexpr\s+ResultCategory\s+(?P<constant>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*"
    r"ResultCategory\(\s*(?P<value>[0-9]+)\s*\)\s*;"
)


def fail(message):
    print(f"Result error category check failed: {message}", file=sys.stderr)
    return 1


def main():
    registry = json.loads(REGISTRY_PATH.read_text(encoding="utf-8"))
    if registry.get("schema") != 1:
        return fail("unsupported registry schema")

    external_range = registry.get("externalCategoryRange", {})
    if external_range != {"first": 0x80000000, "last": 0xFFFFFFFF}:
        return fail("external category range must remain 0x80000000..0xffffffff")

    categories = registry.get("categories")
    if not isinstance(categories, list) or not categories:
        return fail("categories must be a non-empty list")

    values = [entry.get("value") for entry in categories]
    if values[0] != 0 or categories[0].get("owner") != "Common":
        return fail("category zero must be the first Common reservation")
    if values != sorted(values):
        return fail("built-in categories must remain in ascending append-only order")
    if len(values) != len(set(values)):
        return fail("duplicate category value")
    if any(not isinstance(value, int) or value < 0 or value >= 0x80000000 for value in values):
        return fail("built-in category value lies outside 0..0x7fffffff")

    registered = {}
    for entry in categories[1:]:
        declaration = ROOT / entry.get("declaration", "")
        if not declaration.is_file():
            return fail(f"missing declaration {declaration.relative_to(ROOT)}")
        matches = {
            (match.group("constant"), int(match.group("value")))
            for match in CATEGORY_PATTERN.finditer(declaration.read_text(encoding="utf-8"))
        }
        expected = (entry.get("constant"), entry["value"])
        if expected not in matches:
            return fail(f"{entry['declaration']} does not declare {expected[0]} with value {expected[1]}")
        registered[expected[0]] = expected[1]

    declared = {}
    for header in list((ROOT / "Libraries").rglob("*.h")) + list((ROOT / "Tools").rglob("*.h")):
        for match in CATEGORY_PATTERN.finditer(header.read_text(encoding="utf-8")):
            constant = match.group("constant")
            value = int(match.group("value"))
            if constant in declared:
                return fail(f"category constant {constant} is declared more than once")
            declared[constant] = value

    if declared != registered:
        missing = sorted(set(declared) - set(registered))
        stale = sorted(set(registered) - set(declared))
        return fail(f"registry/header mismatch (unregistered={missing}, stale={stale})")

    print(f"Validated {len(categories) - 1} built-in Result error categories")
    return 0


if __name__ == "__main__":
    sys.exit(main())
