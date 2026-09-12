#!/usr/bin/env python3
"""Acceptance-protocol fixture; this deliberately does not implement Sane File."""

import sys


def main() -> int:
    if len(sys.argv) != 4:
        print("invalid argument count", file=sys.stderr)
        return 2
    source, destination, capacity_arg = sys.argv[1:]
    try:
        capacity = int(capacity_arg)
    except ValueError:
        capacity = 0
    if not 1 <= capacity <= 4096:
        print("invalid buffer capacity", file=sys.stderr)
        return 2
    try:
        with open(source, "rb") as input_file:
            with open(destination, "wb") as output_file:
                while True:
                    block = input_file.read(capacity)
                    if not block:
                        break
                    output_file.write(block)
    except OSError as error:
        print(f"file error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
