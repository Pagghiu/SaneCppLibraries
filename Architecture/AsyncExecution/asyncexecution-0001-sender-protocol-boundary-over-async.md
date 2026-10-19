# ASYNCEXECUTION-0001 - Sender Protocol Boundary over Async

Status: Accepted
Date: 2026-10-19

## Context

AsyncExecution supplies execution control over the existing Async loop while preserving core library ownership and build boundaries. The approved protocol baseline is the official C++26 working draft; published ISO C++23 has no sender/receiver framework.

## Decision

Pin WG21 N5050 (2026-06-01), identified by editor report N5051 as the final C++26 draft and DIS basis. Use sender/operation tags, static get_completion_signatures<Self, Env...>(), member connect/start, receiver environments and exactly one value/error/stopped completion. Connected operations are immovable; terminal delivery may release their storage. Keep native submission, cancellation and quiescence on the owner loop.

Use the standard implementation by default when __cpp_lib_senders is available. An explicit opt-in selects external NVIDIA stdexec for interoperability tests. Keep provider headers/traits in the internal protocol boundary, never add execution types to namespace std, and retain independent feature gates for base senders and counting scopes. Provider revision, toolchain availability and test evidence belong in the validation log. Do not vendor or automatically download providers.

AsyncExecution may allocate for owning conveniences and graph storage, as authorized for this experimental library. Report its checked allocation failures through Result. Individual native operations embed requests; core Async retains its allocation policy and has no dependency on this adapter. Loops, pools, descriptors and buffers remain borrowed unless a factory explicitly transfers ownership. Source-specific acknowledgement policies govern stop; errors retain their original Result identity.

Reuse provider algorithms for scheduling and composition. SC supplies native I/O, fallible transforms, Result consumers and structured lifetime policy. The borrowing context remains primary; the owning runtime is optional. No detached graph ownership or replacement execution runtime is introduced.

## Confirmation

Keep dedicated concept/protocol, laziness, value/error/stopped, affinity, stop/quiescence and terminal-destruction tests. Audit pinned exec.async.ops/opstate, snd.concepts/getcomplsigs/connect, sched/schedule, get.env/get.stop.token and completion-scheduler/progress mappings. Verify strict disabled-provider builds and enabled single-file/unity boundaries. Report native standard-library evidence separately from external-provider interoperability; source/protocol comparison is not formal conformance proof.

## Related

- https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2026/n5050.pdf
- https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2026/n5051.html
- https://github.com/cplusplus/draft/blob/n5050/source/exec.tex
- [Tutorial](../../Documentation/Libraries/AsyncExecution.md)
