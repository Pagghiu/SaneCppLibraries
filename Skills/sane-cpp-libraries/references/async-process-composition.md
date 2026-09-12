# Async process and pipe composition

Use this recipe when an application launches processes and handles their output through `Async`. It describes API obligations, not a drop-in runner.

## Establish storage and ownership first

Keep the `Process`, `PipeDescriptor`s, `AsyncProcessExit`, `AsyncFileRead`s, read buffers, retained output, and callback state in stable caller-owned storage. A scheduler may have a fixed array of slots; a slot is unavailable until all operations that reference it have completed or been stopped.

Create every pipe deliberately. `PipeDescriptor::createPipe(PipeOptions)` has `blocking`, `readInheritable`, and `writeInheritable` options. The option describes the pipe endpoints, so do not assume it can make only the parent side nonblocking. Use the `Process` redirection overloads shown by `Process.h` and `AsyncTestProcess.inl`, and close/detach unneeded parent ends after launch so EOF remains meaningful.

## Register operations before relying on completion

1. Create and configure an `AsyncEventLoop`.
2. Associate externally created descriptors when the selected nonblocking path requires it, or use the documented task/thread-pool route for blocking descriptors.
3. Configure each `AsyncFileRead` with a nonempty caller buffer and callback; start it with its pipe descriptor.
4. Launch `Process` with the caller-owned redirection descriptors, then start `AsyncProcessExit` with its process handle.
5. On each read completion, first check the `Result`. Account for `numBytes`, retain only available capture capacity, and reactivate only when not at EOF. Do not turn an error into normal EOF.

The exact order can vary with the failure-cleanup design, but every successful setup step needs a cleanup owner before the next fallible step. If partial setup fails, stop/retire active requests as their API requires, close descriptors, and reap a launched child before returning a setup error.

## Define completion, reuse, and cancellation

For independent stdout and stderr, the normal completion predicate is:

`process-exit observed AND stdout terminal AND stderr terminal`

Terminal means EOF after successful reads, or an explicitly recorded I/O failure. A capture limit does not make a stream terminal: continue draining when the child can otherwise block. Slot reuse is safe only after that predicate and the event loop no longer references its request objects.

On cancellation, stop launching queued work, signal only the intended child scope, keep enough I/O and exit processing alive to drain and reap, and publish one unambiguous result per logical job. Test normal completion, launch failure, a read failure if injectable, capture exhaustion, early EOF, cancellation, and partial setup cleanup separately.

## Verify before committing

Read the relevant source anchors, build before running tests, and use a workload larger than a pipe buffer so a short smoke test cannot hide backpressure. If testing request reuse, use fewer slots than logical jobs and record an observable slot-generation or reuse assertion.
