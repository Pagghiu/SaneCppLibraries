# This script automates the creation of single-file C++ libraries and their corresponding test files
# for the Sane C++ project.

# What it does:
# 1.  Parses `Dependencies/Dependencies.json` to understand the project's library structure and dependencies.
# 2.  For each library, it generates a single-file header (`.h`) in `_Build/_SingleFileLibraries/`.
# 3.  For each generated single-file library, it creates a basic test file (`.cpp`) in `_Build/_SingleFileLibrariesTest/`.

# How it works (Amalgamation Logic):
# - **Input:** It can either use a specific order file (`SingleFileLibs/SaneCpp<LibraryName>.json`) if present,
#   which defines `includeOrder` (for the public API section) and `implementationOrder` (for the
#   SANE_CPP_IMPLEMENTATION section). If no order file exists, it infers the order by
#   including all public headers for the API and all .cpp files for the implementation.
# - **Content Processing:**
#   - It recursively inlines the content of internal headers (`.h` files within 'Internal' directories)
#     and `.inl` files that belong to the *same* library being processed.
#   - It strips out common C++ boilerplate like `#pragma once`, copyright headers, and SPDX-License-Identifier.
#   - It removes `#include` directives for files that are part of *other* libraries, as these
#     dependencies are handled by top-level `#include "SaneCpp<Dependency>.h"` statements.
# - **Duplicate Prevention:** A `processed_files` set, using normalized absolute paths, is maintained
#   for each library to ensure that no file's content is inlined more than once, preventing
#   redundant code and circular inclusion issues.
# - **Test File Generation:** Each generated test file includes the single-file library and a dummy `main()`
#   function to ensure it compiles.

import json
import os
import re
import sys
import subprocess
import argparse
import hashlib
from collections import defaultdict
DIVIDER = "//" + ''.join(['-' for _ in range(120-2)]) + '\n'

def get_library_files(root_dir, library_name):
    """Gets all .h and .cpp files for a given library."""
    library_dir = os.path.join(root_dir, 'Libraries', library_name)
    files = []
    for root, _, filenames in os.walk(library_dir):
        for filename in filenames:
            if filename.endswith(('.h', '.cpp', '.inl')):
                files.append(os.path.join(root, filename))
    return files

def get_common_files(root_dir):
    """Gets all .h, .cpp and .inl files for Libraries/Common."""
    common_dir = os.path.join(root_dir, 'Libraries', 'Common')
    files = []
    for root, _, filenames in os.walk(common_dir):
        for filename in filenames:
            if filename.endswith(('.h', '.cpp', '.inl')):
                files.append(os.path.join(root, filename))
    return files

def get_git_version():
    """Retrieves the git version information."""
    try:
        tag = subprocess.check_output(['git', 'describe', '--tags', '--abbrev=0'], stderr=subprocess.DEVNULL).strip().decode('utf-8')
        commit = subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD'], stderr=subprocess.DEVNULL).strip().decode('utf-8')
        return f"{tag} ({commit})"
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "unknown"

def count_loc(content):
    """Counts lines of code and comments in a C-style source string."""
    code_lines = 0
    comment_lines = 0
    in_multiline_comment = False
    for line in content.splitlines():
        stripped = line.strip()
        if not stripped:
            comment_lines += 1
            continue

        if in_multiline_comment:
            comment_lines += 1
            if '*/' in stripped:
                in_multiline_comment = False
            continue

        if stripped.startswith('//'):
            comment_lines += 1
        elif stripped.startswith('/*'):
            comment_lines += 1
            if '*/' not in stripped:
                in_multiline_comment = True
        else:
            # This is a line of code. It might contain a comment, but it's primarily code.
            code_lines += 1
            
    return code_lines, comment_lines

def make_loc(code=0, comments=0):
    return {'code': code, 'comments': comments}

def add_loc(lhs, rhs):
    return make_loc(lhs['code'] + rhs['code'], lhs['comments'] + rhs['comments'])

def sum_loc(values):
    total = make_loc()
    for value in values:
        total = add_loc(total, value)
    return total

def count_files_loc(files):
    content = ""
    for file_path in files:
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content += f.read().rstrip() + '\n'
        except Exception as e:
            print(f"Error processing file {file_path}: {e}")
    code, comments = count_loc(content)
    return make_loc(code, comments)

def count_files_loc_by_kind(files):
    headers = []
    sources = []
    for file_path in files:
        if file_path.endswith('.h'):
            headers.append(file_path)
        elif file_path.endswith(('.cpp', '.inl')):
            sources.append(file_path)
    return {
        'header': count_files_loc(headers),
        'source': count_files_loc(sources),
    }

