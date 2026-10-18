# ASYNCEXECUTION-0001 - Prototype Standard Senders over Caller-Owned Async

Status: Accepted; scheduler, cancellation and ownership scope extended by ASYNCEXECUTION-0002
Date: 2026-10-17

## Context

We need a small experiment establishing whether standard execution senders can wrap Async without allocating operation storage or adding a second event loop. ISO/IEC 14882:2024 has no sender/receiver execution framework. The user approved targeting the latest official WG21 draft instead.

## Decision

Target WG21 N5050 (2026-06-01), identified by editor report N5051 as the final C++26 draft and basis of the DIS, replacing N5046. N5051 reports editorial changes with no intervening WG21 motions; this baseline correction does not itself establish a need to rewrite the execution protocol. Use its static `get_completion_signatures<Self, Env...>()`, member `connect`, `start`, and receiver completion protocols. This is draft interoperability, not published ISO conformance.

AsyncExecution is an optional adapter library, analogous to Await: it explicitly depends on Async. Standard execution headers stay in an internal integration header and are enabled only with standard C++ headers available. No execution types are added to namespace std. The core Async library does not depend on AsyncExecution.

The standard implementation is the default protocol provider. An explicit opt-in selects NVIDIA stdexec for interoperability tests on platforms without standard senders. Do not vendor or automatically download stdexec. Keep the reference implementation outside the repository and record its tested commit in the library documentation.

Connected operation states embed their request, receiver, and file-task storage and cannot move. Caller-owned loops, pools, buffers and descriptors outlive completion. Sender creation and connect submit no work. Start and completion dispatch use the loop's owner thread; work bodies execute on the explicitly supplied thread pool. The caller drives and closes the loop.

Use set_value for successful operation-specific results, set_error(SC::Result) for submission/completion failures, and set_stopped when stop is observed before submission or at successful completion. Stop is cooperative: active requests are not interrupted. Errors take precedence over stop at completion. Completion notification is the final access to operation storage.

## Consequences

The slice supports timers, socket send/receive, file read/write at explicit offsets, and thread-pool work. It does not offer a general scheduler, cross-thread start, prompt cancellation, detached lifetime management, or a replacement implementation of standard execution algorithms. Socket/file transfers may be partial. Work functions must not throw. Allocation freedom applies to SC's sender/operation machinery, not caller pool creation or arbitrary external sender algorithms.

## Confirmation

The dedicated SCAsyncExecutionTest target checks sender and operation-state concepts, standard-algorithm composition, lazy submission, success/error/stopped completion, TCP transfer, and buffered file I/O. Existing strict targets retain their no-standard-header configuration. Native std::execution validation must be reported separately from reference-provider validation.

## Related

- https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2026/n5050.pdf
- https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2026/n5051.html
- https://www.iso.org/standard/83626.html
- ../../Documentation/Libraries/AsyncExecution.md

## N5050 protocol audit (2026-10-18)

Compared the official draft repository tags `n5046` and `n5050`, file `source/exec.tex`: byte-identical, SHA-256 `4a60ba52a15d14987b722be3d3b867e87046cb5167956e85d13ba7462294e978`. N5051 reports no intervening WG21 motions. No execution-clause migration is needed solely because of this baseline update.

The implementation audit covered:

| Pinned clauses | Adapter mapping and finding |
| --- | --- |
| exec.async.ops, exec.opstate, exec.opstate.start | Lazy connection, immovable operation storage, noexcept start, exactly one value/error/stopped notification; no operation access after terminal delivery. Native cancellation joins request storage before notification. |
| exec.snd.concepts, exec.getcomplsigs, exec.connect | sender_tag, static consteval get_completion_signatures<Self, Env...>, member connect, Result errors and operation-specific success values. |
| exec.sched, exec.schedule | Copyable borrowing handles, resource-identity equality, worker/owner-loop value affinity. Audit found missing scheduler-side value completion-scheduler queries; both schedulers now return themselves, matching schedule sender attributes. |
| exec.get.env, exec.get.stop.token | Read-only attributes/receiver environments, receiver token callbacks; native submission/cancellation marshalled to owner loop. |
| exec.get.compl.sched, exec.get.fwd.progress | Value-only completion affinity queries and conservative weakly_parallel progress. No promised error/stopped completion affinity. |

These are source-level mappings and provider test targets, not proof of native standard-library conformance or an exhaustive formal audit. Pinned source: https://github.com/cplusplus/draft/blob/n5050/source/exec.tex (comparison: n5046 at the same path).

Native provider availability checked on the available host/VM toolchains: Apple clang 17/libc++ 200100, Homebrew clang 23.1.2/libc++ 230102, Ubuntu GCC 13.3 and clang 18.1.3/libstdc++ 20240904 do not define __cpp_lib_senders after including execution. The newest installed MSVC 14.44.35207 execution/yvals_core headers have neither that macro nor sender/scheduler tags. Native sender tests are unavailable on these installations; retain the default-provider feature gate and test with the explicitly selected external provider. Recheck availability when toolchains change.
