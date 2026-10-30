#!/usr/bin/env python3
"""Profile and stage verified same-run Fil-C objects for the supported build importer."""

import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import time
import tempfile
import urllib.parse
import urllib.request
import zipfile


def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(chunk)
    return value.hexdigest()


def command(entry):
    return list(entry['arguments']) if 'arguments' in entry else shlex.split(entry['command'])


def normalized(args):
    result = []
    index = 0
    while index < len(args):
        if args[index] in ('-o', '-MF', '-MT', '-MJ'):
            result.extend((args[index], '<output>'))
            index += 2
        else:
            result.append(args[index])
            index += 1
    return result


def compiler_identity(compiler):
    wrapper = Path(compiler)
    # SC's Fil-C package uses a tiny shell wrapper to select the real compiler.
    # Read its literal path, never evaluate shell content from an artifact.
    with wrapper.open('rb') as stream:
        prefix = stream.read(4096)
    real = wrapper
    if prefix.startswith(b'#!/bin/sh\n'):
        match = re.search(rb'^COMPILER_PATH="([^"\n]+)"$', prefix, re.M)
        if match is None:
            raise ValueError('Unrecognized Fil-C compiler wrapper')
        real = Path(os.fsdecode(match[1]))
    return dict(wrapper=str(wrapper), wrapper_sha256=digest(wrapper),
                executable=str(real), executable_sha256=digest(real),
                version=subprocess.check_output([compiler, '--version'], text=True))


def preprocess(entry):
    args = command(entry)
    output = []
    index = 0
    while index < len(args):
        arg = args[index]
        if arg in ('-o', '-MF', '-MT', '-MJ'):
            index += 2
        elif arg in ('-c', '-MMD', '-MD'):
            index += 1
        else:
            output.append(arg)
            index += 1
    # Generate full dependencies with the same preprocessing invocation, including
    # system headers. A stable target keeps producer/consumer dependency bytes comparable.
    with tempfile.TemporaryDirectory(prefix='sc-filc-dependencies-') as temporary:
        dependency_path = Path(temporary) / 'source.d'
        output.extend(('-E', '-P', '-MD', '-MF', str(dependency_path), '-MT', 'shared.o'))
        result = subprocess.run(output, cwd=entry['directory'], stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, check=True)
        dependency_bytes = dependency_path.read_bytes()
    dependency_text = dependency_bytes.decode().replace('\\\n', '')
    target, separator, body = dependency_text.partition(':')
    if not separator or target != 'shared.o':
        raise ValueError('Malformed compiler dependency output')
    paths = shlex.split(body)
    if not paths:
        raise ValueError('Empty compiler dependency output')
    dependencies = []
    for name in sorted(set(paths)):
        path = Path(name)
        if not path.is_absolute():
            path = Path(entry['directory']) / path
        path = path.resolve(strict=True)
        dependencies.append(dict(path=str(path), sha256=digest(path)))
    if str(Path(entry['file']).resolve()) not in {item['path'] for item in dependencies}:
        raise ValueError('Dependency output omits the source file')
    return hashlib.sha256(result.stdout).hexdigest(), dependency_bytes, dependencies


def collect(options):
    root = Path.cwd().resolve()
    revision = options.revision or os.environ.get('GITHUB_SHA')
    if not revision:
        revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
    output = options.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    start = time.monotonic()
    database = json.loads((options.intermediate / 'compile_commands.json').read_text())
    entries = [entry for entry in database if Path(entry['file']).resolve().is_relative_to(root / 'Libraries')]
    if len(entries) != 40 or len({entry['file'] for entry in entries}) != 40:
        raise ValueError('Expected exactly 40 separate common library translation units')
    compilers = {command(entry)[0] for entry in entries}
    identities = {compiler: compiler_identity(compiler) for compiler in sorted(compilers)}
    (output / 'identity.json').write_text(json.dumps(
        dict(revision=revision, compilers=identities), indent=2) + '\n')
    print(f'Fingerprinted {len(identities)} distinct compilers; preprocessing {len(entries)} sources', flush=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=min(4, os.cpu_count() or 1)) as pool:
        preprocessed = []
        for index, profile in enumerate(pool.map(preprocess, entries), 1):
            preprocessed.append(profile)
            if index % 10 == 0:
                print(f'Preprocessed {index}/{len(entries)} sources', flush=True)
    rows = []
    for entry, (expanded_hash, dependency_bytes, dependencies) in zip(entries, preprocessed):
        args = command(entry)
        obj = Path(entry.get('output') or args[args.index('-o') + 1]).resolve()
        # Never copy arbitrary paths described by imported data.
        if not obj.is_relative_to(options.intermediate.resolve()):
            raise ValueError('Object is outside the selected intermediate directory')
        row = dict(source=str(Path(entry['file']).resolve().relative_to(root)),
                   directory=entry['directory'], command=normalized(args),
                   preprocessed_sha256=expanded_hash, dependencies=dependencies,
                   dependency_sha256=hashlib.sha256(dependency_bytes).hexdigest())
        dependency_output = output / (row['source'] + '.d')
        dependency_output.parent.mkdir(parents=True, exist_ok=True)
        dependency_output.write_bytes(dependency_bytes)
        if options.bundle:
            row.update(object_sha256=digest(obj), object=str(obj), bytes=obj.stat().st_size)
        rows.append(row)
    result = dict(format=2, revision=revision, run_id=os.environ.get('GITHUB_RUN_ID'),
                  run_attempt=os.environ.get('GITHUB_RUN_ATTEMPT'), compilers=identities, rows=rows,
                  collect_seconds=time.monotonic() - start,
                  note='Diagnostic only: objects are not imported; normal compile/link and tests remain authoritative')
    (output / 'manifest.json').write_text(json.dumps(result, indent=2) + '\n')
    if options.bundle:
        before = time.monotonic()
        with zipfile.ZipFile(output / 'objects.zip', 'x', zipfile.ZIP_DEFLATED, compresslevel=1) as archive:
            archive.write(output / 'manifest.json', 'manifest.json')
            for row in rows:
                archive.write(row['object'], row['source'] + '.o')
                archive.write(output / (row['source'] + '.d'), row['source'] + '.d')
        result['bundle_seconds'] = time.monotonic() - before
        result['bundle_bytes'] = (output / 'objects.zip').stat().st_size
    (output / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({key: value for key, value in result.items() if key not in ('rows', 'compilers')}, indent=2))


