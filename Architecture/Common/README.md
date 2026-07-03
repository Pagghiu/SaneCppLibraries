# Common Architecture Decisions

`Libraries/Common` ADRs use the `COMMON-NNNN` identifier scope. These decisions apply to the shared source-fragment area that supports multiple libraries without becoming a Sane C++ library dependency.

## Decision Log

- [COMMON-0001 - Split foundational primitives into Common fragments](common-0001-split-foundational-primitives-into-common-fragments.md)
- [COMMON-0002 - Use guarded headers for shared public definitions](common-0002-use-guarded-headers-for-shared-public-definitions.md)
- [COMMON-0003 - Use unguarded inl files as per-consumer implementation source](common-0003-use-unguarded-inl-files-as-per-consumer-implementation-source.md)
- [COMMON-0004 - Keep Common free from Foundation and library dependencies](common-0004-keep-common-free-from-foundation-and-library-dependencies.md)
- [COMMON-0005 - Let each consuming library own its assert provider](common-0005-let-each-consuming-library-own-its-assert-provider.md)
- [COMMON-0006 - Treat Common public layouts as cross-library API surface](common-0006-treat-common-public-layouts-as-cross-library-api-surface.md)
- [COMMON-0007 - Keep IGrowableBuffer as the minimal output-growth adapter](common-0007-keep-igrowablebuffer-as-the-minimal-output-growth-adapter.md)
- [COMMON-0008 - Keep StringSpan and StringPath in Common](common-0008-keep-stringspan-and-stringpath-in-common.md)
