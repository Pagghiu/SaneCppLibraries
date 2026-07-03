# COMMON-0004 - Keep Common Free From Foundation and Library Dependencies

Status: Accepted
Date: 2026-07-03

## Context

Common exists so libraries can consume small primitives without depending on Foundation or another Sane library. If Common includes Foundation or grows dependencies on other libraries, it recreates the same dependency pressure it was introduced to remove.

## Decision

Common fragments may depend on other Common fragments when necessary, but they must not depend on Foundation or another Sane C++ library. Existing includes that point back into Foundation should be migrated away as the split continues.

## Consequences

Common fragments need to remain small and layered carefully. Some convenience functions should stay in a concrete library instead of moving to Common if they require that library's types or policy. This preserves Common as a dependency-breaking source-sharing area rather than a hidden foundation library.

## Confirmation

A change preserves this decision when `Libraries/Common` files do not include `Libraries/Foundation` or other library headers, dependency metadata does not mention Common, and consumers can include Common fragments without pulling an unintended library edge.

## Related

- [SC-0010 - Treat Common as source sharing, not a library](../Global/sc-0010-treat-common-as-source-sharing-not-a-library.md)
- [COMMON-0001 - Split foundational primitives into Common fragments](common-0001-split-foundational-primitives-into-common-fragments.md)
- [SC-0003 - Keep libraries independently consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
