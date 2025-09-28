"""
Module for parsing include statements and building dependency maps.
"""
import re
import config
import library_scanner


def build_dependency_map(project_root, libraries):
    """
    Parses include statements in source files to build a direct dependency map.
    """
    libraries_set = set(libraries)
    dep_map = {lib: set() for lib in libraries}
    include_regex = re.compile(config.INCLUDE_PATTERN)

    for lib in libraries:
        files = library_scanner.collect_source_files(project_root, lib)
        for fpath in files:
            try:
                with open(fpath, 'r', encoding='utf-8', errors='ignore') as f:
                    for line in f:
                        match = include_regex.search(line)
                        if match:
                            # Skip if marked as optional
                            if config.OPTIONAL_DEPENDENCY_MARKER in line:
                                continue
                            dep = match.group(1)
                            if dep != lib and dep in libraries_set:
                                dep_map[lib].add(dep)
            except Exception as e:
                print(f"Warning: Could not read {fpath}: {e}")
    return dep_map
