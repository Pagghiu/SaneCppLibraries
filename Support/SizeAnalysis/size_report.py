#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import pathlib
import re
import subprocess
import sys
from collections import defaultdict


def run_command(args: list[str]) -> str:
    return subprocess.run(args, check=True, text=True, capture_output=True).stdout


def detect_binary_format(binary: pathlib.Path) -> str:
    suffix = binary.suffix.lower()
    if suffix in {".dylib"}:
        return "macho"
    if suffix in {".so"}:
        return "elf"
    if suffix in {".dll"}:
        return "pe"
    if sys.platform == "darwin":
        return "macho"
    if sys.platform.startswith("linux"):
        return "elf"
    return "unknown"


def parse_macho_sections(binary: pathlib.Path) -> dict[str, int]:
    output = run_command(["size", "-m", str(binary)])
    sections: dict[str, int] = {}
    for line in output.splitlines():
        match = re.search(r"Section (__[A-Za-z0-9_]+): ([0-9]+)", line)
        if match:
            sections[match.group(1)] = int(match.group(2))
        match = re.search(r"Segment (__[A-Za-z0-9_]+): ([0-9]+)", line)
        if match:
            sections[match.group(1)] = int(match.group(2))
    return sections


def parse_elf_sections(binary: pathlib.Path) -> dict[str, int]:
    output = run_command(["size", "-A", "-d", str(binary)])
    sections: dict[str, int] = {}
    for line in output.splitlines():
        match = re.match(r"([.A-Za-z0-9_]+)\s+([0-9]+)\s+", line.strip())
        if match:
            sections[match.group(1)] = int(match.group(2))
    return sections


def collect_exports(binary: pathlib.Path, binary_format: str) -> list[str]:
    if binary_format == "macho":
        output = run_command(["xcrun", "dyld_info", "-exports", str(binary)])
        return [line.strip() for line in output.splitlines() if "0x" in line and "[re-export]" not in line]
    if binary_format == "elf":
        output = run_command(["nm", "-D", "--defined-only", "-C", str(binary)])
        return [line.strip() for line in output.splitlines() if line.strip()]
    return []


def collect_nm_counts(binary: pathlib.Path, binary_format: str) -> dict[str, int]:
    output = run_command(["nm", str(binary)])
    lines = [line for line in output.splitlines() if line.strip()]
    counts = {
        "lines": len(lines),
        "global": 0,
        "local_text": 0,
    }
    for line in lines:
        if re.match(r"^[0-9A-Fa-f]+\s+[A-Z]\s+", line):
            counts["global"] += 1
        if re.match(r"^[0-9A-Fa-f]+\s+t\s+", line):
            counts["local_text"] += 1
    if binary_format == "elf":
        dynamic = run_command(["nm", "-D", "--defined-only", str(binary)])
        counts["dynamic_exports"] = len([line for line in dynamic.splitlines() if line.strip()])
    return counts


def bucket_template_family(symbol: str) -> str | None:
    families = (
        "SC::Function<",
        "SC::Event<",
        "SC::StringFormat<",
        "SC::VirtualArray<",
        "SC::IntrusiveDoubleLinkedList<",
        "SC::AsyncRequestReadableStream<",
        "SC::AsyncRequestWritableStream<",
        "applyOnAsync<",
        "HttpParser::process<",
        "HttpMultipartParser::process<",
        "StringIterator<",
    )
    for family in families:
        if family in symbol:
            return family
    return None


def bucket_library(symbol: str) -> str:
    buckets = (
        ("Async", ("SC::Async", "AsyncEventLoop", "AsyncRequest", "AsyncBuffer", "AsyncPipeline")),
        ("Http", ("SC::Http", "HttpParser", "HttpMultipartParser")),
        ("Strings", ("SC::String", "CommandLine", "Path::", "Console::")),
        ("Socket", ("SC::Socket",)),
        ("File", ("SC::File", "FileSystem", "FileDescriptor")),
        ("Memory", ("SC::Memory", "VirtualMemory", "Segment", "GrowableBuffer")),
        ("Threading", ("SC::Thread", "SC::Mutex", "SC::ConditionVariable", "ThreadPool")),
        ("Process", ("SC::Process",)),
        ("Reflection", ("SC::Reflection",)),
        ("Serialization", ("SC::Serialization",)),
        ("Hashing", ("SC::Hash",)),
        ("Time", ("SC::Time",)),
        ("Testing", ("SC::Test",)),
        ("Foundation", ("SC::Result", "SC::Span", "SC::Optional", "SC::UniqueHandle", "SC::Assert")),
    )
    for label, needles in buckets:
        if any(needle in symbol for needle in needles):
            return label
    return "Other"


