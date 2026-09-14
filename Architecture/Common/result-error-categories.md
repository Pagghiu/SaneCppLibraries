# Result Error Category Registry

The machine-readable [category registry](result-error-categories.json) is the authoritative append-only assignment of
built-in structured `Result` categories. Category zero is reserved by Common for uncategorized errors. Error enums and
their optional formatters remain in the library that owns each category.

Values from `0x80000000` through `0xffffffff` are reserved for applications and external libraries. Built-in
categories must be appended to the JSON registry and must never be renumbered or reused. Run
`python3 Support/Scripts/CheckResultErrorCategories.py` after adding or changing a category; CI verifies uniqueness,
range, ordering, declaration paths, constant names, and header values.
