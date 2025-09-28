"""
Configuration constants for the update_dependencies script.
"""
import os

# Default extensions for source files
SOURCE_EXTENSIONS = {'.h', '.cpp', '.inl'}

# Regex pattern for matching include statements
INCLUDE_PATTERN = r'#include\s+"(?:\.\./)+([A-Za-z0-9_]+)/'

# Optional dependency marker
OPTIONAL_DEPENDENCY_MARKER = '// OPTIONAL DEPENDENCY'

# Output file paths (relative to project root)
OUTPUT_MD = os.path.join('Documentation', 'Pages', 'Dependencies.md')
OUTPUT_JSON = os.path.join('Support', 'Dependencies', 'Dependencies.json')
OUTPUT_DOT = os.path.join('Documentation', 'Pages', 'Dependencies.dot')

# Directories
LIBRARIES_DIR = 'Libraries'
DOC_LIBRARIES_DIR = os.path.join('Documentation', 'Libraries')

# Libraries to ignore (e.g., LibrariesExtra)
IGNORED_LIBRARIES = {'LibrariesExtra'}
