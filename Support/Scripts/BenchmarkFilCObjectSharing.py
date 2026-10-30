#!/usr/bin/env python3
"""Compare cold Fil-C Core builds with and without verified reuse on one runner."""

import argparse
import json
from pathlib import Path
import resource
import shutil
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--intermediate', type=Path, required=True)
    parser.add_argument('--bundle', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    root = Path.cwd().resolve()
    intermediate = options.intermediate.resolve(strict=True)
    if (intermediate.parent != root / '_Build/_Intermediates/SCTestCore'
            or intermediate.name not in ('linux-arm64-Native-filc-Release', 'linux-x86_64-Native-filc-Release')):
        raise ValueError('Benchmark requires the native Fil-C Core intermediate directory')
    output = options.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    original = output / 'original-intermediates'
    executable = root / '_Build/_Outputs' / intermediate.name / 'SCTestCore'
    shutil.copy2(executable, output / 'original-executable')
    intermediate.rename(original)
    results = []
    build = ['./SC.sh', 'build']
    profile = ['python3', 'Support/Scripts/ProfileFilCObjectSharing.py']

    def command(case, name, args):
        with (case / (name + '.log')).open('w') as log:
            subprocess.run(args, cwd=root, stdout=log, stderr=subprocess.STDOUT, check=True)

    try:
        for index, mode in enumerate(('control', 'reuse', 'reuse', 'control')):
            case = output / f'{index}-{mode}'
            case.mkdir()
            before_cpu = resource.getrusage(resource.RUSAGE_CHILDREN)
            start = time.monotonic()
            row = dict(index=index, mode=mode, success=False)
            results.append(row)
            try:
                if mode == 'control':
                    command(case, 'compile', build + ['compile', 'SCTestCore', 'Release', 'native',
                                                     '--toolchain', 'filc', '--verbose'])
                else:
                    command(case, 'unique', build + ['compile-objects', 'SCTestCore', 'Release', 'native',
                                                    '--toolchain', 'filc', '--exclude-sources', 'Libraries/*', '--verbose'])
                    command(case, 'collect', profile + ['collect', '--intermediate', str(intermediate),
                                                       '--output', str(case / 'profile')])
                    command(case, 'stage', profile + ['stage', '--local', str(case / 'profile'),
                                                     '--bundle', str(options.bundle.resolve()),
                                                     '--intermediate', str(intermediate), '--output', str(case / 'stage')])
                    command(case, 'import', build + ['import-objects', 'SCTestCore', 'Release', 'native',
                                                    '--toolchain', 'filc', '--manifest', str(case / 'stage/import.txt')])
                    command(case, 'link', build + ['compile', 'SCTestCore', 'Release', 'native',
                                                  '--toolchain', 'filc', '--verbose'])
                    command(case, 'verify', profile + ['verify-link', '--intermediate', str(intermediate),
                                                      '--log', str(case / 'link.log'), '--output', str(case / 'link-proof.json')])
                row['success'] = True
            finally:
                after_cpu = resource.getrusage(resource.RUSAGE_CHILDREN)
                row['seconds'] = time.monotonic() - start
                row['child_cpu_seconds'] = ((after_cpu.ru_utime + after_cpu.ru_stime)
                                            - (before_cpu.ru_utime + before_cpu.ru_stime))
                (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
                print(json.dumps(row), flush=True)
                if intermediate.exists():
                    intermediate.rename(case / 'intermediates')
    finally:
        # Preserve the imported binary that already passed the real TLS/test fixture.
        if intermediate.exists():
            intermediate.rename(output / 'failed-intermediates')
        original.rename(intermediate)
        shutil.copy2(output / 'original-executable', executable)
    print('ABBA comparison complete; original tested outputs restored. Transfer/readiness costs are separate.')


if __name__ == '__main__':
    main()
