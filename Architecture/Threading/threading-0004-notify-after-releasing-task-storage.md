# THREADING-0004 - Notify After Releasing Task Storage

Status: Accepted
Date: 2026-10-17

## Context

A sender scheduler must allow its receiver to destroy the connected operation during notification. ThreadPool normally accesses a Task after invoking its function, so putting terminal notification directly in that function cannot safely release task storage. The AsyncExecution prototype instead posted cleanup to its event loop and waited for the worker; long CPU continuations then blocked the owner loop.

## Decision

Add an optional caller-provided Task::afterRelease function. After the task body returns, the worker copies the notification while task storage is still valid, clears the pool's task links/ownership under its mutex, and invokes only the local notification copy. Neither the worker nor pool accesses the Task after release. The notification may destroy or requeue the task, provided callers synchronize other users/waiters of that storage.

waitForTask waits for task-body completion and node release. waitForAllTasks and destroy also wait for release notifications: the running-execution count remains held until notification returns. Notifications execute on the worker and must not wait for or destroy their own pool. Producers must stop before pool destruction, as before. No allocation or hidden task storage is introduced into Threading.

## Consequences

Task storage grows by one fixed-capacity callback, without per-task allocation. Per-task waits cover the body and node release; callers needing notification completion must join the whole pool. Notification code must synchronize task reuse and keep its pool alive through return.

## Confirmation

ThreadPool tests destroy/reconstruct/requeue task storage from its release notification and check worker affinity and full-pool joining. AsyncExecution must exercise long CPU work concurrent with owner-loop timers and sockets, queued cancellation and terminal operation destruction. Run Debug/Release suites and preserve single-file compilation.
