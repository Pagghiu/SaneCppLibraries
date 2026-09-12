# Predeclared source-review obligations

- The traversal state passed to the iterator is caller-owned and has a documented bound.
- Iterator errors are checked after enumeration; normal end is not treated as every error.
- Public/project source uses public Sane APIs, with no `Internal` or test-header dependency.
- Output buffers/paths have a stated capacity or visible allocation policy.
