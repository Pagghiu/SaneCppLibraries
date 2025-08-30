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

def _process_file_recursively(file_path, processed_files, root_dir, current_library_name, authors_info, spdx_set):
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
                        if included_library_name == current_library_name:
                            file_content += _process_file_recursively(normalized_abs_included_path, processed_files, root_dir, current_library_name, authors_info, spdx_set)
                        else:
                            # This is an include from another library, so we strip it.
                            pass
                        continue
                
                file_content += line
    except Exception as e:
        print(f"Error processing file {file_path}: {e}")
        return "" # Return empty string on error
    file_content = file_content.rstrip() + '\n\n'
    return file_content

def amalgamate_files_recursively(library_files, order_list, root_dir, library_name, processed_files, authors_info, spdx_set):
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
        content += _process_file_recursively(full_path, processed_files, root_dir, library_name, authors_info, spdx_set)
    return content

def update_library_doc_statistics(library_name, root_dir, header_code_loc, header_comment_loc, sources_code_loc, sources_comment_loc):
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

    total_loc = header_code_loc + header_comment_loc + sources_code_loc + sources_comment_loc
    total_code_loc = header_code_loc + sources_code_loc
    total_comment_loc = header_comment_loc + sources_comment_loc
    
    stats_section = f"""\n\n# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | {header_code_loc}\t\t\t| {header_comment_loc}\t\t| {header_code_loc + header_comment_loc}\t|
| Sources   | {sources_code_loc}\t\t\t| {sources_comment_loc}\t\t| {sources_code_loc + sources_comment_loc}\t|
| Sum       | {total_code_loc}\t\t\t| {total_comment_loc}\t\t| {total_loc}\t|
"""

    # Find dependencies section to insert after
    dep_section_pattern = r'(^# Dependencies\n(?:.*?\n)*?)(?=\n#|\Z)'
    dep_match = re.search(dep_section_pattern, content, re.MULTILINE)

    if dep_match:
        dep_section = dep_match.group(0)
        new_content = content.replace(dep_section, dep_section.rstrip() + stats_section)
    else:
        # If no dependencies section, append at the end
        new_content = content.rstrip() + '\n\n' + stats_section.lstrip()

    with open(doc_path, 'w', encoding='utf-8') as f:
        f.write(new_content)
    print(f"  Updated statistics in {os.path.basename(doc_path)}")

def camel_to_snake(name):
    import re
    return re.sub(r'(?<!^)(?=[A-Z])', '_', name).lower()

def update_table_loc_in_file(file_path, lib_line_counts, is_readme=False):
    """
    Updates the 'LOC' column in the libraries table in the given file with the correct values (excluding comments).
    If is_readme is True, expects the table format as in README.md.
    """
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


    # Build a normalized name map for matching
    def normalize(name):
        return name.replace(' ', '').replace('_', '').lower()

    lib_name_map = {normalize(lib): lib for lib in lib_line_counts}

    # Update the table lines
    for i in range(table_start, table_end):
        line = lines[i]
        parts = line.split('|')
        if len(parts) < 3:
            continue
        lib_col = parts[0].strip()
        if is_readme:
            m = re.match(r'\[([A-Za-z0-9_ ]+)\]', lib_col)
            if m:
                lib_name_raw = m.group(1)
                lib_name_norm = normalize(lib_name_raw)
                lib_name = lib_name_map.get(lib_name_norm)
                if not lib_name:
                    continue
            else:
                continue
        else:
            # In Libraries.md, the first column is @subpage library_xxx
            lib_name = None
            for lib in lib_line_counts:
                if f'@subpage library_{camel_to_snake(lib)}' == lib_col:
                    lib_name = lib
                    break
            if not lib_name:
                continue
        if lib_name in lib_line_counts:
            loc = lib_line_counts[lib_name][1]  # non-comment lines
            parts[-1] = f'   {loc}\n'
            lines[i] = '|'.join(parts)

    with open(file_path, 'w', encoding='utf-8') as f:
        f.writelines(lines)

