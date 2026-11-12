# ASYNC-0023 - Resume short io_uring socket writes

Status: Accepted

## Context

A successful io_uring socket write can report a byte count smaller than the requested buffer. Treating
that completion as terminal drops the remaining bytes in streams that submit one send at a time.
Forcing a short write in the shared HTTP large-download test reproduced a truncated response with
libcurl's partial-file error.

## Decision

Keep the completed prefix on the request and submit the remaining bytes before notifying the callback.
For a short vectored write, submit the remaining spans individually so the kernel can read the
caller-owned buffers without modifying their descriptors. An interrupted write retries at the current
offset. A zero-byte completion with data left is a terminal incomplete send.

## Confirmation

Shorten one real io_uring submission in the existing large-download test. Confirm that the
unmodified backend loses the omitted bytes and that the updated backend delivers the entire body.
Run the existing Linux TCP send, cancellation, and full SCTest coverage in Debug and Release.