def split_processed_loc(lines_by_origin):
    owned_code, owned_comments = count_loc(''.join(lines_by_origin['owned']))
    common_code, common_comments = count_loc(''.join(lines_by_origin['common']))
    return {
        'owned': make_loc(owned_code, owned_comments),
        'common': make_loc(common_code, common_comments),
    }

def processed_line_origin(file_path, root_dir):
    relative_path_from_root = os.path.relpath(os.path.realpath(file_path), root_dir)
    path_parts = relative_path_from_root.split(os.sep)
    if path_parts[0] == 'Libraries' and len(path_parts) > 1 and path_parts[1] == 'Common':
        return 'common'
    return 'owned'

def _process_file_recursively(file_path, processed_files, root_dir, current_library_name, authors_info, spdx_set,
                              lines_by_origin):
    # Normalize file_path to ensure consistent representation
    normalized_file_path = os.path.realpath(file_path)

    if normalized_file_path in processed_files:
        return "" # Already processed, return empty string

    # Mark as processed *before* reading content to prevent infinite recursion for circular dependencies
    processed_files.add(normalized_file_path)

    file_content = ""
    relative_path_from_root = os.path.relpath(normalized_file_path, root_dir)
    path_parts = relative_path_from_root.split(os.sep)
    if path_parts[0] in ['Libraries', 'LibrariesExtra']:
        relative_to_libs = os.path.join(*path_parts[1:])
        file_content += DIVIDER
        file_content += f'// {relative_to_libs.replace(os.sep, "/")}\n'
        file_content += DIVIDER
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            for line in f:
                stripped_line = line.strip()
                
                # Extract Copyright and SPDX info
                copyright_match = re.match(r'// Copyright \(c\) (.*)', stripped_line)
                if copyright_match:
                    author = copyright_match.group(1).strip()
                    authors_info[author] += 1

                spdx_match = re.match(r'// SPDX-License-Identifier: (.*)', stripped_line)
                if spdx_match:
                    spdx_identifier = spdx_match.group(1).strip()
                    spdx_set.add(spdx_identifier)

                if stripped_line.startswith(('#pragma once')):
                    continue

                match = re.match(r'#include "(.*)"', stripped_line)
                if match:
                    included_path_str = match.group(1)
                    
                    # Resolve included path relative to the current file's directory
                    abs_included_path = os.path.abspath(os.path.join(os.path.dirname(file_path), included_path_str))
                    normalized_abs_included_path = os.path.realpath(abs_included_path)

                    # Check if the included file exists
                    if not os.path.exists(normalized_abs_included_path):
                        file_content += line
                        continue

                    # Check if the included file is within the Libraries or LibrariesExtra directories
                    normalized_root_libraries = os.path.realpath(os.path.join(root_dir, 'Libraries'))
                    normalized_root_libraries_extra = os.path.realpath(os.path.join(root_dir, 'LibrariesExtra'))

                    if not (normalized_abs_included_path.startswith(normalized_root_libraries) or \
                            normalized_abs_included_path.startswith(normalized_root_libraries_extra)):
                        file_content += line
                        continue

                    # Determine the library name of the included file
                    relative_path_from_root = os.path.relpath(normalized_abs_included_path, root_dir)
                    path_parts = relative_path_from_root.split(os.sep)
                    
                    if path_parts[0] in ['Libraries', 'LibrariesExtra'] and len(path_parts) > 1: # Ensure there's a library name
                        included_library_name = path_parts[1]
                        if path_parts[0] == 'Libraries' and included_library_name == 'Common':
                            file_content += _process_file_recursively(normalized_abs_included_path, processed_files,
                                                                      root_dir, current_library_name, authors_info,
                                                                      spdx_set, lines_by_origin)
                        elif included_library_name == current_library_name:
                            file_content += _process_file_recursively(normalized_abs_included_path, processed_files,
                                                                      root_dir, current_library_name, authors_info,
                                                                      spdx_set, lines_by_origin)
                        else:
                            # This is an include from another library, so we strip it.
                            pass
                        continue
                
                file_content += line
                lines_by_origin[processed_line_origin(normalized_file_path, root_dir)].append(line)
    except Exception as e:
        print(f"Error processing file {file_path}: {e}")
        return "" # Return empty string on error
    file_content = file_content.rstrip() + '\n\n'
    return file_content

