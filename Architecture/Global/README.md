# Global Architecture Decisions

Project-wide ADRs use the `SC-NNNN` identifier scope. These decisions apply to all libraries unless a library-specific ADR records an explicit exception or refinement.

## Decision Log

- [SC-0001 - Library code must not hide dynamic allocation](sc-0001-no-hidden-allocation.md)
- [SC-0002 - Use scoped architecture decision records](sc-0002-use-scoped-architecture-decision-records.md)
- [SC-0003 - Keep libraries independently consumable](sc-0003-keep-libraries-independently-consumable.md)
- [SC-0004 - Single-file libraries are first-class distribution artifacts](sc-0004-single-file-libraries-are-first-class-distribution-artifacts.md)
- [SC-0005 - Avoid STL, exceptions, and RTTI in library code](sc-0005-avoid-stl-exceptions-and-rtti-in-library-code.md)
- [SC-0006 - Use explicit Result-based error propagation](sc-0006-use-explicit-result-based-error-propagation.md)
- [SC-0007 - Keep public headers free of system and compiler headers](sc-0007-keep-public-headers-free-of-system-and-compiler-headers.md)
- [SC-0008 - Prefer native OS APIs over third-party dependencies](sc-0008-prefer-native-os-apis-over-third-party-dependencies.md)
- [SC-0009 - Isolate platform-specific implementations behind internal code](sc-0009-isolate-platform-specific-implementations-behind-internal-code.md)
- [SC-0010 - Treat Common as source sharing, not a library](sc-0010-treat-common-as-source-sharing-not-a-library.md)
- [SC-0011 - Make allocation-capable facilities explicit and optional](sc-0011-make-allocation-capable-facilities-explicit-and-optional.md)
- [SC-0012 - Support bring-your-own containers](sc-0012-support-bring-your-own-containers.md)
- [SC-0013 - Maintain agentic development as the primary contribution workflow](sc-0013-maintain-agentic-development-as-the-primary-contribution-workflow.md)
- [SC-0014 - Use automated checks to protect architecture](sc-0014-use-automated-checks-to-protect-architecture.md)
- [SC-0015 - Prefer maturing existing libraries over expanding scope](sc-0015-prefer-maturing-existing-libraries-over-expanding-scope.md)
- [SC-0016 - Support layered adoption modes](sc-0016-support-layered-adoption-modes.md)
- [SC-0017 - Publish Draft and MVP libraries deliberately](sc-0017-publish-draft-and-mvp-libraries-deliberately.md)