def parse_macho_map(map_path: pathlib.Path, project_root: pathlib.Path) -> dict[str, object]:
    text = map_path.read_text().splitlines()
    object_files: dict[int, str] = {}
    text_start = 0
    text_end = 0
    in_objects = False
    in_sections = False
    in_symbols = False
    symbols: list[tuple[int, int, int, str]] = []

    for line in text:
        if line.startswith("# Object files:"):
            in_objects = True
            in_sections = False
            in_symbols = False
            continue
        if line.startswith("# Sections:"):
            in_objects = False
            in_sections = True
            in_symbols = False
            continue
        if line.startswith("# Symbols:"):
            in_objects = False
            in_sections = False
            in_symbols = True
            continue

        if in_objects:
            match = re.match(r"\[\s*(\d+)\]\s+(.*)", line)
            if match:
                object_files[int(match.group(1))] = match.group(2)
            continue

        if in_sections:
            match = re.match(r"0x([0-9A-Fa-f]+)\s+0x([0-9A-Fa-f]+)\s+(__\S+)\s+(__\S+)", line)
            if match and match.group(3) == "__TEXT" and match.group(4) == "__text":
                text_start = int(match.group(1), 16)
                text_end = text_start + int(match.group(2), 16)
            continue

        if in_symbols:
            match = re.match(r"0x([0-9A-Fa-f]+)\s+0x([0-9A-Fa-f]+)\s+\[\s*(\d+)\]\s+(.*)", line)
            if match:
                address = int(match.group(1), 16)
                size = int(match.group(2), 16)
                file_id = int(match.group(3))
                name = match.group(4)
                if size > 0 and text_start <= address < text_end:
                    symbols.append((address, size, file_id, name))

    demangled_output = ""
    if symbols:
        demangled_output = subprocess.run(
            ["xcrun", "llvm-cxxfilt"],
            input="\n".join(symbol[3] for symbol in symbols),
            text=True,
            capture_output=True,
            check=True,
        ).stdout
    demangled = demangled_output.splitlines()

    code_symbols: list[dict[str, object]] = []
    object_totals: dict[str, int] = defaultdict(int)
    folder_totals: dict[str, int] = defaultdict(int)
    library_totals: dict[str, int] = defaultdict(int)
    template_totals: dict[str, int] = defaultdict(int)
    template_text = 0

    for (address, size, file_id, _), demangled_name in zip(symbols, demangled):
        source_path = object_files.get(file_id, f"[{file_id}]")
        source_display = source_path
        if source_path.startswith(str(project_root) + "/"):
            source_display = source_path[len(str(project_root)) + 1 :]
        folder_display = source_display
        if "/" in folder_display:
            folder_display = "/".join(folder_display.split("/")[:2])
        object_totals[source_display] += size
        folder_totals[folder_display] += size
        library_totals[bucket_library(demangled_name)] += size
        if "<" in demangled_name and ">" in demangled_name:
            template_text += size
            family = bucket_template_family(demangled_name)
            if family is not None:
                template_totals[family] += size
        code_symbols.append(
            {
                "address": address,
                "size": size,
                "object": source_display,
                "symbol": demangled_name,
            }
        )

    code_symbols.sort(key=lambda item: item["size"], reverse=True)

    return {
        "text_total": sum(item["size"] for item in code_symbols),
        "template_text": template_text,
        "top_symbols": code_symbols[:30],
        "top_objects": sorted(object_totals.items(), key=lambda item: item[1], reverse=True)[:30],
        "top_folders": sorted(folder_totals.items(), key=lambda item: item[1], reverse=True)[:20],
        "library_buckets": sorted(library_totals.items(), key=lambda item: item[1], reverse=True),
        "template_families": sorted(template_totals.items(), key=lambda item: item[1], reverse=True),
    }


def build_report(binary: pathlib.Path, map_path: pathlib.Path | None) -> dict[str, object]:
    binary_format = detect_binary_format(binary)
    report: dict[str, object] = {
        "binary": str(binary),
        "format": binary_format,
        "file_size": binary.stat().st_size,
        "sections": {},
        "exports": [],
        "nm": {},
    }

    if binary_format == "macho":
        report["sections"] = parse_macho_sections(binary)
    elif binary_format == "elf":
        report["sections"] = parse_elf_sections(binary)

    report["exports"] = collect_exports(binary, binary_format)
    report["nm"] = collect_nm_counts(binary, binary_format)

    if map_path is not None:
        report["map"] = parse_macho_map(map_path, pathlib.Path.cwd())

    return report


