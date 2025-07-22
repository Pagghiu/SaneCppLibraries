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
9. Generates an SVG pie chart (Dependencies.svg) in the documentation directory, visualizing the relative size (LOC) of each library, with a legend.

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
        out.write(f'![Dependencies](Dependencies.svg)\n')

    # Generate SVG pie chart using only the Python standard library
    svg_path = os.path.join(os.path.dirname(OUTPUT_MD), 'Dependencies.svg')
    lib_names = []
    lib_values = []
    for lib in sorted(libraries):
        lib_names.append(lib)
        lib_values.append(lib_line_counts[lib][1])
    total = sum(lib_values)
    if total == 0:
        print("No data to plot in SVG pie chart.")
        return
    # Pie chart parameters
    cx, cy, r = 200, 200, 180
    colors = [
        '#e6194b', '#3cb44b', '#ffe119', '#4363d8', '#f58231', '#911eb4', '#46f0f0', '#f032e6',
        '#bcf60c', '#fabebe', '#008080', '#e6beff', '#9a6324', '#fffac8', '#800000', '#aaffc3',
        '#808000', '#ffd8b1', '#000075', '#808080', '#ffffff', '#000000'
    ]
    svg = [
        f'<svg width="700" height="530" viewBox="0 0 700 530" xmlns="http://www.w3.org/2000/svg">',
        '<style> .legend { font: 14px sans-serif; } .label { font: 12px sans-serif; } </style>',
        '<rect width="700" height="530" fill="white"/>'
    ]
    # Draw pie slices
    angle = 0
    for i, (name, value) in enumerate(zip(lib_names, lib_values)):
        if value == 0:
            continue
        frac = value / total
        theta1 = angle
        theta2 = angle + frac * 360
        x1 = cx + r * math.cos(math.radians(theta1 - 90))
        y1 = cy + r * math.sin(math.radians(theta1 - 90))
        x2 = cx + r * math.cos(math.radians(theta2 - 90))
        y2 = cy + r * math.sin(math.radians(theta2 - 90))
        large_arc = 1 if theta2 - theta1 > 180 else 0
        color = colors[i % len(colors)]
        path = (
            f'M {cx},{cy} '  # Move to center
            f'L {x1},{y1} '  # Line to start of arc
            f'A {r},{r} 0 {large_arc},1 {x2},{y2} '  # Arc
            f'Z'  # Close path
        )
        svg.append(f'<path d="{path}" fill="{color}" stroke="#222" stroke-width="1"/>')
        # Add label
        mid_angle = (theta1 + theta2) / 2
        label_r = r * 0.65
        lx = cx + label_r * math.cos(math.radians(mid_angle - 90))
        ly = cy + label_r * math.sin(math.radians(mid_angle - 90))
        percent = f"{frac*100:.1f}%"
        # Determine rotation for label
        text_angle = mid_angle
        # Normalize angle to [-180, 180) for flipping logic
        norm_angle = ((mid_angle + 90) % 360) - 180
        flip = abs(norm_angle) <= 90  # Inverted logic
        rotate_angle = text_angle + 90
        if flip:
            rotate_angle += 180
        # Determine text color based on background brightness
        def hex_to_rgb(hex_color):
            hex_color = hex_color.lstrip('#')
            return tuple(int(hex_color[i:i+2], 16) for i in (0, 2, 4))
        r_bg, g_bg, b_bg = hex_to_rgb(color)
        brightness = 0.299 * r_bg + 0.587 * g_bg + 0.114 * b_bg
        text_color = '#fff' if brightness < 128 else '#000'
        svg.append(f'<text x="{lx}" y="{ly}" class="label" text-anchor="middle" alignment-baseline="middle" transform="rotate({rotate_angle} {lx} {ly})" fill="{text_color}">{name}</text>')
        angle = theta2
    # Draw legend
    legend_x = 420
    legend_y = 20
    for i, (name, value) in enumerate(zip(lib_names, lib_values)):
        color = colors[i % len(colors)]
        svg.append(f'<rect x="{legend_x}" y="{legend_y + i*22}" width="18" height="18" fill="{color}" stroke="#222"/>')
        svg.append(f'<text x="{legend_x + 24}" y="{legend_y + 14 + i*22}" class="legend">{name} ({value})</text>')
    svg.append('</svg>')
    with open(svg_path, 'w', encoding='utf-8') as svg_file:
        svg_file.write('\n'.join(svg))
    print(f"SVG pie chart written to {svg_path}")

    print(f"Dependency file written to {OUTPUT_MD}") 

if __name__ == '__main__':
    main() 
