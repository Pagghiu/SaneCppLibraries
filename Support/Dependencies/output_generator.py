"""
Module for generating output files: Markdown, JSON, DOT, and updating library docs.
"""
import os
import re
import json
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

    # Remove existing # Dependencies section
    dep_section_pattern = re.compile(r'(^# Dependencies\n(?:.*?\n)*?)(?=^# |\Z)', re.MULTILINE)
    content = dep_section_pattern.sub('', content)

    # Prepare new section
    dep_section = f'# Dependencies\n- Dependencies: {format_deps(minimal_deps)}\n- All dependencies: {format_deps(all_deps)}\n\n'

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
    with open(output_path, 'w', encoding='utf-8') as out:
        out.write('@page page_dependencies Dependencies\n')
        out.write('This file describes what each library depends on.\n\n')
        out.write('# Interactive Dependencies\n\n')
        out.write('@htmlinclude Documentation/Pages/Dependencies.html\n\n\n')
        for lib in sorted(libraries):
            minimal = minimal_map[lib]
            all_deps = transitive_map[lib]
            out.write(f'# [{lib}](@ref library_{camel_to_snake(lib)})\n')
            out.write(f'- Dependencies: {format_deps(minimal)}\n')
            out.write(f'- All dependencies: {format_deps(all_deps)}\n')
            out.write('\n')
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
