# Async process and pipe composition

Use this recipe when an application launches processes and handles their output through `Async`. It describes API obligations, not a drop-in runner.

## Establish storage and ownership first

Keep the `Process`, `PipeDescriptor`s, `AsyncProcessExit`, `AsyncFileRead`s, read buffers, retained output, and callback state in stable caller-owned storage. A scheduler may have a fixed array of slots; a slot is unavailable until all operations that reference it have completed or been stopped.

Create every pipe deliberately. `PipeDescriptor::createPipe(PipeOptions)` has `blocking`, `readInheritable`, and `writeInheritable` options. The option describes the pipe endpoints, so do not assume it can make only the parent side nonblocking. Use the `Process` redirection overloads shown by `Process.h` and `AsyncTestProcess.inl`, and close/detach unneeded parent ends after launch so EOF remains meaningful.

Choose the read strategy from the contract, not convenience. `AsyncFileRead::executeOn` with an `AsyncTaskSequence` and
`ThreadPool` supports blocking descriptors by running their reads on workers; those reads are still blocking. If the
contract forbids blocking pipe reads, use a native nonblocking parent-read path and ensure the descriptor inherited as
child stdout/stderr is restored to ordinary blocking behavior before launch. This may require a platform-specific
application boundary because `PipeOptions::blocking` configures both endpoints.

## Register operations before relying on completion

1. Create and configure an `AsyncEventLoop`.
2. Associate externally created descriptors when the selected nonblocking path requires it, or use the documented task/thread-pool route for blocking descriptors.
3. Configure each `AsyncFileRead` with a nonempty caller buffer and callback; start it with its pipe descriptor.
4. Launch `Process` with the caller-owned redirection descriptors, then start `AsyncProcessExit` with its process handle.
5. On each read completion, first check the `Result`. Account for `numBytes`, retain only available capture capacity, and reactivate only when not at EOF. Do not turn an error into normal EOF.
6. If the workflow has a deadline, start an `AsyncLoopTimeout` before any blocking `run`/`runOnce` call. A stored timestamp
   checked between iterations cannot wake a loop whose children and pipes are quiet.

The exact order can vary with the failure-cleanup design, but every successful setup step needs a cleanup owner before the next fallible step. If partial setup fails, stop/retire active requests as their API requires, close descriptors, and reap a launched child before returning a setup error.

A failed launch can finish a logical job immediately, without posting an async completion. If that transition frees a slot
while work remains queued, keep scheduling until capacity is filled or the queue is empty before calling a blocking
`runOnce()`. Otherwise a quiet loop can strand the queue until its batch deadline fires. Keep this progress rule separate
from the deadline, which bounds waiting but does not launch work.

## Define completion, reuse, and cancellation

For independent stdout and stderr, the normal completion predicate is:

`process-exit observed AND stdout terminal AND stderr terminal`

Terminal means EOF after successful reads, or an explicitly recorded I/O failure. A capture limit does not make a stream terminal: continue draining when the child can otherwise block. Slot reuse is safe only after that predicate and the event loop no longer references its request objects.

Slot reuse does not imply that every object inside the slot is reusable as-is. Inspect reset semantics for `Process`
argument/environment storage and other accumulating state. Reconstruct or explicitly reset such an object only after its
previous handle and async-exit request are terminal.

On cancellation, stop launching queued work, signal only the intended child scope, keep enough I/O and exit processing alive to drain and reap, and publish one unambiguous result per logical job. Test normal completion, launch failure, a read failure if injectable, capture exhaustion, early EOF, cancellation, and partial setup cleanup separately.

An `AsyncProcessExit` callback establishes an exit result, but the application must still verify whether the selected
platform backend or another `Process` call performs the required OS reap. Do not equate notification with reclamation;
include the normal (not only failure/cancellation) path in source review or a zombie/reaping diagnostic.

## Verify before committing

Read the relevant source anchors, build before running tests, and use a workload larger than a pipe buffer so a short smoke test cannot hide backpressure. Use a quiet child whose lifetime exceeds the deadline so continuous output cannot accidentally wake the loop. Put a missing or nonexecutable job before successful queued jobs to verify that synchronous launch failure does not stall them. If testing request reuse, use fewer slots than logical jobs, vary executable paths/outcomes across generations, and record an observable slot-generation or reuse assertion.