def compare(options):
    local = json.loads((options.local / 'manifest.json').read_text())
    with zipfile.ZipFile(options.bundle) as archive:
        producer = json.loads(archive.read('manifest.json'))
        if local['format'] != 2 or producer['format'] != 2:
            raise ValueError('Unsupported object profile format')
        if any(local.get(key) != producer.get(key) for key in ('revision', 'run_id', 'run_attempt', 'compilers')):
            raise ValueError('Revision, run attempt or compiler identity differs')
        left = {row['source']: row for row in local['rows']}
        right = {row['source']: row for row in producer['rows']}
        if (len(left) != 40 or len(local['rows']) != 40 or len(producer['rows']) != 40
                or left.keys() != right.keys()):
            raise ValueError('Common source inventory differs')
        expected_names = {'manifest.json'} | {source + suffix for source in left for suffix in ('.o', '.d')}
        if set(archive.namelist()) != expected_names or len(archive.namelist()) != len(expected_names):
            raise ValueError('Object bundle inventory differs')
        for source, row in left.items():
            other = right[source]
            for key in ('directory', 'command', 'preprocessed_sha256', 'dependencies', 'dependency_sha256'):
                if row[key] != other[key]:
                    raise ValueError(f'{source}: {key} differs')
            if hashlib.sha256(archive.read(source + '.o')).hexdigest() != other['object_sha256']:
                raise ValueError(f'{source}: transmitted object is corrupt')
            if hashlib.sha256(archive.read(source + '.d')).hexdigest() != other['dependency_sha256']:
                raise ValueError(f'{source}: transmitted dependencies are corrupt')
        result = dict(compatible_contracts=40, transport_bytes=options.bundle.stat().st_size,
                      producer_collect_seconds=producer['collect_seconds'],
                      consumer_collect_seconds=local['collect_seconds'],
                      objects_imported=0, normal_objects_unchanged=True)
        options.output.write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result, indent=2))


def stage(options):
    # The supported build action owns acceptance metadata. This stage only writes
    # verified compiler payloads and a documented inventory in a new directory.
    output = options.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    compare(argparse.Namespace(local=options.local, bundle=options.bundle,
                               output=output / 'compatibility.json'))
    local = json.loads((options.local / 'manifest.json').read_text())
    current_compilers = {compiler: compiler_identity(compiler) for compiler in local['compilers']}
    if current_compilers != local['compilers']:
        raise ValueError('Compiler changed since compatibility collection')
    root = Path.cwd().resolve()
    database = json.loads((options.intermediate / 'compile_commands.json').read_text())
    entries = {str(Path(entry['file']).resolve().relative_to(root)): entry for entry in database
               if Path(entry['file']).resolve().is_relative_to(root / 'Libraries')}
    fields = ['SC-BUILD-OBJECTS-1']
    with zipfile.ZipFile(options.bundle) as archive:
        for row in local['rows']:
            source = row['source']
            path = Path(source)
            if path.is_absolute() or '..' in path.parts or not source.startswith('Libraries/'):
                raise ValueError('Unsafe source path')
            entry = entries[source]
            if normalized(command(entry)) != row['command'] or entry['directory'] != row['directory']:
                raise ValueError('Consumer compilation database changed')
            for dependency in row['dependencies']:
                if digest(Path(dependency['path'])) != dependency['sha256']:
                    raise ValueError('Dependency changed since compatibility collection')
            obj = output / (source + '.o')
            dep = output / (source + '.d')
            obj.parent.mkdir(parents=True, exist_ok=True)
            obj.write_bytes(archive.read(source + '.o'))
            dep.write_bytes(archive.read(source + '.d'))
            # Native SC-build emits its exact rendered command in the database.
            # Do not reconstruct quoting or write private .command files here.
            fields.extend((str(Path(entry['file']).resolve()), str(obj), str(dep),
                           entry['directory'], entry['command']))
    if any(any(character in value for character in ('\n', '\r', '\0')) for value in fields):
        raise ValueError('Object inventory fields cannot contain line delimiters')
    (output / 'import.txt').write_text('\n'.join(fields) + '\n')
    print(f'Staged {len(local["rows"])} verified objects for build import-objects; no build outputs modified')


