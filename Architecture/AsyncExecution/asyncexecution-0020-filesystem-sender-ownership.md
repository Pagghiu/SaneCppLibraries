# ASYNCEXECUTION-0020 - Filesystem Sender Ownership

Status: Proposed
Date: 2026-10-19

## Context

The borrowing context already exposes native file transfers. Remaining filesystem operations use AsyncFileSystemOperation, whose internal AsyncLoopWork runs on a pool on macOS/Windows/epoll; Linux io_uring can execute open/close/read/write/rename/remove natively. Copy operations use the native implementation's pool fallback. An adapter must not mistake the outer request's Free state for quiescence while its internal worker is running.

## Decision

Add context fsOpen/Close/Read/Write, followed by fsCopyFile/CopyDirectory/Rename/RemoveEmptyDirectory/RemoveFile. Borrow a required pool and preserve NativePreferred versus explicit ForceThreadPool. Copy and resolve path arguments into native StringPath storage when constructing the sender, matching existing owning file conveniences. Paths remain stable independently of application temporaries; capacity/conversion failure is an asynchronous initialization Result.

fsOpen emits a move-only AsyncExecutionFile containing a FileDescriptor, with a nothrow transfer contract for provider composition. Adopt a successful native open handle before observing stop. On stopped delivery, close the unused handle before calling the receiver; a cleanup failure is an error instead of stopped. Preserve an earlier native error over cleanup error. A successful receiver owns the handle and may compose fsClose or use normal FileDescriptor RAII.

fsClose borrows a FileDescriptor exclusively until terminal completion. Failed validation/submission retains ownership; a completed close attempt invalidates the descriptor, including a close failure, matching UniqueHandle::close's rule against retrying an ambiguous native close. The currently supported native paths distinguish worker FileError::CloseFailed and io_uring AsyncError::EventCompletionFailed from submission failures. Test that distinction directly; do not infer it merely from request.isFree(), and do not detach at acceptance while queue failure can still occur. This error-stage mapping is an explicit dependency of this adapter and must be revisited if native close error identities change.

fsRead emits AsyncExecutionReadResult and fsWrite emits a byte count for one native operation with explicit offset. Retain native partial-transfer/EOF semantics, validate representable offsets and single-transfer sizes, and preserve native EmptyBuffer errors. These are separate from the existing exact/all transfer algorithms. Borrow buffers and descriptors through completion.

After native submission, filesystem stop is cooperative: retain the normal callback until the operation/worker completes, then observe the token. A queued or running filesystem operation can have irreversible effects; cancellation is not rollback. Do not synchronously join a worker from the owner dispatcher, cancel away the only open-handle result, or deliver stopped because the outer request is Free. Prestop prevents submission and leaves descriptors/filesystem unchanged. The generic operation needs an explicit join-on-stop policy for these setups, plus a discard-value cleanup hook for open.

Metadata/path operations emit no success values. Use native copy flags, backend fallback and existing Result identity. No new scheduler, inter-adapter dependency or core allocation policy is introduced. Factories, validation, path copying and native setup belong in AsyncExecution.cpp.

## Confirmation

Run lazy path-copy and open→write→read→EOF→close composition on supported Linux backends and explicit forced fallback. Check invalid input/prestop, original errors, native close failure versus pool queue failure, retained descriptor ownership on failed submission, and opened-handle cleanup before stopped delivery. Gate a one-worker pool, queue open/close/read/write, request stop while the worker is occupied and prove owner-loop progress plus no terminal callback before releasing/joining work. Free connected storage inside value/error/stopped delivery. Exercise copy-file/directory, rename and both removals, native/forced fallback and errors. Update parity/compatibility docs and run required platform/integration gates before accepting this proposal.