def amalgamate_files_recursively(library_files, order_list, root_dir, library_name, processed_files, authors_info,
                                 spdx_set, lines_by_origin):
    """Amalgamates files, inlining internal includes."""
    content = ""
    
    # Create a map for quick lookup of library files by their basename and relative path
    library_file_lookup = {}
    for f_path in library_files:
        normalized_f_path = os.path.realpath(f_path)
        library_file_lookup[normalized_f_path] = normalized_f_path # Store realpath as key
        library_file_lookup[os.path.basename(normalized_f_path)] = normalized_f_path
        relative_to_lib_dir = os.path.relpath(normalized_f_path, os.path.join(root_dir, 'Libraries', library_name))
        library_file_lookup[relative_to_lib_dir.replace(os.sep, '/')]= normalized_f_path
        library_file_lookup[relative_to_lib_dir.replace('/', os.sep)] = normalized_f_path
        
    for filename in order_list:
        # Normalize filename from order_list to a canonical form for lookup
        # We need a base path to resolve relative filenames from order_list
        # Assuming the first file in library_files is a good base for relative paths within the library
        base_for_relative_lookup = library_files[0] if library_files else root_dir
        
        # Try to resolve filename to a realpath first
        potential_full_path = os.path.realpath(os.path.join(os.path.dirname(base_for_relative_lookup), filename))

        full_path = None
        if potential_full_path in library_file_lookup:
            full_path = library_file_lookup[potential_full_path]
        elif os.path.exists(potential_full_path):
            relative_path = os.path.relpath(potential_full_path, root_dir)
            path_parts = relative_path.split(os.sep)
            if path_parts[0] == 'Libraries' and len(path_parts) > 1 and path_parts[1] == 'Common':
                full_path = potential_full_path
        elif filename in library_file_lookup: # Fallback to original lookup keys
            full_path = library_file_lookup[filename]
        else:
            # Fallback: Iterate through library_files and check endswith, then normalize
            normalized_filename_for_lookup = filename.replace('/', os.sep).replace('\\', os.path.sep)
            for f in library_files:
                normalized_f = os.path.realpath(f);
                if normalized_f.endswith(normalized_filename_for_lookup):
                    full_path = normalized_f
                    break
        
        if not full_path:
            print(f"Warning: File '{filename}' from order list not found in library '{library_name}'")
            continue
            
        # Ensure full_path is always a realpath before passing to recursive processing
        full_path = os.path.realpath(full_path) # Redundant but safe
        content += _process_file_recursively(full_path, processed_files, root_dir, library_name, authors_info, spdx_set,
                                             lines_by_origin)
    return content

def normalize_content_for_hash(content):
    return content.replace('\r\n', '\n').replace('\r', '\n')

def calculate_content_hash(headers, sources):
    content = normalize_content_for_hash(headers + sources)
    return hashlib.sha256(content.encode('utf-8')).hexdigest()[:16].upper()

def macro_library_name(library_name):
    return re.sub(r'[^A-Za-z0-9]', '_', library_name).upper()

def validate_dependency_order(library_name, dependencies):
    visiting = []
    visited = set()
    order = []

    def visit(name):
        if name not in dependencies:
            chain = ' -> '.join(visiting + [name])
            raise RuntimeError(f"Unknown library dependency '{name}' while resolving {chain}")
        if name in visiting:
            cycle_start = visiting.index(name)
            cycle = ' -> '.join(visiting[cycle_start:] + [name])
            raise RuntimeError(f"Dependency cycle detected: {cycle}")
        if name in visited:
            return

        visiting.append(name)
        for dependency in dependencies[name].get('direct_dependencies', []):
            visit(dependency)
        visiting.pop()

        visited.add(name)
        order.append(name)

    visit(library_name)

    dependency_order = order[:-1]
    expected_dependencies = set(dependencies[library_name].get('all_dependencies', []))
    actual_dependencies = set(dependency_order)
    if expected_dependencies != actual_dependencies:
        missing = sorted(actual_dependencies - expected_dependencies)
        extra = sorted(expected_dependencies - actual_dependencies)
        details = []
        if missing:
            details.append(f"missing from all_dependencies: {', '.join(missing)}")
        if extra:
            details.append(f"extra in all_dependencies: {', '.join(extra)}")
        raise RuntimeError(f"Dependency metadata mismatch for {library_name}: {'; '.join(details)}")

    return dependency_order

