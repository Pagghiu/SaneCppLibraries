"""
Module for computing transitive and minimal dependencies.
"""
from collections import defaultdict


def compute_transitive_dependencies(dep_map):
    """
    Computes transitive (all) dependencies for each library.
    """
    def get_transitive(lib):
        visited = set()
        stack = list(dep_map[lib])
        while stack:
            dep = stack.pop()
            if dep not in visited:
                visited.add(dep)
                stack.extend(dep_map[dep])
        return visited

    return {lib: get_transitive(lib) for lib in dep_map}


def compute_minimal_dependencies(dep_map, transitive_map):
    """
    Computes minimal dependencies (minimal set of direct dependencies not implied by others).
    """
    def get_minimal(lib):
        direct = dep_map[lib]
        minimal = set()
        for dep in direct:
            is_implied = any(dep in transitive_map[other] for other in direct if other != dep)
            if not is_implied:
                minimal.add(dep)
        return minimal

    return {lib: get_minimal(lib) for lib in dep_map}


def compute_ranks(minimal_map):
    """
    Computes ranks for layering libraries in the dependency graph.
    """
    rank_map = {}

    def get_rank(lib):
        if lib in rank_map:
            return rank_map[lib]
        if not minimal_map[lib]:
            rank_map[lib] = 0
            return 0
        deps_ranks = [get_rank(dep) for dep in minimal_map[lib]]
        rank = max(deps_ranks) + 1 if deps_ranks else 0
        rank_map[lib] = rank
        return rank

    for lib in minimal_map:
        get_rank(lib)

    ranks = defaultdict(list)
    for lib, rank in rank_map.items():
        ranks[rank].append(lib)

    return rank_map, ranks
