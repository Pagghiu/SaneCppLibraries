# Predeclared source-review obligations

- The storage owner and text lifetime are explicit and testable.
- Byte and event capacities are independently enforced and failure is distinguishable.
- No allocation is hidden in the library's normal path.
- Reset/iterator validity and failed-init state are documented and implemented consistently.
- The review grades behavior and architecture, not use of words such as “bounded” or “caller-owned”.