def build_library_record(root_dir, library_name, data):
    print(f'Processing library: {library_name}')

    library_files = get_library_files(root_dir, library_name)

    order_file_path = os.path.join(root_dir, 'Support', 'SingleFileLibs', f'SaneCpp{library_name}.json')

    processed_files_for_lib = set()
    authors_info_for_lib = defaultdict(int)
    spdx_set_for_lib = set()
    header_lines_by_origin = {'owned': [], 'common': []}
    sources_lines_by_origin = {'owned': [], 'common': []}

    if os.path.exists(order_file_path):
        print(f"  (Using order file: {os.path.basename(order_file_path)})")
        with open(order_file_path, 'r', encoding='utf-8') as f:
            order_data = json.load(f)
        include_order = order_data.get('includeOrder', [])
        implementation_order = order_data.get('implementationOrder', [])
        headers = amalgamate_files_recursively(library_files, include_order, root_dir, library_name,
                                               processed_files_for_lib, authors_info_for_lib, spdx_set_for_lib,
                                               header_lines_by_origin)
        sources = amalgamate_files_recursively(library_files, implementation_order, root_dir, library_name,
                                               processed_files_for_lib, authors_info_for_lib, spdx_set_for_lib,
                                               sources_lines_by_origin)
    else:
        public_headers = [f for f in library_files if f.endswith('.h') and 'Internal' not in f.split(os.sep)]
        # Make deterministic across platforms/runs
        public_headers.sort(key=lambda p: os.path.basename(p))
        public_header_basenames_for_lib = [os.path.basename(f) for f in public_headers]
        headers = amalgamate_files_recursively(library_files, public_header_basenames_for_lib, root_dir, library_name,
                                               processed_files_for_lib, authors_info_for_lib, spdx_set_for_lib,
                                               header_lines_by_origin)

        source_files = [f for f in library_files if f.endswith('.cpp')]
        # Make deterministic across platforms/runs
        source_files.sort(key=lambda p: os.path.basename(p))
        source_basenames = [os.path.basename(f) for f in source_files]
        sources = amalgamate_files_recursively(library_files, source_basenames, root_dir, library_name,
                                               processed_files_for_lib, authors_info_for_lib, spdx_set_for_lib,
                                               sources_lines_by_origin)

    header_code_loc, header_comment_loc = count_loc(headers)
    sources_code_loc, sources_comment_loc = count_loc(sources)
    header_processed_loc = split_processed_loc(header_lines_by_origin)
    sources_processed_loc = split_processed_loc(sources_lines_by_origin)

    return {
        'name': library_name,
        'data': data,
        'headers': headers,
        'sources': sources,
        'authors_info': authors_info_for_lib,
        'spdx_set': spdx_set_for_lib,
        'header_code_loc': header_code_loc,
        'header_comment_loc': header_comment_loc,
        'sources_code_loc': sources_code_loc,
        'sources_comment_loc': sources_comment_loc,
        'owned_header_loc': header_processed_loc['owned'],
        'owned_sources_loc': sources_processed_loc['owned'],
        'common_header_loc': header_processed_loc['common'],
        'common_sources_loc': sources_processed_loc['common'],
        'content_hash': calculate_content_hash(headers, sources),
    }

def aggregate_records(records):
    authors_info = defaultdict(int)
    spdx_set = set()
    header_code_loc = 0
    header_comment_loc = 0
    sources_code_loc = 0
    sources_comment_loc = 0

    for record in records:
        for author, count in record['authors_info'].items():
            authors_info[author] += count
        spdx_set.update(record['spdx_set'])
        header_code_loc += record['header_code_loc']
        header_comment_loc += record['header_comment_loc']
        sources_code_loc += record['sources_code_loc']
        sources_comment_loc += record['sources_comment_loc']

    return {
        'authors_info': authors_info,
        'spdx_set': spdx_set,
        'header_code_loc': header_code_loc,
        'header_comment_loc': header_comment_loc,
        'sources_code_loc': sources_code_loc,
        'sources_comment_loc': sources_comment_loc,
    }

