# ASYNCEXECUTION-0016 - Composition, Affinity and Read Results

Status: Accepted
Date: 2026-10-18

## Decision

Use standard scheduling and composition plus the SC tryThen error bridge as the primary idiom. then transforms values synchronously; tryThen turns Result/AsyncExecutionValueResult<T> into value/error/stopped channels; let_value connects dependent asynchronous work; when_all expresses fan-out and joins cancellation. Examples use on(pool, tryThen(codec)) to restore the caller's scheduler before dependent I/O.

Remove the experimental AsyncExecutionBufferResult carrier and the sendTCP checked-carrier/status-injection overloads. TCP accepts owned bytes and reports its own connection/send failures. Codecs select their error channel before TCP is constructed, through tryThen, rather than asking a transport to interpret a previous computation's Result. This is an intentional experimental API break; there is no compatibility shim. Migrate all repository consumers and document the replacement.

Retain onPool as an explicit SC convenience with a distinct affinity contract: on a valid running context, execute the supplied body on the runtime's pool and complete on its owner loop, including when started from another worker. schedule(pool) completes and runs continuations on a worker; standard on restores the scheduler supplied by the caller's environment, which need not be the owner's loop. onPool uses SC::AsyncLoopWork and is useful when that owner binding is desired, while primary examples remain provider composition. It is neither a pool scheduler nor a general replacement for on. Keep context.work as the borrowed native work primitive.

Retain AsyncExecutionReadResult for both TCP receive and file reads. Both report a byte span and an end-of-input observation with identical partial-transfer/progress mechanics. A socket's EOF means orderly peer closure; a file's EOF describes the current file range/end and does not imply a permanently closed stream. Distinct nominal types would duplicate storage/consumer handling without enforcing resource ownership or adding information. Describe resource meaning at each factory; do not rename peer adapters for superficial symmetry. Exact-read helpers retain their explicit incomplete-input errors.

## Confirmation

No live API or repository consumer uses the removed carrier or status-injection overloads. A failing codec preserves its exact Result and never constructs dependent TCP work. Tests distinguish worker scheduler continuation from owner-loop onPool completion, including a worker-started convenience. Existing TCP/file partial-read, EOF and exact-transfer tests remain green. Update examples/migration notes and separate-TU/unity/amalgamation checks. Cross-adapter error-versus-stop races and file-send/datagram compatibility remain their own roadmap checks.
