"""
Module for generating output files: Markdown, JSON, DOT, and updating library docs.
"""
import os
import re
import json
import subprocess
import config


def camel_to_snake(name):
    """Converts CamelCase to snake_case."""
    return re.sub(r'(?<!^)(?=[A-Z])', '_', name).lower()


def format_deps(deps):
    """Formats dependencies as Markdown links."""
    if not deps:
        return '*(none)*'
    return ', '.join(f'[{dep}](@ref library_{camel_to_snake(dep)})' for dep in sorted(deps))


def update_library_md(project_root, lib, direct_deps, minimal_deps, all_deps):
    """Updates the # Dependencies section in a library's documentation file."""
    md_path = os.path.join(project_root, config.DOC_LIBRARIES_DIR, f'{lib}.md')
    if not os.path.isfile(md_path):
        return  # Skip if no doc file exists

    with open(md_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Prepare new section
    svg_path = f'{lib}.svg'
    dep_section = f'# Dependencies\n- Dependencies: {format_deps(minimal_deps)}\n- All dependencies: {format_deps(all_deps)}\n\n![Dependency Graph]({svg_path})\n\n\n'

    # Find existing # Dependencies section
    dep_start = content.find('# Dependencies')
    if dep_start != -1:
        # Find the end of the section (beginning of next # section)
        next_section_match = re.search(r'^# ', content[dep_start + len('# Dependencies'):-1], re.MULTILINE)
        if next_section_match:
            dep_end = dep_start + len('# Dependencies') + next_section_match.start()
        else:
            dep_end = len(content)
        # Replace the section
        new_content = content[:dep_start] + dep_section + content[dep_end:]
    else:
        # Insert before # Statistics or # Features, or at end
        stats_match = re.search(r'^# Statistics', content, re.MULTILINE)
        features_match = re.search(r'^# Features', content, re.MULTILINE)

        if stats_match:
            insert_pos = stats_match.start()
        elif features_match:
            insert_pos = features_match.start()
        else:
            insert_pos = len(content.rstrip()) + 1

        new_content = content[:insert_pos] + dep_section + content[insert_pos:]

    with open(md_path, 'w', encoding='utf-8') as f:
        f.write(new_content)


def write_markdown(project_root, libraries, dep_map, minimal_map, transitive_map):
    """Writes the main Dependencies.md file."""
    output_path = os.path.join(project_root, config.OUTPUT_MD)
    # Create output directory if it doesn't exist
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, 'w', encoding='utf-8') as out:
        out.write('@page page_dependencies Dependencies\n')
        out.write('This file describes what each library depends on.\n\n')
        out.write('# Dependencies\n\n')
        out.write('![Dependency Graph](https://pagghiu.github.io/images/dependencies/SaneCppLibrariesDependencies.svg)\n\n')
        out.write('# Dependencies (automatically generated)\n\n')
        out.write('![Dependency Graph](Dependencies.svg)\n\n\n')
        out.write('# Dependencies (interactive visualization)\n\n')
        out.write('@htmlinclude _Build/_Dependencies/Dependencies.html\n\n\n')
        for lib in sorted(libraries):
            minimal = minimal_map[lib]
            all_deps = transitive_map[lib]
            # out.write(f'# [{lib}](@ref library_{camel_to_snake(lib)})\n')
            # out.write(f'- Dependencies: {format_deps(minimal)}\n')
            # out.write(f'- All dependencies: {format_deps(all_deps)}\n')
            # out.write('\n')
            # Update library's own .md
            update_library_md(project_root, lib, dep_map[lib], minimal, all_deps)
    print(f"Dependency file written to {output_path}")


def write_json(project_root, libraries, dep_map, transitive_map, minimal_map):
    """Writes Dependencies.json."""
    output_path = os.path.join(project_root, config.OUTPUT_JSON)
    data = {}
    for lib in sorted(libraries):
        data[lib] = {
            "direct_dependencies": sorted(dep_map[lib]),
            "minimal_dependencies": sorted(minimal_map[lib]),
            "all_dependencies": sorted(transitive_map[lib])
        }
    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=4, ensure_ascii=False)
    print(f"Dependencies JSON file written to {output_path}")


def write_dot(project_root, minimal_map, rank_map, ranks):
    """Writes Dependencies.dot for Graphviz."""
    output_path = os.path.join(project_root, config.OUTPUT_DOT)
    # Create output directory if it doesn't exist
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write('digraph LibraryDependencies {\n')
        f.write('  node [shape=box];\n')
        f.write('  splines=line;\n')
        f.write('\n')
        # Nodes
        for lib in sorted(minimal_map):
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
        for lib in sorted(minimal_map):
            for dep in sorted(minimal_map[lib]):
                f.write(f'  {lib} -> {dep};\n')
        f.write('}\n')
    print(f"DOT file written to {output_path}")


def write_individual_dots(project_root, libraries, dep_map, minimal_map, transitive_map):
    """Writes individual DOT files for each library."""
    output_dir = os.path.join(project_root, '_Build', '_Dependencies')
    os.makedirs(output_dir, exist_ok=True)
    for lib in sorted(libraries):
        output_path = os.path.join(output_dir, f'{lib}.dot')
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(f'digraph {lib}Dependencies {{\n')
            f.write('  node [shape=box];\n')
            f.write('  splines=line;\n')
            f.write('\n')
            # Nodes: the library and its transitive dependencies
            all_nodes = {lib} | set(transitive_map[lib])
            for node in sorted(all_nodes):
                f.write(f'  {node};\n')
            f.write('\n')
            # Edges: lib to its minimal deps, then deps to their minimal deps
            edges = set()
            # First, lib to minimal deps
            for dep in sorted(minimal_map[lib]):
                edges.add((lib, dep))
            # Then, for all nodes in transitive deps, add their minimal deps if within subgraph
            for node in sorted(transitive_map[lib]):
                for dep in sorted(minimal_map.get(node, [])):
                    if dep in all_nodes:
                        edges.add((node, dep))
            for src, dst in sorted(edges):
                f.write(f'  {src} -> {dst};\n')
            f.write('}\n')
        print(f"Individual DOT file written to {output_path}")


def generate_svgs(project_root):
    """Generates SVG files from all DOT files using Graphviz dot command."""
    dot_dir = os.path.join(project_root, '_Build', '_Dependencies')
    if not os.path.exists(dot_dir):
        return
    for filename in os.listdir(dot_dir):
        if filename.endswith('.dot'):
            dot_path = os.path.join(dot_dir, filename)
            svg_path = dot_path.replace('.dot', '.svg')
            result = subprocess.run(['dot', '-Tsvg', dot_path, '-o', svg_path], capture_output=True, text=True)
            if result.returncode == 0:
                print(f"Generated SVG: {svg_path}")
            else:
                print(f"Failed to generate SVG for {dot_path}: {result.stderr}")
