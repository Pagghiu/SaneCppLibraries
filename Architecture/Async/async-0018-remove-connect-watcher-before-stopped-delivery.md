# ASYNC-0018 - Remove Connect Watcher Before Stopped Delivery

Status: Accepted
Date: 2026-10-19

## Context

POSIX nonblocking connect installs an output watcher when connect returns EINPROGRESS. Normal completion removes it, but the generic cancellation path previously left it registered. After stopped delivery marks the request free, another active request can keep polling and receive the stale socket event. A macOS frame-driven cancellation exposed the non-active-request assertion; a direct callback regression reproduces it without sender composition.

## Decision

POSIX connect cancellation explicitly removes an installed output watcher before cancellation teardown can finish and deliver afterStopped. Clear the watcher flag only after successful removal. Keep the borrowed socket open and preserve its native connection state; cancellation never transfers descriptor ownership. Successful/failed normal completion behavior is unchanged. Windows and io_uring retain their native completion-draining paths.

## Confirmation

Collect a pending connect event, request stop before dispatch, join stopped delivery, then poll again with unrelated counted work while keeping the socket open. No normal callback or event for the free request may occur. Run this regression and the native Async suite in Debug/Release on accessible platforms, then exercise frame-driven shutdown at coarse cadence. The historical normal-mode timeout remains a separate unclassified observation.