def verify_link(options):
    database = json.loads((options.intermediate / 'compile_commands.json').read_text())
    log = options.log.read_text()
    kinds = r'(?:CC|CXX|OBJC|OBJCXX|COMPILE)'
    skipped = re.findall(r'^\[\d+/\d+\] SKIP ' + kinds + r' ', log, re.M)
    compiled = re.findall(r'^\[\d+/\d+\] ' + kinds + r' ', log, re.M)
    if len(skipped) != len(database) or compiled:
        raise ValueError('Final ordinary build did not skip every compilation unit')
    result = dict(source_count=len(database), skipped=len(skipped), compiled=len(compiled),
                  normal_dependency_export_link_checks=True)
    options.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


def wait_artifact(options):
    repository = os.environ['GITHUB_REPOSITORY']
    run_id = os.environ['GITHUB_RUN_ID']
    if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', repository) or not run_id.isdigit():
        raise ValueError('Invalid GitHub run identity')
    start = time.monotonic()
    query = urllib.parse.urlencode(dict(name=options.name, per_page=100))
    url = f'https://api.github.com/repos/{repository}/actions/runs/{run_id}/artifacts?{query}'
    while True:
        request = urllib.request.Request(url, headers={
            'Authorization': 'Bearer ' + os.environ['GITHUB_TOKEN'],
            'Accept': 'application/vnd.github+json', 'X-GitHub-Api-Version': '2022-11-28',
            'User-Agent': 'SC-ci-object-profile'})
        # Only list metadata on api.github.com. Artifact download/authentication is
        # delegated to actions/download-artifact, not forwarded to a redirect host.
        with urllib.request.urlopen(request, timeout=30) as response:
            data = json.load(response)
        matches = [entry for entry in data['artifacts'] if entry['name'] == options.name and not entry['expired']]
        elapsed = time.monotonic() - start
        if len(matches) > 1:
            raise ValueError('Ambiguous producer artifact')
        if matches:
            result = dict(artifact_id=matches[0]['id'], name=options.name, wait_seconds=elapsed,
                          size_in_bytes=matches[0]['size_in_bytes'])
            options.output.write_text(json.dumps(result, indent=2) + '\n')
            print(json.dumps(result, indent=2))
            return
        if elapsed >= options.timeout:
            raise TimeoutError(f'Producer artifact {options.name} was not published in {options.timeout}s')
        print(f'Waiting for producer artifact {options.name}: {elapsed:.1f}s', flush=True)
        time.sleep(min(5, options.timeout - elapsed))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_subparsers(dest='mode', required=True)
    collect_parser = modes.add_parser('collect')
    collect_parser.add_argument('--intermediate', type=Path, required=True)
    collect_parser.add_argument('--output', type=Path, required=True)
    collect_parser.add_argument('--bundle', action='store_true')
    collect_parser.add_argument('--revision', help='Explicit source revision for VM mounts whose Git metadata is host-only')
    compare_parser = modes.add_parser('compare')
    compare_parser.add_argument('--local', type=Path, required=True)
    compare_parser.add_argument('--bundle', type=Path, required=True)
    compare_parser.add_argument('--output', type=Path, required=True)
    stage_parser = modes.add_parser('stage')
    stage_parser.add_argument('--local', type=Path, required=True)
    stage_parser.add_argument('--bundle', type=Path, required=True)
    stage_parser.add_argument('--intermediate', type=Path, required=True)
    stage_parser.add_argument('--output', type=Path, required=True)
    link_parser = modes.add_parser('verify-link')
    link_parser.add_argument('--intermediate', type=Path, required=True)
    link_parser.add_argument('--log', type=Path, required=True)
    link_parser.add_argument('--output', type=Path, required=True)
    waiter = modes.add_parser('wait-artifact')
    waiter.add_argument('--name', required=True)
    waiter.add_argument('--timeout', type=int, default=300)
    waiter.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    if options.mode == 'collect':
        collect(options)
    elif options.mode == 'compare':
        compare(options)
    elif options.mode == 'stage':
        stage(options)
    elif options.mode == 'verify-link':
        verify_link(options)
    else:
        if options.timeout <= 0:
            raise ValueError('Timeout must be positive')
        wait_artifact(options)


if __name__ == '__main__':
    main()
