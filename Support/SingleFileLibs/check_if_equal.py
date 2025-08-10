
import sys
import os
import filecmp

def are_dirs_equal(dir1, dir2):
    """
    Compare two directories recursively. Files in each directory are
    assumed to be equal if their names and contents are equal.
    """
    dirs_cmp = filecmp.dircmp(dir1, dir2)
    if len(dirs_cmp.left_only)>0 or len(dirs_cmp.right_only)>0 or \
        len(dirs_cmp.funny_files)>0:
        # Print the list of files on one dir
        print("Directories have different files:")
        if len(dirs_cmp.left_only) > 0:
            print(" - Only in dir1:")
            for file in dirs_cmp.left_only:
                print(f"   - {file}")
        if len(dirs_cmp.right_only) > 0:
            print(" - Only in dir2:")
            for file in dirs_cmp.right_only:
                print(f"   - {file}")
        return False
    (_, mismatch, errors) = filecmp.cmpfiles(
        dir1, dir2, dirs_cmp.common_files, shallow=False)
    if len(mismatch)>0 or len(errors)>0:
        # Print the list of files that differ
        print("Directories have files with different contents:")
        for file in mismatch:
            print(f" - {file}")
        return False
    for common_dir in dirs_cmp.common_dirs:
        new_dir1 = os.path.join(dir1, common_dir)
        new_dir2 = os.path.join(dir2, common_dir)
        if not are_dirs_equal(new_dir1, new_dir2):
            return False
    return True

def main():
    if len(sys.argv) != 3:
        print("Usage: python check_if_equal.py <dir1> <dir2>")
        sys.exit(1)

    dir1 = sys.argv[1]
    dir2 = sys.argv[2]

    if not os.path.isdir(dir1) or not os.path.isdir(dir2):
        print("Error: Both arguments must be directories.")
        sys.exit(1)

    if are_dirs_equal(dir1, dir2):
        print("The two directories are equal.")
        sys.exit(0)
    else:
        print("The two directories are NOT equal.")
        sys.exit(1)

if __name__ == "__main__":
    main()
