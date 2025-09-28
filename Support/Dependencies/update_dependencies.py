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
12. Writes dependencies to Documentation/Pages/Dependencies.html as an interactive HTML graph using vis.js with layered layout, click highlighting, and multiple selection support.

Usage:
    python3 update_dependencies.py [<SANE_CPP_LIBRARIES_ROOT>]

If <SANE_CPP_LIBRARIES_ROOT> is not provided, the current directory is used.

This will update Documentation/Pages/Dependencies.md, the # Dependencies sections of each library's documentation file, create Support/Dependencies/Dependencies.json, and generate Documentation/Pages/Dependencies.dot for graph visualization.
"""
import os
import sys

import library_scanner
import include_parser
import dependency_calculator
import output_generator
import interactive_dependencies


def main():
    if len(sys.argv) > 2:
        print("Usage: python3 update_dependencies.py [<SANE_CPP_LIBRARIES_ROOT>]")
        sys.exit(1)

    if len(sys.argv) == 2:
        PROJECT_ROOT = os.path.abspath(sys.argv[1])
    else:
        # Assume the script is in Support/Dependencies, project root is two levels up
        script_dir = os.path.dirname(os.path.abspath(__file__))
        PROJECT_ROOT = os.path.dirname(os.path.dirname(script_dir))

    # Find libraries
    libraries = library_scanner.find_libraries(PROJECT_ROOT)

    # Build dependency map
    dep_map = include_parser.build_dependency_map(PROJECT_ROOT, libraries)

    # Compute transitive and minimal dependencies
    transitive_map = dependency_calculator.compute_transitive_dependencies(dep_map)
    minimal_map = dependency_calculator.compute_minimal_dependencies(dep_map, transitive_map)

    # Compute reverse dependencies
    reverse_map = dependency_calculator.compute_reverse_dependencies(dep_map)

    # Compute ranks
    rank_map, ranks = dependency_calculator.compute_ranks(minimal_map)

    # Write outputs
    output_generator.write_markdown(PROJECT_ROOT, libraries, dep_map, minimal_map, transitive_map)
    output_generator.write_json(PROJECT_ROOT, libraries, dep_map, transitive_map, minimal_map)
    output_generator.write_dot(PROJECT_ROOT, minimal_map, rank_map, ranks)
    interactive_dependencies.write_interactive_html(PROJECT_ROOT, libraries, dep_map, minimal_map, transitive_map, reverse_map, rank_map, ranks)


if __name__ == '__main__':
    main()
