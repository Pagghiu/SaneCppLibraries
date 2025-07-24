#!/usr/bin/env python3
"""
This script scans all libraries in the Libraries/ directory of the Sane C++ Libraries project
and generates documentation about dependencies and code statistics for each library.

Features and workflow (step by step):

1. Identifies all libraries as sub-folders of Libraries/ (ignores LibrariesExtra/).
2. Recursively collects all .h, .cpp, and .inl files for each library.
3. For each library, counts lines of code (LOC):
   - Total lines (including comments and blank lines)
   - Lines excluding comments and blank lines
4. Parses each file for #include statements that reference other libraries via relative paths (e.g., #include "../../Reflection/Reflection.h").
   - Uses a regex to match any number of ../ and extract the library name that follows.
   - Only includes dependencies that are other libraries in Libraries/.
   - Skips any #include line that contains '// OPTIONAL DEPENDENCY' after the include.
5. Builds a direct dependency map for each library.
6. Computes all (direct + indirect) dependencies for each library using a transitive closure.
7. Writes the results to Documentation/Pages/Dependencies.md in Markdown format, listing for each library:
   - Direct dependencies
   - All dependencies (direct + indirect)
   - Lines of code (excluding comments)
   - Lines of code (including comments)
   - Each dependency is a Markdown link to the corresponding Doxygen page, e.g. [Foundation](@ref library_foundation), with camel case converted to snake_case for the @ref.
   - At the end, writes project-wide LOC totals and embeds a generated SVG pie chart visualizing the LOC distribution per library.
8. For each library, updates or inserts a '# Dependencies' and a '# Statistics' section in its documentation file (Documentation/Libraries/{Library}.md):
   - The '# Dependencies' section lists direct and all dependencies, using the same link format as above.
   - The '# Statistics' section lists lines of code (excluding and including comments).
   - Both sections are inserted before the '# Features' section if present, or at the end otherwise.
   - Any existing '# Dependencies' or '# Statistics' section is replaced.
   - Naming conventions are consistent between library name, file name, and link references.

Usage:
    python3 update_documentation.py [<SANE_CPP_LIBRARIES_ROOT>]

If <SANE_CPP_LIBRARIES_ROOT> is not provided, the current directory is used.

This will update Documentation/Pages/Dependencies.md, the # Dependencies and # Statistics sections of each library's documentation file, and generate an SVG pie chart with the latest dependency and code statistics information.
"""
import os
import re
import sys
from collections import defaultdict, deque
import math

if len(sys.argv) > 2:
    print("Usage: python3 update_documentation.py [<SANE_CPP_LIBRARIES_ROOT>]")
    sys.exit(1)

if len(sys.argv) == 2:
    PROJECT_ROOT = os.path.abspath(sys.argv[1])
else:
    PROJECT_ROOT = os.getcwd()

LIBRARIES_DIR = os.path.join(PROJECT_ROOT, 'Libraries')
OUTPUT_MD = os.path.join(PROJECT_ROOT, 'Documentation', 'Pages', 'Dependencies.md')
SOURCE_EXTENSIONS = {'.h', '.cpp', '.inl'}

# Step 1: Find all libraries (subfolders of Libraries/)
libraries = [name for name in os.listdir(LIBRARIES_DIR)
             if os.path.isdir(os.path.join(LIBRARIES_DIR, name))]
libraries_set = set(libraries)

# Step 2: For each library, collect all source files
# (Add: For each library, also count lines of code including and excluding comments)
def collect_source_files(lib):
    files = []
    for root, _, filenames in os.walk(os.path.join(LIBRARIES_DIR, lib)):
        for fname in filenames:
            if os.path.splitext(fname)[1] in SOURCE_EXTENSIONS:
                files.append(os.path.join(root, fname))
    return files

