# ASYNCEXECUTION-0002 - Schedulers, Cancellation and Owning Facade

Status: Accepted; pool-ticket reclamation order superseded by ASYNCEXECUTION-0004
Date: 2026-10-17

## Context

The initial adapter established sender interoperability, but caller-managed requests, buffers and descriptors obscured execution control. The user explicitly authorized allocations in AsyncExecution and requested idiomatic composition with when_all, CPU work and dependent TCP operations.

## Decision

Extend ASYNCEXECUTION-0001 with an owning AsyncExecution facade. It owns one Async event loop, a command dispatcher and a thread pool. Its readFile/writeFile convenience senders own paths and buffers and perform blocking file work on that pool. Native TCP requests own their socket lifetime. The lower-level borrowing context remains available.

Expose event-loop and pool schedulers. continues_on(getPoolScheduler()) must deliver successful completion on a worker, so subsequent then executes there. A pool ticket is separately allocated: receivers may destroy operation storage during notification, while ThreadPool still accesses its task after its callback returns. The loop joins and releases the ticket before observing later queued terminal completion. Pool execution agents provide weakly_parallel progress; callbacks must not block waiting for this runtime's owner thread.

All native Async request submission, cancellation and completion occurs on the loop owner. Cross-thread start posts an intrusive command. Stop callbacks post owner-thread cancellation; unregistering synchronizes foreign callbacks before terminal notification. Active timer/socket cancellation waits for Async quiescence. Running worker functions cannot be preempted. Pool-backed requests retain their normal completion path, which joins the task before reporting stopped; this avoids marking a request Cancelling while its worker completion is already queued. Native timers and sockets still use Async request cancellation. A pool scheduler observes stop before invoking its receiver. Runtime destruction requires completion of all submitted work.

run connects, starts and drives a composed sender while retaining operation storage until its terminal completion. It also accepts a stop token. Ordinary failures preserve Result identity. Infrastructure failures that would make safe operation cleanup impossible use the existing Async fail-fast policy.

Use then for synchronous CPU transforms and let_value for a dependent asynchronous sender. A fallible buffer transform can return AsyncExecutionBufferResult; then treats it as a value. The sendTCP overload explicitly checks its status before connecting and converts failure to set_error(Result). This checked-value convention is an SC extension, not automatic standard then behavior.

## Consequences

The prototype now permits intentional allocation in AsyncExecution. Core Async allocation and ownership policies are unchanged. Buffer allocation failure is reported through Result. The demo checks canonical Base64 vectors, worker affinity, joined composition and TCP payload decoding. Native standard-library interoperability remains unverified; the externally supplied reference provider remains the feasibility test path.
