# TOOLS-0027 - Fall Back to BSD Package Downloaders

Status: Accepted

## Context

Package archives are fetched by a host command and validated against a pinned digest.
BSD base systems can provide a downloader before curl or wget has been installed.

## Decision

After curl and wget both fail to launch, use NetBSD ftp in auto-fetch mode or FreeBSD
fetch. Keep the existing exit-status and digest checks. A successfully launched
downloader reporting a transfer error still fails the operation.

## Confirmation

With only base downloader commands available on PATH, fetch a local fixture, reuse a
matching cached copy, and reject a new download with a mismatched digest.
