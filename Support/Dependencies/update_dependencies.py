#!/usr/bin/env python3
"""
This script scans all libraries in the Libraries/ directory of the Sane C++ Libraries project
and generates documentation about dependencies for each library.

Features and workflow (step by step):

1. Identifies all libraries as sub-folders of Libraries/ (ignores LibrariesExtra/).
2. Recursively collects all .h, .cpp, and .inl files for each library.
3. Parses each file for #include statements that reference other libraries via relative paths (e.g., #include "../../Reflection/Reflection.h").
   - Uses a regex to match any number of ../ and extract the library name that follows.
   - Only includes dependencies that are other libraries in Libraries/.
   - Skips any #include line that contains '// OPTIONAL DEPENDENCY' after the include.
4. Builds a direct dependency map for each library.
5. Computes all (direct + indirect) dependencies for each library using a transitive closure.
6. Computes minimal dependencies for each library (minimal set of direct dependencies that are not implied by others).
7. Writes the results to Documentation/Pages/Dependencies.md in Markdown format, listing for each library:
   - Direct dependencies
   - Minimal dependencies
   - All dependencies (direct + indirect)
   - Each dependency is a Markdown link to the corresponding Doxygen page, e.g. [Foundation](@ref library_foundation), with camel case converted to snake_case for the @ref.
8. For each library, updates or inserts a '# Dependencies' section in its documentation file (Documentation/Libraries/{Library}.md):
   - The '# Dependencies' section lists direct, minimal, and all dependencies, using the same link format as above.
   - Existing '# Dependencies' sections are replaced.
   - Naming conventions are consistent between library name, file name, and link references.
9. Writes dependencies to Support/Dependencies/Dependencies.json in JSON format.
10. Computes ranks for layering libraries in the dependency graph (Foundation at bottom, then layers based on minimal dependencies).
11. Writes dependencies to Documentation/Pages/Dependencies.dot in DOT format with layered ranks for visualization using Graphviz (you can generate an image with: `dot -Tpng Documentation/Pages/Dependencies.dot -o Documentation/Pages/Dependencies.png`).

Usage:
    python3 update_dependencies.py [<SANE_CPP_LIBRARIES_ROOT>]

If <SANE_CPP_LIBRARIES_ROOT> is not provided, the current directory is used.

This will update Documentation/Pages/Dependencies.md, the # Dependencies sections of each library's documentation file, create Support/Dependencies/Dependencies.json, and generate Documentation/Pages/Dependencies.dot for graph visualization.
"""
import os
import re
import sys
import json
from collections import defaultdict

if len(sys.argv) > 2:
    print("Usage: python3 update_dependencies.py [<SANE_CPP_LIBRARIES_ROOT>]")
    sys.exit(1)

if len(sys.argv) == 2:
    PROJECT_ROOT = os.path.abspath(sys.argv[1])
else:
    PROJECT_ROOT = os.getcwd()

LIBRARIES_DIR = os.path.join(PROJECT_ROOT, 'Libraries')
OUTPUT_MD = os.path.join(PROJECT_ROOT, 'Documentation', 'Pages', 'Dependencies.md')
OUTPUT_JSON = os.path.join(PROJECT_ROOT, 'Support', 'Dependencies', 'Dependencies.json')
SOURCE_EXTENSIONS = {'.h', '.cpp', '.inl'}

# Step 1: Find all libraries (sub-folders of Libraries/)
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

# Step 5: Compute minimal dependencies (minimal set of direct dependencies)
def compute_minimal(lib, dep_map, transitive_map):
    direct = dep_map[lib]
    minimal = set()
    for dep in direct:
        is_implied = False
        for other_dep in direct:
            if other_dep != dep and dep in transitive_map[other_dep]:
                is_implied = True
                break
        if not is_implied:
            minimal.add(dep)
    return minimal

minimal_map = {lib: compute_minimal(lib, dep_map, transitive_map) for lib in libraries}

def camel_to_snake(name):
    import re
    return re.sub(r'(?<!^)(?=[A-Z])', '_', name).lower()

def format_deps(deps):
    if not deps:
        return '*(none)*'
    return ', '.join(f'[{dep}](@ref library_{camel_to_snake(dep)})' for dep in sorted(deps))

DOC_LIBRARIES_DIR = os.path.join(PROJECT_ROOT, 'Documentation', 'Libraries')

