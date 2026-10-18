# ASYNCEXECUTION-0013 - Observable Runtime Shutdown

Status: Accepted
Date: 2026-10-18

## Decision

Provide owner-thread Result-returning AsyncExecution::close(). Closing an empty/already closed runtime succeeds. An active context (queued/native work, an active consumer, owner delivery/dispatch or a worker notification) reports ActiveRequestsRemain before closing resources. A foreign-thread close reports InvalidState. Rejected close leaves the runtime usable; callers stop and join graphs, then close again. It does not cancel or block on an arbitrary active graph.

Track successful creation/cleanup stages for context, loop and pool. Close the dispatcher first, then loop, then pool; clear a stage only on success. Return the first native cleanup Result unchanged; later calls resume from the unfinished stage. Creation failure rolls back successful stages; a rollback failure is returned and the retained state can be closed explicitly. Creation is rejected while any cleanup stage remains.

The context stops accepting starts when actual shutdown begins. A failed dispatcher stop/drain retains a closing state; create/run cannot reopen it, and another close resumes drain without stopping an already cancelling request twice. Backend failure recovery remains subject to the underlying Async/ThreadPool contract; stage tracking cannot repair invalid native resources. A failed close requires resolving its cause before retrying, and the object must remain alive meanwhile.

Worker notifications publish a separate count before dropping outstanding task accounting. Close reads outstanding before notification counts, avoiding a false idle gap. Receiver delivery may destroy its operation; the copied callback saves its context pointer and never accesses the operation afterward. This count does not prohibit an owner-thread consumer from driving unrelated work while a worker continuation runs.

All graphs and producer threads must be quiescent before runtime destruction, on the owner thread. The destructor joins completed graphs' pool notification tails and invokes the same checked cleanup. It cannot safely stop/join arbitrary active graphs without a structured lifetime owner. Borrowed contexts/pools survive notification return. Native requests submitted directly to getEventLoop remain the caller's responsibility; close is not a general graph registry. Scopes will supply graph-level ownership.

## Confirmation

Test empty/repeated close, failed creation rollback, recreation, wrong-thread/reentrant/queued/native/pool active rejection, externally driven cancellation, and owner-loop progress while a pool continuation is gated. Validate existing terminal operation destruction and pool notification-tail tests. Native cleanup-error injection is not supplied by this API; report the extent of exercised failure paths separately from stage-policy source review.
