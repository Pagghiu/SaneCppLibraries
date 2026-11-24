# SC-0018 - Port BSD Systems by Platform and Capability

Status: Accepted
Date: 2026-11-01

## Context

BSD systems share POSIX interfaces but differ in operating-system APIs and behavior. Linux and Apple code also contain assumptions that do not necessarily hold on a BSD system. Platform detection alone cannot establish that a library is supported.

## Decision

Identify each BSD host explicitly. Share implementations where the required API and its behavior have been verified for each target, and isolate differences behind internal platform or capability selection. Do not identify a BSD as Linux or Apple to reuse code. Bring up libraries and tools incrementally, with an explicit tested platform and scope; do not claim support for the BSD family from results on one system.

Keep macOS, Windows, and Linux as the primary support tier. Treat every BSD port as lower-tier experimental support,
tied to the tested operating-system release and capability matrix even when it runs the shared suite.

## Consequences

NetBSD, FreeBSD, and OpenBSD are qualified independently against their pinned releases. Reusing POSIX, inotify, or kqueue code can reduce duplication, but individual facilities, including filesystem watching, retain their own compatibility tests. A capability can remain unavailable on one BSD without weakening the explicit behavior of the others.

## Confirmation

Platform detection distinguishes the operating systems, platform-specific branches select only validated implementations, and support claims name the tested libraries, systems, and configurations. Tests cover behavioral assumptions on each system for which code is enabled.
