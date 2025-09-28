"""
Module for scanning libraries and collecting source files.
"""
import os
import config


def find_libraries(project_root):
    """
    Finds all libraries as sub-folders of Libraries/, ignoring specified ones.
    """
    libraries_dir = os.path.join(project_root, config.LIBRARIES_DIR)
    libraries = [name for name in os.listdir(libraries_dir)
                 if os.path.isdir(os.path.join(libraries_dir, name)) and name not in config.IGNORED_LIBRARIES]
    return libraries


def collect_source_files(project_root, lib):
    """
    Collects all source files (.h, .cpp, .inl) for a given library.
    """
    files = []
    lib_dir = os.path.join(project_root, config.LIBRARIES_DIR, lib)
    for root, _, filenames in os.walk(lib_dir):
        for fname in filenames:
            if os.path.splitext(fname)[1] in config.SOURCE_EXTENSIONS:
                files.append(os.path.join(root, fname))
    return files
