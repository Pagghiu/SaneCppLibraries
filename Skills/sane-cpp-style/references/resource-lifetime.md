# Resource, capacity, and lifetime design

Use this reference for a new service, component, library, async operation, queue, traversal, or change that adds persistent state.

## Make the policy observable

Draw or write a small ownership table before coding. Include storage owner, borrowed inputs, active-operation state, cleanup trigger, and reuse condition. Prefer a lifecycle that can be tested from the outside:

`empty -> configured -> active -> terminal -> reusable/destroyed`

For a fixed-concurrency design, define a slot that owns the request state, buffers, callback context, and handles for one logical operation. A free-count alone does not prove safe reuse. A slot becomes reusable only after every observer that can access its state is terminal or detached.

## Capacity is part of the contract

Choose the bound closest to the costly resource: number of active operations, bytes retained, queue entries, recursion depth, file descriptors, or time. State the unit and the response at exhaustion. Good policies include a `Result`/status failure, caller growth through an explicit allocator, bounded wait, cancellation, or intentional coalescing. A silent fallback to unlimited heap allocation changes the architecture.

When output must be limited but a producer can block, distinguish retained bytes from consumed bytes. Continue draining, discard according to a named policy, and retain the total/error state necessary to report a truthful result.

## Failure and cancellation

For each setup step, identify what has become live and how it is cleaned up if the next step fails. For asynchronous operations, cancellation changes intent; it does not necessarily make OS requests, callbacks, descriptors, or child processes disappear. Define how callbacks quiesce, handles close, and work is reaped/drained before state is released.

A deadline must be represented in the primitive that can otherwise block: a timer in the wait set, a bounded wait, or an
explicit wake-up. Polling the clock only before or after an unbounded wait cannot guarantee timely cancellation. Likewise,
a completion notification may precede required reclamation such as joining a thread or reaping a child; model both states.

Use RAII where the project allows it for local scope cleanup, but do not let an RAII wrapper hide the externally observable lifetime of asynchronous or shared work. A destructor is not a substitute for a documented cancellation/completion protocol.
