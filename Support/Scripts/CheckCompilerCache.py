#!/usr/bin/env python3
# Copyright (c) Stefano Cristiano
# SPDX-License-Identifier: MIT

"""Check the CI compiler cache's input and compiler-content invalidation."""

import os
from pathlib import Path
import shlex
import subprocess
import tempfile


def main():
    policy = subprocess.check_output(
        ["ccache", "--get-config", "compiler_check"], text=True
    ).strip()
    if policy != "content":
        raise RuntimeError(f"Expected compiler-content checking, got {policy!r}")

    real_compiler = Path("/usr/bin/gcc").resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="sc-compiler-cache-", dir="_Build") as temporary:
        root = Path(temporary).resolve()
        cache_directory = root / "cache"
        cache_directory.mkdir()
        environment = dict(os.environ)
        environment.update(
            CCACHE_DIR=str(cache_directory),
            CCACHE_COMPILERCHECK=policy,
            CCACHE_MAXSIZE="16M",
        )
        environment.pop("CCACHE_DISABLE", None)

        # A compiler facade lets us change the configured executable's contents
        # without touching the installed compiler. Its execution remains identical.
        compiler = root / "gcc"
        compiler.write_text(
            "#!/bin/sh\n# compiler identity A\n"
            f"exec {shlex.quote(str(real_compiler))} \"$@\"\n"
        )
        compiler.chmod(0o755)
        header = root / "probe.h"
        source = root / "probe.c"
        header.write_text("#define CACHE_VALUE 7\n")
        source.write_text('#include "probe.h"\nint cache_probe(void) { return CACHE_VALUE; }\n')

        def statistics():
            output = subprocess.check_output(
                ["ccache", "--print-stats"], env=environment, text=True
            )
            return {
                key: int(value)
                for key, value in (line.split() for line in output.splitlines())
            }

        def compile_probe(label, *, hit=False, optimization="-O2"):
            before = statistics()
            subprocess.run(
                [
                    "ccache", str(compiler), optimization, "-c", str(source),
                    "-o", str(root / "probe.o"),
                ],
                env=environment,
                check=True,
            )
            after = statistics()
            hits = sum(
                after[key] - before[key]
                for key in ("direct_cache_hit", "preprocessed_cache_hit")
            )
            misses = after["cache_miss"] - before["cache_miss"]
            if (hits, misses) != ((1, 0) if hit else (0, 1)):
                raise RuntimeError(f"{label}: unexpected cache delta hits={hits}, misses={misses}")
            print(f"{label}: hits={hits}, misses={misses}")

        subprocess.run(
            ["ccache", "--zero-stats"], env=environment,
            check=True, stdout=subprocess.DEVNULL,
        )
        compile_probe("Cold compilation")
        compile_probe("Identical compilation", hit=True)
        source.write_text('#include "probe.h"\nint cache_probe(void) { return CACHE_VALUE + 1; }\n')
        compile_probe("Changed source")
        header.write_text("#define CACHE_VALUE 9\n")
        compile_probe("Changed header")
        compile_probe("Changed flags", optimization="-O1")

        previous = compiler.stat()
        compiler.write_text(compiler.read_text().replace("identity A", "identity B"))
        os.utime(compiler, ns=(previous.st_atime_ns, previous.st_mtime_ns))
        current = compiler.stat()
        if (current.st_size, current.st_mtime_ns) != (previous.st_size, previous.st_mtime_ns):
            raise RuntimeError("Compiler identity probe did not preserve size and timestamp")
        compile_probe("Changed compiler content with unchanged size/mtime", optimization="-O1")


if __name__ == "__main__":
    main()
