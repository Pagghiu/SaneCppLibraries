# Foundation Entry Points

## What To Teach First

- `SC::Result` for fallible operations.
- `SC::Span` and `SC::StringSpan` for non-owning views.
- `SC::Function` for callbacks with a fixed capture budget.
- `SC::Deferred` for scoped cleanup without exceptions.
- `SC::UniqueHandle` and `SC::OpaqueObject` for OS resource ownership and static PIMPL-style hiding.

## Best Files To Inspect

- `Libraries/Common/Result.h`
- `Libraries/Foundation/Span.h`
- `Libraries/Foundation/StringSpan.h`
- `Libraries/Foundation/Function.h`
- `Libraries/Foundation/Deferred.h`
- `Libraries/Foundation/UniqueHandle.h`
- `Libraries/Foundation/OpaqueObject.h`

## Best Examples

- `Tests/Libraries/Foundation/BaseTest.cpp`
- `Tests/Libraries/Foundation/FunctionTest.cpp`
- `Tests/Libraries/Foundation/StringSpanTest.cpp`
- `Tests/Libraries/Foundation/UniqueHandleTest.cpp`

## Common Advice

- Check every `Result` and surface the error early.
- Use views when ownership is already external.
- Keep callback captures small.
- Use the handle wrappers instead of ad hoc cleanup code.

## Handoff

- Route allocation and owned-string questions to `memory`.
- Route style and repo-wide rule questions to `core-patterns`.