def update_library_md(lib, direct_deps, minimal_deps, all_deps):
    md_path = os.path.join(DOC_LIBRARIES_DIR, f'{lib}.md')
    if not os.path.isfile(md_path):
        return  # Skip if no doc file exists
    with open(md_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Remove any existing # Dependencies section
    dep_section_pattern = re.compile(r'(^# Dependencies\n(?:.*?\n)*?)(?=^# |\Z)', re.MULTILINE)
    content = dep_section_pattern.sub('', content)

    # Prepare new dependencies section
    dep_section = f'# Dependencies\n- Dependencies: {format_deps(minimal_deps)}\n- All dependencies: {format_deps(all_deps)}\n\n'

    # Find where to insert:
    # 1. Before # Statistics if it exists
    # 2. Before # Features if it exists
    # 3. At the end otherwise
    stats_match = re.search(r'^# Statistics', content, re.MULTILINE)
    features_match = re.search(r'^# Features', content, re.MULTILINE)

    if stats_match:
        insert_pos = stats_match.start()
    elif features_match:
        insert_pos = features_match.start()
    else:
        insert_pos = len(content.rstrip()) + 1  # +1 for newline

    new_content = content[:insert_pos] + dep_section + content[insert_pos:]

    with open(md_path, 'w', encoding='utf-8') as f:
        f.write(new_content)

def write_dependencies_json(dep_map, transitive_map, minimal_map):
    """
    Writes dependencies to Support/Dependencies/Dependencies.json in JSON format.
    """
    dependencies_data = {}

    for lib in sorted(libraries):
        direct_deps = list(dep_map[lib])
        all_deps = list(transitive_map[lib])
        minimal_deps = list(minimal_map[lib])

        dependencies_data[lib] = {
            "direct_dependencies": sorted(direct_deps),
            "minimal_dependencies": sorted(minimal_deps),
            "all_dependencies": sorted(all_deps)
        }

    with open(OUTPUT_JSON, 'w', encoding='utf-8') as f:
        json.dump(dependencies_data, f, indent=4, ensure_ascii=False)

    print(f"Dependencies JSON file written to {OUTPUT_JSON}")

def write_dot(minimal_map, rank_map, ranks):
    dot_path = os.path.join(PROJECT_ROOT, 'Documentation', 'Pages', 'Dependencies.dot')
    with open(dot_path, 'w', encoding='utf-8') as f:
        f.write('digraph LibraryDependencies {\n')
        f.write('  node [shape=box];\n')
        f.write('  splines=line;\n')
        f.write('\n')
        # Nodes
        for lib in sorted(libraries):
            f.write(f'  {lib};\n')
        f.write('\n')
        # Ranks
        for rank in sorted(ranks.keys()):
            f.write(f'  subgraph rank{rank} {{\n')
            f.write('    rank=same;\n')
            for lib in sorted(ranks[rank]):
                f.write(f'    {lib};\n')
            f.write('  }\n')
        f.write('\n')
        # Edges
        for lib in sorted(libraries):
            for dep in sorted(minimal_map[lib]):
                f.write(f'  {lib} -> {dep};\n')
        f.write('}\n')
    print(f"DOT file written to {dot_path}")

def main():
    # Write Markdown output
    with open(OUTPUT_MD, 'w', encoding='utf-8') as out:
        out.write(
"""@page page_dependencies Dependencies
""")
        out.write('This file describes what each library depends on.\n\n')
        for lib in sorted(libraries):
            direct = dep_map[lib]
            minimal = minimal_map[lib]
            all_deps = transitive_map[lib]
            out.write(f'# [{lib}](@ref library_{camel_to_snake(lib)})\n')
            out.write(f'- Dependencies: {format_deps(minimal)}\n')
            out.write(f'- All dependencies: {format_deps(all_deps)}\n')
            out.write(f'\n')
            # Update the library's own .md file
            update_library_md(lib, direct, minimal, all_deps)
    
    # Write dependencies to JSON file
    write_dependencies_json(dep_map, transitive_map, minimal_map)

    # Compute ranks for layering
    rank_map = {}
    def get_rank(lib):
        if lib in rank_map:
            return rank_map[lib]
        if not minimal_map[lib]:
            rank_map[lib] = 0
            return 0
        deps_ranks = [get_rank(dep) for dep in minimal_map[lib]]
        rank = max(deps_ranks) + 1 if deps_ranks else 0
        rank_map[lib] = rank
        return rank

    for lib in libraries:
        get_rank(lib)

    ranks = defaultdict(list)
    for lib, rank in rank_map.items():
        ranks[rank].append(lib)

    # Write DOT file with layers
    write_dot(minimal_map, rank_map, ranks)

    print(f"Dependency file written to {OUTPUT_MD}")

if __name__ == '__main__':
    main()
