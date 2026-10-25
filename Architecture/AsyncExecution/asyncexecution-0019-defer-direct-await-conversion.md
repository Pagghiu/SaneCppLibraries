# ASYNCEXECUTION-0019 - Defer Direct Await Conversion

Status: Accepted
Date: 2026-10-19

## Context

The adapters can share an AsyncEventLoop, but direct conversion also couples task ownership, allocator policy and cancellation. AwaitTask uses an Await-specific typed continuation, a caller-provided AwaitAllocator, private start/resume/frame ownership and internal cancellation/deferred-destruction helpers. Its public await_suspend accepts AwaitTask::Handle. It has no public completion-observer API for attaching a sender receiver to an existing task.

AsyncExecution connects immovable provider operation states, supports value packs and move-only values, and has distinct error/stopped channels. Its optional provider/build gate differs from Await's C++20 and no-standard-library coroutine configuration. The current prototype has operation parity use cases; it has no concrete mixed-task ownership use case requiring a conversion layer.

## Decision

Defer direct sender-to-Await and Await-to-sender conversion. Keep both libraries independent. They can be configured to borrow the same underlying loop, with explicit owner driving, Await deferred-frame draining and joining of both adapters before close. The execution dispatcher keeps counted native work alive, so task completion alone is insufficient to end an unrelated native run loop. Do not add implicit operator co_await, make Await depend on execution providers, poll task completion from a worker, or overwrite private task/group completion hooks.

A future optional bridge must include both adapters explicitly and define these contracts before implementation:

- Storage: an explicit caller-selected allocator owns stable, correctly aligned connected storage. It survives terminal acknowledgement and is reclaimed before frame/graph release. AsyncExecution's allocation permission does not change Await's allocator contract.
- Affinity: require matching borrowed loops; submit, cancel and resume Await tasks on their owner. Marshal worker/inline sender completion safely, including completion during await_suspend. Never use a blocking consumer inside a coroutine/owner callback.
- Cancellation: translate a task cancellation request into a sender stop request and join terminal completion before resuming/destroying the frame. Define stopped-to-AwaitCancelledResult mapping while preserving sender Result errors; no reinterpretation of an error channel merely because its numeric result resembles cancellation.
- Ownership: state whether a task is transferred or borrowed, reject incompatible already-started/group-owned tasks, and expose a supported completion observer if needed. Await-to-sender cannot take ownership by reaching through private handles.
- Values/build: start with explicit Result plus caller output objects, including move-only transfer and failure preservation. Gate the bridge separately so standalone Await and disabled-provider AsyncExecution still compile unchanged.

Bridge implementation is an explicit follow-up after a concrete mixed-task use case and supported Await completion/cancellation hooks justify this additional surface. Native standard-library provider availability is a separate validation concern, not the reason for deferral.

## Confirmation

This decision adds no bridge API or inter-library dependency. Existing operation parity tests remain the evidence for shared semantics; they do not prove direct conversion. Before accepting a future implementation, require deterministic inline/worker completion handshakes, native error/stop races, over-aligned allocation failure, terminal storage destruction, owner affinity, group/frame lifetime, and enabled/disabled standalone builds.

## Source basis

- [AwaitTask, Promise and AwaitEventLoop](../../Libraries/Await/Await.h)
- [Await cancellation and deferred frame cleanup](../../Libraries/Await/Await.cpp)
- [Execution provider and scope gates](../../Extra/Experimental/AsyncExecution/Internal/AsyncExecutionProtocol.h)