def write_generated_banner(f, output_filename, library_name, dependency_text, git_version, statistics, authors_info, spdx_set,
                           standalone):
    build_kind = 'standalone single file build' if standalone else 'single file build'
    f.write(f'{DIVIDER}')
    f.write(f'// {output_filename} - Sane C++ {library_name} Library ({build_kind})\n')
    f.write(f'{DIVIDER}')
    f.write(f'// Dependencies:       {dependency_text}\n')
    f.write(f'// Version:            {git_version}\n')
    f.write(f'// LOC header:         {statistics["header_code_loc"]} (code) + {statistics["header_comment_loc"]} (comments)\n')
    f.write(f'// LOC implementation: {statistics["sources_code_loc"]} (code) + {statistics["sources_comment_loc"]} (comments)\n')
    f.write(f'// Documentation:      https://pagghiu.github.io/SaneCppLibraries\n')
    f.write(f'// Source Code:        https://github.com/pagghiu/SaneCppLibraries\n')
    f.write(f'{DIVIDER}')
    f.write(f'// All copyrights and SPDX information for this library (each amalgamated section has its own copyright attributions):\n')
    sorted_authors = sorted(authors_info.items(), key=lambda item: item[1], reverse=True)
    for author, _ in sorted_authors:
        f.write(f'// Copyright (c) {author}\n')

    if spdx_set:
        f.write(f'// SPDX-License-Identifier: {", ".join(sorted(list(spdx_set)))}\n')
    f.write(f'{DIVIDER}')

def write_library_block(f, record, embedded=False):
    library_name = record['name']
    macro_name = macro_library_name(library_name)
    included_guard = f'SANE_CPP_{macro_name}_INCLUDED'
    content_guard = f'SANE_CPP_{macro_name}_CONTENT_{record["content_hash"]}'
    define_guard = f'SANE_CPP_{macro_name}_HEADER'

    if embedded:
        f.write(f'{DIVIDER}')
        f.write(f'// Embedded SaneCpp{library_name}.h\n')
        f.write(f'{DIVIDER}')

    f.write(f'#if defined({included_guard}) && !defined({content_guard})\n')
    f.write(f'#error "SaneCpp{library_name} was already included from a different single-file version"\n')
    f.write(f'#endif\n\n')
    f.write(f'#if (defined({define_guard}) || defined(SANE_CPP_{macro_name}_IMPLEMENTATION)) && !defined({included_guard})\n')
    f.write(f'#error "SaneCpp{library_name} was already included without single-file content markers"\n')
    f.write(f'#endif\n\n')
    f.write(f'#if !defined({included_guard})\n')
    f.write(f'#define {included_guard} 1\n')
    f.write(f'#define {content_guard} 1\n')
    f.write(f'#endif\n\n')

    f.write(f'#if !defined({define_guard})\n')
    f.write(f'#define {define_guard} 1\n')
    f.write(record['headers'])
    f.write(f'\n#endif // {define_guard}\n')

    if record['sources']:
        implementation_guard = f'SANE_CPP_{macro_name}_IMPLEMENTATION'
        f.write(f'#if defined(SANE_CPP_IMPLEMENTATION) && !defined({implementation_guard})\n')
        f.write(f'#define {implementation_guard} 1\n')
        f.write(record['sources'])
        f.write(f'\n#endif // {implementation_guard}\n')

def write_regular_header(output_filepath, output_filename, record, git_version):
    dependencies = record['data'].get('all_dependencies', [])
    dependency_text = ", ".join([f"SaneCpp{dep}.h" for dep in dependencies]) if dependencies else "None"
    statistics = {
        'header_code_loc': record['header_code_loc'],
        'header_comment_loc': record['header_comment_loc'],
        'sources_code_loc': record['sources_code_loc'],
        'sources_comment_loc': record['sources_comment_loc'],
    }

    with open(output_filepath, 'w', encoding='utf-8') as f:
        write_generated_banner(f, output_filename, record['name'], dependency_text, git_version, statistics,
                               record['authors_info'], record['spdx_set'], standalone=False)

        for dep in dependencies:
            f.write(f'#include "SaneCpp{dep}.h"\n')
        if dependencies:
            f.write('\n')

        write_library_block(f, record)

def write_standalone_header(output_filepath, output_filename, record, dependency_order, records_by_name, git_version):
    ordered_records = [records_by_name[name] for name in dependency_order + [record['name']]]
    aggregate = aggregate_records(ordered_records)
    dependency_text = "Embedded: " + ", ".join([f"SaneCpp{dep}.h" for dep in dependency_order]) if dependency_order else "None"

    statistics = {
        'header_code_loc': aggregate['header_code_loc'],
        'header_comment_loc': aggregate['header_comment_loc'],
        'sources_code_loc': aggregate['sources_code_loc'],
        'sources_comment_loc': aggregate['sources_comment_loc'],
    }

    with open(output_filepath, 'w', encoding='utf-8') as f:
        write_generated_banner(f, output_filename, record['name'], dependency_text, git_version, statistics,
                               aggregate['authors_info'], aggregate['spdx_set'], standalone=True)

        for embedded_record in ordered_records:
            write_library_block(f, embedded_record, embedded=embedded_record['name'] != record['name'])

