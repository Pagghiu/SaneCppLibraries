#!/usr/bin/env python3
"""Acceptance-protocol fixture; this deliberately does not implement Sane Async."""

import json
import sys
import time


def main() -> int:
    try:
        if len(sys.argv) not in (5, 7):
            raise ValueError("wrong argument count")
        count, slots, work_ms, deadline_ms = map(int, sys.argv[1:5])
        if not (1 <= count <= 64 and 1 <= slots <= 4 and 1 <= work_ms <= 2000 and 1 <= deadline_ms <= 2000):
            raise ValueError("argument outside allowed range")
        failure = None
        if len(sys.argv) == 7:
            if sys.argv[5] != "--fail-index":
                raise ValueError("unknown option")
            failure = int(sys.argv[6])
            if not 0 <= failure < count:
                raise ValueError("invalid failure index")
    except ValueError as error:
        print(f"invalid input: {error}", file=sys.stderr)
        return 2

    expired = work_ms > deadline_ms
    if expired:
        time.sleep(deadline_ms / 1000)
    records = []
    for item in range(count):
        active = not expired or item < slots
        records.append(
            {
                "item": item,
                "slot": item % slots if active else None,
                "generation": item // slots if active else None,
                "status": (
                    "timed_out" if expired and active else
                    "cancelled" if expired else
                    "failed" if item == failure else "ok"
                ),
            }
        )
    for record in records:
        print(json.dumps(record))
    return 1 if expired or failure is not None else 0


if __name__ == "__main__":
    raise SystemExit(main())
