"""
Module for checking dependencies against existing Dependencies.json.
"""
import json
import os


def check_dependencies(project_root, libraries, dep_map, minimal_map, transitive_map):
    """
    Check if computed dependencies match existing Dependencies.json.
    Prints detailed changes if any, and returns True if passed, False if failed.
    """
    # Generate computed data
    computed_data = {}
    for lib in sorted(libraries):
        computed_data[lib] = {
            "direct_dependencies": sorted(dep_map[lib]),
            "minimal_dependencies": sorted(minimal_map[lib]),
            "all_dependencies": sorted(transitive_map[lib])
        }

    json_path = os.path.join(project_root, 'Support', 'Dependencies', 'Dependencies.json')
    if not os.path.exists(json_path):
        print("Dependencies check failed: Dependencies.json does not exist.")
        return False

    with open(json_path, 'r', encoding='utf-8') as f:
        existing_data = json.load(f)

    changes = []
    for lib in sorted(libraries):
        if lib not in existing_data:
            changes.append(f"New library: {lib}")
            continue
        for key in ["direct_dependencies", "minimal_dependencies", "all_dependencies"]:
            existing_deps = set(existing_data[lib].get(key, []))
            computed_deps = set(computed_data[lib].get(key, []))
            added = computed_deps - existing_deps
            removed = existing_deps - computed_deps
            if added or removed:
                changes.append(f"{lib} ({key}):")
                if added:
                    changes.append(f"  Added: {sorted(added)}")
                if removed:
                    changes.append(f"  Removed: {sorted(removed)}")

    if not changes:
        print("Dependencies check passed: no changes detected.")
        return True
    else:
        print("Dependencies check failed: changes detected.")
        for change in changes:
            print(f"  {change}")
        return False