def write_test_runtime_shims(f, output_filename):
    if output_filename.startswith('SaneCppFoundation'):
        return

    f.write("""#if defined(SC_PROVIDE_CPP_RUNTIME_SHIMS) && SC_PROVIDE_CPP_RUNTIME_SHIMS
#include <stdlib.h>

#if !defined(__SANITIZE_ADDRESS__)
void operator delete(void* p) noexcept
{
    if (p != 0)
    {
        ::free(p);
    }
}

void operator delete[](void* p) noexcept
{
    if (p != 0)
    {
        ::free(p);
    }
}

void operator delete(void* p, decltype(sizeof(0))) noexcept
{
    if (p != 0)
    {
        ::free(p);
    }
}

void operator delete[](void* p, decltype(sizeof(0))) noexcept
{
    if (p != 0)
    {
        ::free(p);
    }
}

void* operator new(decltype(sizeof(0)) len) { return ::malloc(len); }
void* operator new[](decltype(sizeof(0)) len) { return ::malloc(len); }
#endif

void* __cxa_pure_virtual   = 0;
void* __gxx_personality_v0 = 0;

using guard_type = long long int;
extern "C" int __cxa_guard_acquire(guard_type* guard_object)
{
    if (*reinterpret_cast<const unsigned char*>(guard_object) != 0)
    {
        return 0;
    }
    return 1;
}

extern "C" void __cxa_guard_release(guard_type* guard_object) { *reinterpret_cast<unsigned char*>(guard_object) = 1; }
extern "C" void __cxa_guard_abort(guard_type* guard_object) { (void)(guard_object); }

#if defined(__linux__)
extern "C" int __cxa_atexit(void (*func)(void*), void* obj, void* dso_symbol);
#if defined(__GLIBC__)
extern "C" int __cxa_thread_atexit(void (*func)(void*), void* obj, void* dso_symbol)
{
    int __cxa_thread_atexit_impl(void (*)(void*), void*, void*);
    return __cxa_thread_atexit_impl(func, obj, dso_symbol);
}
#else
extern "C" int __cxa_thread_atexit(void (*func)(void*), void* obj, void* dso_symbol)
{
    return __cxa_atexit(func, obj, dso_symbol);
}
#endif
#endif
#endif

""")

def write_regular_test(test_filepath, output_filename):
    with open(test_filepath, 'w', encoding='utf-8') as f:
        write_test_runtime_shims(f, output_filename)
        f.write(f'#define SANE_CPP_IMPLEMENTATION\n')
        f.write(f'#include "{output_filename}"\n\n')
        f.write(f'int main()\n')
        f.write(f'{{\n')
        f.write(f'    return 0;\n')
        f.write(f'}}\n')

def write_standalone_test(test_filepath, output_filename):
    with open(test_filepath, 'w', encoding='utf-8') as f:
        write_test_runtime_shims(f, output_filename)
        f.write(f'#include "{output_filename}"\n\n')
        f.write(f'#define SANE_CPP_IMPLEMENTATION\n')
        f.write(f'#include "{output_filename}"\n\n')
        f.write(f'int main()\n')
        f.write(f'{{\n')
        f.write(f'    return 0;\n')
        f.write(f'}}\n')

def format_code_loc_row(label, split_loc):
    header_loc = split_loc['header']['code']
    source_loc = split_loc['source']['code']
    return f"| {label:<11} | {header_loc}\t\t| {source_loc}\t\t| {header_loc + source_loc}\t|"

def payload_split_loc(record):
    return {
        'header': make_loc(record['header_code_loc'], record['header_comment_loc']),
        'source': make_loc(record['sources_code_loc'], record['sources_comment_loc']),
    }

def common_loc_used(record):
    return sum_loc([record['common_header_loc'], record['common_sources_loc']])

def owned_processed_loc(record):
    return sum_loc([record['owned_header_loc'], record['owned_sources_loc']])

