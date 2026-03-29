# String Operations And Encodings

## What To Teach First

- `SC::StringView` for read-only string data.
- `SC::StringBuilder` for constructing text incrementally.
- `SC::StringConverter` and `SC::StringFormat` for conversion and formatting.
- `SC::Path` for path parsing and normalization.
- `SC::Console` and `SC::CommandLine` for user-facing text I/O and argument handling.

## Best Files To Inspect

- `Libraries/Strings/StringView.h`
- `Libraries/Strings/StringBuilder.h`
- `Libraries/Strings/StringConverter.h`
- `Libraries/Strings/StringFormat.h`
- `Libraries/Strings/Path.h`
- `Libraries/Strings/Console.h`
- `Libraries/Strings/CommandLine.h`

## Best Examples

- `Tests/Libraries/Strings/StringViewTest.cpp`
- `Tests/Libraries/Strings/StringBuilderTest.cpp`
- `Tests/Libraries/Strings/StringConverterTest.cpp`
- `Tests/Libraries/Strings/StringFormatTest.cpp`
- `Tests/Libraries/Strings/ConsoleTest.cpp`
- `Tests/Libraries/Strings/CommandLineTest.cpp`

## Common Advice

- Treat `StringView` as the default non-owning answer.
- Explain encoding when the request touches OS boundaries.
- Mention that filesystem paths are returned in native encoding.
- Keep owned-string questions routed to `sane-memory`.

## Handoff

- Route owned-string or allocator questions to `sane-memory`.
- Route path-and-file questions to filesystem skills when the request becomes I/O-focused.
