# Result Error Formatting Contract

Canonical messages are optional presentation data owned by each error category. A library keeps its error enum and
category in its mandatory public header and puts canonical English text in a separate opt-in formatter header. Generic
code may print numeric category and error values without depending on any formatter. Applications may switch on the
same public enums and use `ResultErrorFormatter` with translated text instead.

`ResultErrorFormatter` writes UTF-8 into caller-owned `Span<char>` storage. A successful output is null terminated and
`requiredCapacity` includes that terminator. An empty output span is an exact sizing query. Undersized output is not
partially formatted: its first byte is cleared when present and the returned status is `InsufficientCapacity`.
Formatting status deliberately does not use `Result`, avoiding recursive errors while reporting errors.

A category formatter distinguishes these cases:

- success input: `NotAnError`;
- an error owned by another category: `ForeignCategory`;
- an unknown value in the expected category: `UnknownError`;
- a known error with insufficient output: `InsufficientCapacity` and its exact required capacity;
- a known error written completely: `Success` and its exact required capacity.

Canonical English messages use sentence case, no library or function prefix, and no terminal period. Prefer a concrete
condition over “failed” when the cause is known. Use stable terminology such as “thread pool”, “bytes”, “capacity”, and
“storage”. Context uses labelled values with explicit units where applicable. User-controlled text must be clearly
quoted or escaped; native error numbers must be labelled and must not replace the portable category/error identity.

The opt-in header is intentionally absent from a library's normal umbrella header. This gives static, source, and
single-file consumers a structural way to omit canonical messages. Shared libraries that expose canonical formatting
must likewise place it in an optional artifact or require the application to instantiate the header implementation;
an exported formatter compiled into a mandatory shared library cannot promise omission.
