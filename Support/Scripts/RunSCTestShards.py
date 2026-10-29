#!/usr/bin/env python3
# Copyright (c) Stefano Cristiano
# SPDX-License-Identifier: MIT

"""Run two isolated SCTest processes after compiling once with SC.sh.

The test executable is not rebuilt, retried or reduced here. --compare-serial adds
an unsharded control and checks the exact successful-section occurrence counts.
"""

import argparse
import collections
import json
import os
from pathlib import Path
import re
import signal
import sys
import subprocess
import tempfile
import time


def registrations(text):
    return collections.Counter(re.findall(r"^SCTest::Registration ([^\r\n]+)$", text, re.MULTILINE))


def sections(text):
    return collections.Counter(re.findall(r"🟩 ([^\r\n]+::[^\r\n]+)", text))



def sample_sections(paths, started):
    """Observe existing flushed section output; timestamps have polling/stdio uncertainty."""
    states = {p: dict(offset=0, pending=b"", previous=started, registration=None) for p in paths}
    events = []

    def collect():
        now = time.monotonic()
        for path, state in states.items():
            if not path.exists():
                continue
            with path.open("rb") as stream:
                stream.seek(state["offset"])
                chunk = stream.read()
                state["offset"] += len(chunk)
            lines = (state["pending"] + chunk).split(b"\n")
            state["pending"] = lines.pop()
            for raw in lines:
                line = raw.decode("utf-8", errors="replace").rstrip("\r")
                registration = re.fullmatch(r"SCTest::Registration (.+)", line)
                if registration:
                    state["registration"] = registration[1]
                    state["previous"] = now
                section = re.search(r"🟩 ([^\r\n]+::[^\r\n]+)", line)
                if section:
                    events.append(dict(log=path.name, registration=state["registration"], section=section[1],
                                       observed_seconds=now - started,
                                       since_previous_ms=1000 * (now - state["previous"])))
                    state["previous"] = now
    return collect, events


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", choices=("SCTest", "SCTestCore"), default="SCTest")
    parser.add_argument("--configuration", choices=("Debug", "Release"), required=True)
    parser.add_argument("--generator", choices=("native", "xcode"), default="native")
    parser.add_argument("--toolchain", choices=("clang", "filc"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compare-serial", action="store_true")
    parser.add_argument("--timings", action="store_true", help="Record per-registration execution milliseconds")
    options = parser.parse_args()
    if os.name != "posix":
        parser.error("The parallel CI pilot currently supports Posix hosts")
    if options.generator != "native" and options.toolchain:
        parser.error("Explicit toolchains require the native generator")
    root = Path(__file__).resolve().parents[2]
    output = options.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    command = [str(root / "SC.sh"), "build", "run", options.project,
               options.configuration, options.generator]
    if options.toolchain:
        command += ["--toolchain", options.toolchain]
    command += ["--", "--all-tests"]
    if options.timings:
        command += ["--test-timings"]

    base_offset = int(os.environ.get("SC_TEST_PORT_OFFSET", "0"))
    # Keep both listening-port sets below the host's automatic client-port
    # range. A wide 22000 shift enters Linux's ephemeral range and can collide
    # with active/TIME_WAIT client connections from the serial control.
    bands = [(5000, 8200), (26000, 26200)]
    if not 0 <= base_offset <= 2000:
        raise ValueError("SC_TEST_PORT_OFFSET must be 0..2000 for this two-process pilot")
    constants = set()
    for source in (root / "Tests").rglob("*"):
        if source.suffix in (".cpp", ".h", ".inl"):
            constants.update(int(p) for p in re.findall(r"\bmapPort\(\s*(\d+)\s*\)", source.read_text()))
    constants.discard(0)  # The entrypoint queries mapPort(0) to retrieve its offset.
    outside = [p for p in constants if not any(low <= p <= high for low, high in bands)]
    if outside:
        raise ValueError(f"Extend the audited test-port bands before sharding these ports: {outside}")
    if sys.platform == "darwin":
        ephemeral_start = int(subprocess.check_output(["sysctl", "-n", "net.inet.ip.portrange.first"], text=True))
    elif sys.platform.startswith("linux"):
        ephemeral_start = int(Path("/proc/sys/net/ipv4/ip_local_port_range").read_text().split()[0])
    else:
        raise ValueError("Port-range validation currently supports Linux and macOS")
    reserved = sorted((low + base_offset + 4000 * i, high + base_offset + 4000 * i)
                      for i in range(2) for low, high in bands)
    if (any(left[1] >= right[0] for left, right in zip(reserved, reserved[1:])) or
            reserved[-1][1] >= ephemeral_start):
        raise ValueError("Test-port bands overlap each other or the host ephemeral-port range")
    (output / "ports.json").write_text(json.dumps(dict(bands=reserved, ephemeral_start=ephemeral_start,
                                                      literal_ports_audited=sorted(constants)), indent=2) + "\n")
    process = None

    def execute(args, name):
        nonlocal process
        start = time.monotonic()
        process = None
        try:
            with (output / (name + ".log")).open("w") as log:
                process = subprocess.Popen(args, cwd=root, stdout=log, stderr=log, start_new_session=True)
                if options.timings:
                    paths = ([output / f"shard-{i}.log" for i in range(2)] if name == "parent"
                             else [output / (name + ".log")])
                    collect, events = sample_sections(paths, start)
                    while process.poll() is None:
                        collect()
                        try:
                            process.wait(timeout=0.05)
                        except subprocess.TimeoutExpired:
                            pass
                    collect()
                    timeline = dict(events=events, polling_interval_ms=50,
                                    note="Flushed-output observations, not precise per-section execution times")
                    (output / (name + ".sections.json")).write_text(json.dumps(timeline, indent=2) + "\n")
                code = process.wait()
        except BaseException:
            if process is not None and process.poll() is None:
                try:
                    os.killpg(process.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
                process.wait()
            raise
        return dict(seconds=time.monotonic() - start, exit=code)

    def scratch():
        path = Path(tempfile.mkdtemp(prefix="sc-ts-", dir="/tmp")).resolve()
        (path / "root-marker.txt").write_text("isolated SCTest scratch root\n")
        # Exercise recursive/manual iteration and its exclusion branch even in
        # a fresh root, independent of incidental build-output contents.
        for relative in ("traversal/nested", "someExcludePattern"):
            directory = path / relative
            directory.mkdir(parents=True)
            (directory / "entry.txt").write_text("iterator fixture\n")
        return path

    control = None
    if options.compare_serial:
        serial_root = scratch()
        control = execute(command + ["--test-root", str(serial_root), "--port-offset", str(base_offset)], "serial")
        control["scratch"] = str(serial_root)
        print(f"serial: {control['seconds']:.3f}s, exit {control['exit']}", flush=True)
        if control["exit"]:
            raise RuntimeError("Serial control failed; no parallel run attempted")

    roots = [scratch(), scratch()]
    # Enter the framework once: Fil-C package validation recompiles smoke
    # executables at shared paths, so concurrent framework startups are unsafe.
    # SCTest then starts the same compiled executable internally, as its other
    # child-process tests do, after all runtime preparation has finished.
    parent = execute(command + ["--port-offset", str(base_offset), "--run-shards",
                                str(roots[0]), str(roots[1]), str(output)], "parent")
    parallel_seconds = parent["seconds"]
    parent_text = (output / "parent.log").read_text()
    expected = registrations(parent_text)
    if not expected:
        raise RuntimeError("SCTest did not emit its registration inventory; compile it before running shards")
    statuses = dict(re.findall(r"^SCTest::Shard ([01]) exit (\d+)$",
                               parent_text, re.MULTILINE))
    results = [dict(name="shard-" + str(i), exit=int(statuses[str(i)]) if str(i) in statuses else None,
                    scratch=str(roots[i])) for i in range(2)]
    texts = []
    for result in results:
        text = ""
        for extension in (".log", ".err"):
            path = output / (result["name"] + extension)
            if path.exists():
                text += path.read_text()
        texts.append(text)
        print(text, end="")
    observed_offsets = []
    for text in texts:
        match = re.search(r"TestReport::Using port offset (\d+)", text)
        observed_offsets.append(int(match[1]) if match else 0)
    offsets_match = observed_offsets == [base_offset, base_offset + 4000]
    actual = sum((registrations(t) for t in texts), collections.Counter())
    section_counts = sum((sections(t) for t in texts), collections.Counter())
    summary = dict(processes=results, parent=parent, parallel_seconds=parallel_seconds,
                   observed_port_offsets=observed_offsets, port_offsets_match=offsets_match,
                   registration_inventory_matches=actual == expected,
                   registration_occurrences=sum(actual.values()),
                   section_occurrences=sum(section_counts.values()),
                   unique_sections=len(section_counts), serial=control)
    if options.timings:
        def timings(text):
            return [dict(registration=name, milliseconds=int(ms)) for name, ms in re.findall(
                r"^SCTest::Timing ([^ ]+) (\d+) ms$", text, re.MULTILINE)]
        child_timings = [timings(t) for t in texts]
        timing_inventory_matches = all(
            collections.Counter(t["registration"] for t in measured) == registrations(text)
            for text, measured in zip(texts, child_timings))
        summary["timings"] = dict(shards=child_timings, inventory_matches=timing_inventory_matches)
        if control:
            serial_text = (output / "serial.log").read_text()
            summary["timings"]["serial"] = timings(serial_text)
            timing_inventory_matches = timing_inventory_matches and (
                collections.Counter(t["registration"] for t in summary["timings"]["serial"]) == expected)
            summary["timings"]["inventory_matches"] = timing_inventory_matches
    if control:
        reference = sections((output / "serial.log").read_text())
        summary["section_inventory_matches"] = reference == section_counts
        summary["missing_sections"] = dict(reference - section_counts)
        summary["extra_sections"] = dict(section_counts - reference)
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))
    if (parent["exit"] != 0 or any(r["exit"] != 0 for r in results) or actual != expected or not offsets_match or
            (options.timings and not timing_inventory_matches) or
            (control and not summary["section_inventory_matches"])):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
