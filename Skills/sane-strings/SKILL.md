---
name: sane-strings
description: Sane C++ string formatting, conversion, UTF handling, path manipulation, and console helpers. Use when working with StringView, StringBuilder, converters, command line parsing, or platform path behavior.
---

# Sane Strings

## Overview

Use this skill when the user needs formatting, conversion, encoding-aware string work, or path manipulation. Keep the guidance tied to non-owning string handling and the public string helpers.

## Use This Skill When

- A request asks about `StringView`, `StringBuilder`, or string conversion.
- A request asks how to handle ASCII, UTF-8, or UTF-16 in Sane code.
- A request asks how to parse or normalize paths.
- A request asks how `Console` or `CommandLine` fits into the string stack.

## Start Here

- Read [references/string-operations-and-encodings.md](references/string-operations-and-encodings.md).
- Inspect `Libraries/Strings/StringView.h`, `StringBuilder.h`, `StringConverter.h`, `StringFormat.h`, `Path.h`, `Console.h`, and `CommandLine.h`.
- Use `Tests/Libraries/Strings/StringViewTest.cpp`, `StringBuilderTest.cpp`, `StringConverterTest.cpp`, `StringFormatTest.cpp`, `ConsoleTest.cpp`, and `CommandLineTest.cpp`.

## Key Guidance

- Treat `StringView` as the default read-only string type.
- Use `StringBuilder` when the user needs to construct a string incrementally.
- Keep encoding explicit when crossing operating-system boundaries.
- Remember that filesystem helpers usually return native-encoding paths.

## Pitfalls

- Do not blur owned versus non-owning string types.
- Do not ignore encoding when a path crosses platforms.
- Do not recommend `std::string` as the default answer unless the user explicitly asks for STL integration.
