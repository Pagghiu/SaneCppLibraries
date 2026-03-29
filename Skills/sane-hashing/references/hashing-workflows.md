# Hashing Workflows

## What To Teach First

- `SC::Hashing::TypeMD5`
- `SC::Hashing::TypeSHA1`
- `SC::Hashing::TypeSHA256`
- The byte-stream nature of the API.

## Best Files To Inspect

- `Libraries/Hashing/Hashing.h`
- `Libraries/Hashing/HashingCBindings.h`
- `Libraries/Hashing/HashingCBindings.cpp`

## Best Examples

- `Tests/Libraries/Hashing/HashingTest.cpp`
- `Tests/Libraries/Hashing/HashingCBindingsTest.c`

## Common Advice

- Prefer SHA256 when the question is about modern integrity checks.
- Mention MD5 and SHA1 as compatibility options, not security recommendations.
- Explain the input encoding if the caller starts from text rather than bytes.
- Route package/build verification questions to the build or tooling skills when the context shifts.

## Handoff

- Route build/package-specific hash usage to `sane-build` or `sane-tools` when the ask is about workflow rather than hashing itself.