def update_library_doc_statistics(library_name, root_dir, metrics):
    """Updates the statistics section in the library's documentation markdown file."""
    doc_path = os.path.join(root_dir, 'Documentation', 'Libraries', f'{library_name}.md')
    if not os.path.exists(doc_path):
        print(f"Warning: Documentation file not found for {library_name} at {doc_path}")
        return

    with open(doc_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Remove existing statistics section
    content, num_replacements = re.subn(r'^# Statistics\n(.*\n)*?(?=\n^# |\Z)', '', content, flags=re.MULTILINE)
    if num_replacements == 0:
        content = content.rstrip()

    rows = [
        format_code_loc_row('Library', metrics['library_source']),
        format_code_loc_row('Single File', metrics['regular_payload']),
        format_code_loc_row('Standalone', metrics['standalone_payload']),
    ]

    stats_section = f"""\n\n# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/{library_name}`.\nSingle File counts
`SaneCpp{library_name}.h`.\nStandalone counts `SaneCpp{library_name}Standalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
{chr(10).join(rows)}
"""

    # Always append the statistics section at the end of the markdown page
    new_content = content.rstrip() + '\n\n' + stats_section.lstrip()

    with open(doc_path, 'w', encoding='utf-8') as f:
        f.write(new_content)
    print(f"  Updated statistics in {os.path.basename(doc_path)}")

def camel_to_snake(name):
    import re
    return re.sub(r'(?<!^)(?=[A-Z])', '_', name).lower()

def update_table_loc_in_file(file_path, lib_metrics, common_metrics, is_readme=False):
    """Updates the libraries table with owned, Common, regular single-file and standalone LOC."""
    if not os.path.isfile(file_path):
        return
    with open(file_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    # Find the table start and end
    table_start = None
    table_end = None
    for i, line in enumerate(lines):
        if (is_readme and line.strip().startswith('Library') and 'Description' in line) or \
           (not is_readme and line.strip().startswith('Library') and 'Description' in line):
            table_start = i + 2  # skip header and separator
        elif table_start is not None and (line.strip() == '' or line.startswith('Each library is color-coded')):
            table_end = i
            break
    if table_start is None:
        return  # Table not found
    if table_end is None:
        table_end = table_start + 1
        while table_end < len(lines) and lines[table_end].strip():
            table_end += 1

    table_lines = [
        "Library                                     | Description                                   | Header LOC | Source LOC | Standalone LOC\n",
        ":-------------------------------------------|:----------------------------------------------|-----------:|-----------:|---------------:\n",
    ]

    for lib_name, metrics in lib_metrics.items():
        table_lines.append(
            f"@subpage library_{camel_to_snake(lib_name):<22} | @copybrief library_{camel_to_snake(lib_name):<22} |"
            f" {metrics['library_source']['header']['code']} | {metrics['library_source']['source']['code']} |"
            f" {metrics['standalone_payload']['header']['code'] + metrics['standalone_payload']['source']['code']}\n")

    table_lines.append(
        f"Common source fragments                    | Shared source fragments, not a library       |"
        f" {common_metrics['source']['header']['code']} | {common_metrics['source']['source']['code']} | -\n")

    lines[table_start - 2:table_end] = table_lines

    with open(file_path, 'w', encoding='utf-8') as f:
        f.writelines(lines)

def update_libraries_md_table(lib_metrics, common_metrics, root_dir):
    """Updates the library LOC table in Libraries.md."""
    libraries_md_path = os.path.join(root_dir, 'Documentation', 'Pages', 'Libraries.md')
    update_table_loc_in_file(libraries_md_path, lib_metrics, common_metrics, is_readme=False)

    # readme_md_path = os.path.join(root_dir, 'README.md')
    # update_table_loc_in_file(readme_md_path, lib_line_counts, is_readme=True)

def add_total_loc_after_table(file_path, total_loc_str, is_readme=False):
    if not os.path.isfile(file_path):
        return
    with open(file_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    # Remove old total LOC table if it exists
    new_lines = []
    in_table = False
    for line in lines:
        stripped_line = line.strip()
        if stripped_line.startswith("All LOC counts in the table") and stripped_line.endswith("exclude comments."):
            continue
        if (not in_table and ((stripped_line.startswith("LOC") and "| Count" in stripped_line) or
                              (stripped_line.startswith("LOC metric") and "|" in stripped_line))):
            in_table = True
            continue

        if in_table and '|' not in stripped_line and not stripped_line.startswith(":"):
            in_table = False

        if not in_table:
            new_lines.append(line)
    lines = new_lines

    # Find the table start and end
    table_start = None
    table_end = None
    for i, line in enumerate(lines):
        if (is_readme and line.strip().startswith('Library') and 'Description' in line) or \
           (not is_readme and line.strip().startswith('Library') and 'Description' in line):
            table_start = i + 2  # skip header and separator
        elif table_start is not None and (line.strip() == '' or line.startswith('Each library is color-coded')):
            table_end = i
            break
    if table_start is None:
        return  # Table not found
    if table_end is None:
        table_end = table_start + 1
        while table_end < len(lines) and lines[table_end].strip():
            table_end += 1
    
    lines.insert(table_end, f'\n{total_loc_str}')

    with open(file_path, 'w', encoding='utf-8') as f:
        f.writelines(lines)


    
def main():

    """Main function."""
    parser = argparse.ArgumentParser(description='Create single file libraries for Sane C++.')
    parser.add_argument('--update_loc', action='store_true', help='Update LOC statistics in documentation files.')
    args = parser.parse_args()

    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
    dependencies_path = os.path.join(root_dir, 'Support', 'Dependencies', 'Dependencies.json')
    single_file_lib_dir = os.path.join(root_dir, '_Build', '_SingleFileLibraries')
    single_file_lib_test_dir = os.path.join(root_dir, '_Build', '_SingleFileLibrariesTest')

    os.makedirs(single_file_lib_dir, exist_ok=True)
    os.makedirs(single_file_lib_test_dir, exist_ok=True)

    with open(dependencies_path, 'r', encoding='utf-8') as f:
        dependencies = json.load(f)

    git_version = get_git_version()

    records_by_name = {}
    dependency_orders = {}

    for library_name, data in dependencies.items():
        records_by_name[library_name] = build_library_record(root_dir, library_name, data)

    for library_name in dependencies:
        dependency_orders[library_name] = validate_dependency_order(library_name, dependencies)

    common_metrics = {'source': count_files_loc_by_kind(get_common_files(root_dir))}
    lib_metrics = {}

    for library_name, record in records_by_name.items():
        standalone_records = [records_by_name[name] for name in dependency_orders[library_name] + [library_name]]
        standalone_payload = payload_split_loc(aggregate_records(standalone_records))
        metrics = {
            'library_source': count_files_loc_by_kind(get_library_files(root_dir, library_name)),
            'owned_processed': owned_processed_loc(record),
            'common_used': common_loc_used(record),
            'regular_payload': payload_split_loc(record),
            'standalone_payload': standalone_payload,
        }
        lib_metrics[library_name] = metrics
        if args.update_loc:
            update_library_doc_statistics(library_name, root_dir, metrics)

        output_filename = f'SaneCpp{library_name}.h'
        output_filepath = os.path.join(single_file_lib_dir, output_filename)
        write_regular_header(output_filepath, output_filename, record, git_version)

        standalone_output_filename = f'SaneCpp{library_name}Standalone.h'
        standalone_output_filepath = os.path.join(single_file_lib_dir, standalone_output_filename)
        write_standalone_header(standalone_output_filepath, standalone_output_filename, record,
                                dependency_orders[library_name], records_by_name, git_version)

        test_filename = f'Test_SaneCpp{library_name}.cpp'
        test_filepath = os.path.join(single_file_lib_test_dir, test_filename)
        write_regular_test(test_filepath, output_filename)

        standalone_test_filename = f'Test_SaneCpp{library_name}Standalone.cpp'
        standalone_test_filepath = os.path.join(single_file_lib_test_dir, standalone_test_filename)
        write_standalone_test(standalone_test_filepath, standalone_output_filename)
    if args.update_loc:
        update_libraries_md_table(lib_metrics, common_metrics, root_dir)
        library_headers = sum_loc([metrics['library_source']['header'] for metrics in lib_metrics.values()])
        library_sources = sum_loc([metrics['library_source']['source'] for metrics in lib_metrics.values()])

        def code_sum(header_loc, source_loc):
            return header_loc['code'] + source_loc['code']

        table = [
            "LOC metric                         | Header | Source | Sum",
            ":-----------------------------------|-------:|-------:|----:",
            f"Library source                      | {library_headers['code']} | {library_sources['code']} | {code_sum(library_headers, library_sources)}",
            f"Common source fragments             | {common_metrics['source']['header']['code']} | {common_metrics['source']['source']['code']} | {code_sum(common_metrics['source']['header'], common_metrics['source']['source'])}",
            "",
            "All LOC counts in the tables above exclude comments."
        ]
        total_loc_str = "\n".join(table)
        
        libraries_md_path = os.path.join(root_dir, 'Documentation', 'Pages', 'Libraries.md')
        add_total_loc_after_table(libraries_md_path, total_loc_str, is_readme=False)

        # readme_md_path = os.path.join(root_dir, 'README.md')
        # add_total_loc_after_table(readme_md_path, total_loc_str, is_readme=True)

if __name__ == '__main__':
    sys.exit(main())