def update_libraries_md_table(lib_line_counts, root_dir):
    """
    Updates the 'Lines of code' column in the Libraries.md table with the correct values (excluding comments).
    Also updates the table in README.md.
    """
    libraries_md_path = os.path.join(root_dir, 'Documentation', 'Pages', 'Libraries.md')
    update_table_loc_in_file(libraries_md_path, lib_line_counts, is_readme=False)

    readme_md_path = os.path.join(root_dir, 'README.md')
    update_table_loc_in_file(readme_md_path, lib_line_counts, is_readme=True)

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
        if not in_table and stripped_line.startswith("LOC") and "| Count" in stripped_line:
            in_table = True
        
        if not in_table:
            new_lines.append(line)

        if in_table and stripped_line.startswith("*Total*"):
            in_table = False
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

    lib_line_counts = {}
    total_header_loc = 0
    total_sources_loc = 0
    total_comment_loc = 0

    for library_name, data in dependencies.items():

        print(f'Processing library: {library_name}')

        library_files = get_library_files(root_dir, library_name)
        
        order_file_path = os.path.join(root_dir, 'Support', 'SingleFileLibs', f'SaneCpp{library_name}.json')
        
        processed_files_for_lib = set()
        authors_info_for_lib = defaultdict(int)
        spdx_set_for_lib = set()

        if os.path.exists(order_file_path):
            print(f"  (Using order file: {os.path.basename(order_file_path)})")
            with open(order_file_path, 'r', encoding='utf-8') as f:
                order_data = json.load(f)
            include_order = order_data.get('includeOrder', [])
            implementation_order = order_data.get('implementationOrder', [])
            headers = amalgamate_files_recursively(library_files, include_order, root_dir, library_name, processed_files_for_lib, authors_info_for_lib, spdx_set_for_lib)
            sources = amalgamate_files_recursively(library_files, implementation_order, root_dir, library_name, processed_files_for_lib, authors_info_for_lib, spdx_set_for_lib)
        else:
            public_headers = [f for f in library_files if f.endswith('.h') and 'Internal' not in f.split(os.sep)]
            # Make deterministic across platforms/runs
            public_headers.sort(key=lambda p: os.path.basename(p))
            public_header_basenames_for_lib = [os.path.basename(f) for f in public_headers]
            headers = amalgamate_files_recursively(library_files, public_header_basenames_for_lib, root_dir, library_name, processed_files_for_lib, authors_info_for_lib, spdx_set_for_lib)

            source_files = [f for f in library_files if f.endswith('.cpp')]
            # Make deterministic across platforms/runs
            source_files.sort(key=lambda p: os.path.basename(p))
            source_basenames = [os.path.basename(f) for f in source_files]
            sources = amalgamate_files_recursively(library_files, source_basenames, root_dir, library_name, processed_files_for_lib, authors_info_for_lib, spdx_set_for_lib)

        output_filename = f'SaneCpp{library_name}.h'
        output_filepath = os.path.join(single_file_lib_dir, output_filename)

        header_code_loc, header_comment_loc = count_loc(headers)
        sources_code_loc, sources_comment_loc = count_loc(sources)
        total_header_loc += header_code_loc
        total_sources_loc += sources_code_loc
        total_comment_loc += header_comment_loc + sources_comment_loc
        lib_line_counts[library_name] = (header_code_loc + sources_code_loc, header_code_loc + sources_code_loc)
        if args.update_loc:
            update_library_doc_statistics(library_name, root_dir, header_code_loc, header_comment_loc, sources_code_loc, sources_comment_loc)


        with open(output_filepath, 'w', encoding='utf-8') as f:
            f.write(f'{DIVIDER}')
            f.write(f'// SaneCpp{library_name}.h - Sane C++ {library_name} Library (single file build)\n')
            f.write(f'{DIVIDER}')
            if data.get('all_dependencies'):
                f.write(f'// Dependencies:       {", ".join([f"SaneCpp{dep}.h" for dep in data["all_dependencies"]] )}\n')
            else:
                f.write(f'// Dependencies:       None\n')
            f.write(f'// Version:            {git_version}\n')
            f.write(f'// LOC header:         {header_code_loc} (code) + {header_comment_loc} (comments)\n')
            f.write(f'// LOC implementation: {sources_code_loc} (code) + {sources_comment_loc} (comments)\n')
            f.write(f'// Documentation:      https://pagghiu.github.io/SaneCppLibraries\n')
            f.write(f'// Source Code:        https://github.com/pagghiu/SaneCppLibraries\n')
            f.write(f'{DIVIDER}')
            # Write Copyrights and SPDX
            f.write(f'// All copyrights and SPDX information for this library (each amalgamated section has its own copyright attributions):\n')
            sorted_authors = sorted(authors_info_for_lib.items(), key=lambda item: item[1], reverse=True)
            for author, _ in sorted_authors:
                f.write(f'// Copyright (c) {author}\n')
            
            if spdx_set_for_lib:
                f.write(f'// SPDX-License-Identifier: {", ".join(sorted(list(spdx_set_for_lib)))}\n')
            f.write(f'{DIVIDER}')

            for dep in data.get('all_dependencies', []):
                f.write(f'#include "SaneCpp{dep}.h"\n')
            if data.get('all_dependencies'):
                f.write('\n')

            define_guard = f'SANE_CPP_{library_name.upper()}_HEADER'
            f.write(f'#if !defined({define_guard})\n')
            f.write(f'#define {define_guard} 1\n')
            f.write(headers)
            f.write(f'\n#endif // {define_guard}\n')

            if sources:
                implementation_guard = f'SANE_CPP_{library_name.upper()}_IMPLEMENTATION'
                f.write(f'#if defined(SANE_CPP_IMPLEMENTATION) && !defined({implementation_guard})\n')
                f.write(f'#define {implementation_guard} 1\n')
                f.write(sources)
                f.write(f'\n#endif // {implementation_guard}\n')

        # Create test file
        test_filename = f'Test_SaneCpp{library_name}.cpp'
        test_filepath = os.path.join(single_file_lib_test_dir, test_filename)
        with open(test_filepath, 'w', encoding='utf-8') as f:
            f.write(f'#define SANE_CPP_IMPLEMENTATION\n')
            f.write(f'#include "{output_filename}"\n\n')
            f.write(f'int main()\n')
            f.write(f'{{\n')
            f.write(f'    return 0;\n')
            f.write(f'}}\n')
    if args.update_loc:
        update_libraries_md_table(lib_line_counts, root_dir)
        total_loc_str_sum = f"{total_header_loc + total_sources_loc + total_comment_loc}"

        table = [
            "LOC               | Count",
            ":-----------------|:-----------------",
            f"Header            | {total_header_loc}",
            f"Implementation    | {total_sources_loc}",
            f"Comments          | {total_comment_loc}",
            f"*Total*           | {total_loc_str_sum}"
        ]
        total_loc_str = "\n".join(table)
        
        libraries_md_path = os.path.join(root_dir, 'Documentation', 'Pages', 'Libraries.md')
        add_total_loc_after_table(libraries_md_path, total_loc_str, is_readme=False)

        readme_md_path = os.path.join(root_dir, 'README.md')
        add_total_loc_after_table(readme_md_path, total_loc_str, is_readme=True)

if __name__ == '__main__':
    sys.exit(main())