# Result Error Category Registry

This is the append-only registry for built-in structured `Result` categories. Category zero is reserved by Common for
uncategorized errors. Error enums and their optional formatters remain in the library that owns each category.

| Value | Owner     | Public declaration                |
|------:|-----------|-----------------------------------|
| 0     | Common    | Reserved: uncategorized           |
| 1     | Threading | `Libraries/Threading/Threading.h` |

Values from `0x80000000` through `0xffffffff` are reserved for applications and external libraries. Built-in
categories must be added at the end of this table and must never be renumbered or reused.