def check_thresholds(report: dict[str, object], args: argparse.Namespace) -> list[str]:
    failures: list[str] = []
    if args.max_file_size is not None and report["file_size"] > args.max_file_size:
        failures.append(f"file_size {report['file_size']} exceeds {args.max_file_size}")

    sections: dict[str, int] = report.get("sections", {})  # type: ignore[assignment]
    text_size = sections.get("__text", sections.get(".text", 0))
    cstring_size = sections.get("__cstring", sections.get(".rodata", 0))
    if args.max_text_size is not None and text_size > args.max_text_size:
        failures.append(f"text_size {text_size} exceeds {args.max_text_size}")
    if args.max_cstring_size is not None and cstring_size > args.max_cstring_size:
        failures.append(f"cstring_size {cstring_size} exceeds {args.max_cstring_size}")
    return failures


def print_human(report: dict[str, object], top: int) -> None:
    print(f"binary: {report['binary']}")
    print(f"format: {report['format']}")
    print(f"file_size: {report['file_size']}")

    sections: dict[str, int] = report["sections"]  # type: ignore[assignment]
    if sections:
        print("sections:")
        for name, size in sorted(sections.items(), key=lambda item: item[1], reverse=True):
            print(f"  {name}: {size}")

    exports: list[str] = report["exports"]  # type: ignore[assignment]
    print(f"exports_count: {len(exports)}")
    if exports:
        for line in exports[:top]:
            print(f"  export: {line}")

    nm_counts: dict[str, int] = report["nm"]  # type: ignore[assignment]
    if nm_counts:
        print("nm:")
        for key, value in nm_counts.items():
            print(f"  {key}: {value}")

    map_report = report.get("map")
    if not isinstance(map_report, dict):
        return

    text_total = int(map_report.get("text_total", 0))
    template_text = int(map_report.get("template_text", 0))
    print(f"map_text_total: {text_total}")
    if text_total:
        print(f"template_text: {template_text} ({template_text * 100.0 / text_total:.1f}%)")

    print("library_buckets:")
    for name, size in map_report.get("library_buckets", [])[:top]:
        percentage = (size * 100.0 / text_total) if text_total else 0.0
        print(f"  {name}: {size} ({percentage:.1f}%)")

    template_families = map_report.get("template_families", [])
    if template_families:
        print("template_families:")
        for name, size in template_families[:top]:
            percentage = (size * 100.0 / text_total) if text_total else 0.0
            print(f"  {name}: {size} ({percentage:.1f}%)")

    print("top_objects:")
    for name, size in map_report.get("top_objects", [])[:top]:
        percentage = (size * 100.0 / text_total) if text_total else 0.0
        print(f"  {name}: {size} ({percentage:.1f}%)")

    print("top_symbols:")
    for entry in map_report.get("top_symbols", [])[:top]:
        percentage = (entry["size"] * 100.0 / text_total) if text_total else 0.0
        print(f"  {entry['size']}: {entry['symbol']} [{entry['object']}] ({percentage:.1f}%)")


def main() -> int:
    parser = argparse.ArgumentParser(description="Report size statistics for a binary or shared library.")
    parser.add_argument("binary", help="Path to the binary to analyze")
    parser.add_argument("--map", dest="map_path", help="Optional Mach-O linker map file")
    parser.add_argument("--json", action="store_true", help="Print JSON instead of human-readable text")
    parser.add_argument("--top", type=int, default=15, help="How many top entries to print")
    parser.add_argument("--max-file-size", type=int, help="Fail if file size exceeds this many bytes")
    parser.add_argument("--max-text-size", type=int, help="Fail if text/code size exceeds this many bytes")
    parser.add_argument("--max-cstring-size", type=int, help="Fail if cstring/rodata size exceeds this many bytes")
    args = parser.parse_args()

    binary = pathlib.Path(args.binary).resolve()
    map_path = pathlib.Path(args.map_path).resolve() if args.map_path else None

    report = build_report(binary, map_path)
    failures = check_thresholds(report, args)

    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print_human(report, args.top)

    if failures:
        for failure in failures:
            print(f"threshold_fail: {failure}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
