set print pretty on
python
import sys
import os.path

# De-hardcoded path resolution for SC GDB pretty printers
# This works by finding the GDB directory relative to the current working directory

def find_gdb_directory():
    """Find the GDB directory containing SCGDB.py"""
    current_dir = os.getcwd()

    # Method 1: Check if we're in the project root
    if os.path.exists(os.path.join(current_dir, 'SC.cpp')):
        return os.path.join(current_dir, 'Support', 'DebugVisualizers', 'GDB')

    # Method 2: Check if we're in the GDB directory itself
    if os.path.exists(os.path.join(current_dir, 'SCGDB.py')):
        return current_dir

    # Method 3: Walk up the directory tree looking for project root
    search_dir = current_dir
    for _ in range(5):  # Don't go up more than 5 levels
        if os.path.exists(os.path.join(search_dir, 'SC.cpp')):
            return os.path.join(search_dir, 'Support', 'DebugVisualizers', 'GDB')
        search_dir = os.path.dirname(search_dir)
        if search_dir == os.path.dirname(search_dir):  # Reached root
            break

    # Method 4: Fallback - assume we're in a standard location
    # Try relative to current directory
    candidate = os.path.join(current_dir, 'Support', 'DebugVisualizers', 'GDB')
    if os.path.exists(os.path.join(candidate, 'SCGDB.py')):
        return candidate

    # Last resort: return current directory
    return current_dir

gdbinit_dir = find_gdb_directory()
sys.path.insert(0, gdbinit_dir)
from SCGDB import *
end