def count_lines_in_file(fpath):
    total = 0
    non_comment = 0
    try:
        with open(fpath, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                total += 1
                stripped = line.lstrip()
                if not stripped.startswith('//') and stripped.strip() != '':
                    non_comment += 1
    except Exception as e:
        print(f"Warning: Could not read {fpath}: {e}")
    return total, non_comment

# For each library, collect line counts
lib_line_counts = {}
for lib in libraries:
    total_lines = 0
    non_comment_lines = 0
    for fpath in collect_source_files(lib):
        t, nc = count_lines_in_file(fpath)
        total_lines += t
        non_comment_lines += nc
    lib_line_counts[lib] = (total_lines, non_comment_lines)

# Step 3: Parse includes and build direct dependency map
# Updated regex: match any number of ../ and extract the library name after
include_pattern = re.compile(r'#include\s+"(?:\.\./)+([A-Za-z0-9_]+)/')
dep_map = {lib: set() for lib in libraries}

for lib in libraries:
    for fpath in collect_source_files(lib):
        try:
            with open(fpath, 'r', encoding='utf-8', errors='ignore') as f:
                for line in f:
                    m = include_pattern.search(line)
                    if m:
                        # Skip if this include is marked as optional
                        if '// OPTIONAL DEPENDENCY' in line:
                            continue
                        dep = m.group(1)
                        if dep != lib and dep in libraries_set:
                            dep_map[lib].add(dep)
        except Exception as e:
            print(f"Warning: Could not read {fpath}: {e}")

# Step 4: Compute transitive dependencies
def compute_transitive(lib, dep_map):
    visited = set()
    stack = list(dep_map[lib])
    while stack:
        dep = stack.pop()
        if dep not in visited:
            visited.add(dep)
            stack.extend(dep_map[dep])
    return visited

transitive_map = {lib: compute_transitive(lib, dep_map) for lib in libraries}

def camel_to_snake(name):
    import re
    return re.sub(r'(?<!^)(?=[A-Z])', '_', name).lower()

def format_deps(deps):
    if not deps:
        return '*(none)*'
    return ', '.join(f'[{dep}](@ref library_{camel_to_snake(dep)})' for dep in sorted(deps))

DOC_LIBRARIES_DIR = os.path.join(PROJECT_ROOT, 'Documentation', 'Libraries')

def update_library_md(lib, direct_deps, all_deps, line_counts):
    md_path = os.path.join(DOC_LIBRARIES_DIR, f'{lib}.md')
    if not os.path.isfile(md_path):
        return  # Skip if no doc file exists
    with open(md_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Remove any existing # Dependencies section
    import re
    dep_section_pattern = re.compile(r'(^# Dependencies\n(?:.*?\n)*?)(?=^# |\Z)', re.MULTILINE)
    content = dep_section_pattern.sub('', content)

    # Remove any existing # Statistics section
    import re
    loc_section_pattern = re.compile(r'(^# Statistics\n(?:.*?\n)*?)(?=^# |\Z)', re.MULTILINE)
    content = loc_section_pattern.sub('', content)

    total_lines, non_comment_lines = line_counts

    # Prepare new dependencies section
    dep_section = f'# Dependencies\n- Direct dependencies: {format_deps(direct_deps)}\n- All dependencies: {format_deps(all_deps)}\n\n'

    # Prepare new statitics section
    loc_section = f'# Statistics\n- Lines of code (excluding comments): {non_comment_lines}\n- Lines of code (including comments): {total_lines}\n\n'

    # Find where to insert: before # Features, or at end
    features_match = re.search(r'^# Features', content, re.MULTILINE)
    if features_match:
        insert_pos = features_match.start()
        new_content = content[:insert_pos] + dep_section + loc_section + content[insert_pos:]
    else:
        new_content = content.rstrip() + '\n\n' + dep_section + loc_section

    with open(md_path, 'w', encoding='utf-8') as f:
        f.write(new_content)

def update_libraries_md_table(lib_line_counts):
    """
    Updates the 'Lines of code' column in the Libraries.md table with the correct values (excluding comments).
    """
    libraries_md_path = os.path.join(PROJECT_ROOT, 'Documentation', 'Pages', 'Libraries.md')
    if not os.path.isfile(libraries_md_path):
        return
    with open(libraries_md_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    # Find the table start and end
    table_start = None
    table_end = None
    for i, line in enumerate(lines):
        if line.strip().startswith('Library') and 'Lines of code' in line:
            table_start = i + 2  # skip header and separator
        elif table_start is not None and line.strip() == '':
            table_end = i
            break
    if table_start is None:
        return  # Table not found
    if table_end is None:
        table_end = table_start + 1
        while table_end < len(lines) and lines[table_end].strip():
            table_end += 1

    # Prepare a mapping from subpage name to library name
    subpage_to_lib = {}
    for lib in lib_line_counts:
        subpage = f'@subpage library_{camel_to_snake(lib)}'
        subpage_to_lib[subpage] = lib

    # Update the table lines
    for i in range(table_start, table_end):
        line = lines[i]
        parts = line.split('|')
        if len(parts) < 3:
            continue
        subpage = parts[0].strip()
        if subpage in subpage_to_lib:
            lib = subpage_to_lib[subpage]
            loc = lib_line_counts[lib][1]  # non-comment lines
            # Replace the last column with the correct LOC
            parts[-1] = f'   {loc}\n'
            lines[i] = '|'.join(parts)

    with open(libraries_md_path, 'w', encoding='utf-8') as f:
        f.writelines(lines)

def main():
    # Step 5: Write Markdown output
    with open(OUTPUT_MD, 'w', encoding='utf-8') as out:
        out.write(
"""@page page_dependencies Dependencies
""")
        out.write('This file describes what each library depends on. It also lists the number of lines of code (LOC) for each library, both including and excluding comments.\n\n')
        total_all_lines = 0
        total_all_non_comment = 0
        for lib in sorted(libraries):
            direct = dep_map[lib]
            all_deps = transitive_map[lib]
            total_lines, non_comment_lines = lib_line_counts[lib]
            total_all_lines += total_lines
            total_all_non_comment += non_comment_lines
            out.write(f'# [{lib}](@ref library_{camel_to_snake(lib)})\n')
            out.write(f'- Direct dependencies: {format_deps(direct)}\n')
            out.write(f'- All dependencies: {format_deps(all_deps)}\n')
            out.write(f'- Lines of code (excluding comments): {non_comment_lines}\n')
            out.write(f'- Lines of code (including comments): {total_lines}\n')
            out.write(f'\n')
            # Update the library's own .md file
            update_library_md(lib, direct, all_deps, lib_line_counts[lib])

        # Add total at the end
        out.write('---\n')
        out.write('# Project Total\n')
        out.write(f'- Total lines of code (excluding comments): {total_all_non_comment}\n')
        out.write(f'- Total lines of code (including comments): {total_all_lines}\n\n')

    # Update Libraries.md table with LOC
    update_libraries_md_table(lib_line_counts)


    print(f"Dependency file written to {OUTPUT_MD}") 

if __name__ == '__main__':
    main() 
