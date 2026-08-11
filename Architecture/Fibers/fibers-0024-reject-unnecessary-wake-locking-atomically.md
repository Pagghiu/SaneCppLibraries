# FIBERS-0024 - Reject Unnecessary Wake Locking Atomically

Status: Accepted
Date: 2026-07-22

## Context

FIBERS-0022 coalesces operating-system condition-variable signals, but every logical ready notification still acquires
the wake-event mutex. A sustained stackful micro-task run produces more than one logical wake notification per job even
though workers are usually active. A symbolized optimized Time Profiler capture attributed most sampled CPU to that
mutex and to the steal/idle paths enclosing ready publication; completion finalization remained below 2%.

Skipping the notification entirely when no worker appears parked is unsafe. A worker can capture the current
generation, perform its final ready-work check, and begin parking while publication occurs. The generation must change
for every publication even if no condition-variable signal is needed.

## Decision

Wake generation and parked-worker count are atomic fixed-width values. Every normal ready publication atomically
advances generation first. If the atomic parked count is zero, notification returns without entering the wake-event
mutex. Otherwise it takes the mutex and applies the pending-signal coalescing contract from FIBERS-0022.

A worker holds the wake mutex, publishes its park intent by atomically incrementing the parked count, and then checks
generation again before calling the platform condition wait. This closes both sides of the race:

- publication before the second check changes generation, so the worker retracts park intent without sleeping;
- publication after the second check observes park intent and waits for the mutex until the condition wait atomically
  releases it, after which the notifier records and sends the signal.

Generation and parked count reset only during validated worker-pool reuse while no worker is parked. Pending signal
count and actual signal diagnostics remain protected by the existing mutex. No scheduler-global lock, allocation, or
new dependency is introduced.

## Consequences

Logical `wakeNotifications` still counts every generation change, including broadcasts, while `wakeSignals` counts
individual operating-system signals. A notification that finds no published park intent changes generation but does
not enter the OS mutex.

The fast path adds one atomic generation increment and one atomic parked-count load. Parking adds an atomic publish,
generation recheck, and atomic retract around the existing condition wait. The atomic counters retain the prior
32-bit wrap behavior; equality remains valid because pool reuse resets them while quiescent and a worker only compares
against its immediately observed generation.

FIBERS-0037 later narrows the generation requirement for stackless local batches appended behind existing backlog. It
uses a separate prepare-to-wait handshake with paired barriers while preserving this decision for all other paths.

Five macOS ARM64 Release samples of the sustained one-million-job workload increased median throughput from about
0.83 million to 1.46 million jobs per second. Median individual OS wake signals fell from roughly 110 thousand to 27
thousand. Five complete Debug and five complete Release Fibers stress runs passed before the full validation matrix.

## Alternatives Considered

- Read parked count without changing generation: rejected because publication can be lost while a worker transitions
  from its final ready check into the condition wait.
- Make generation atomic but leave parked count mutex-only: correct but unable to reject the mutex before acquiring it.
- Signal without the wake mutex: rejected because condition-variable signal accounting and the wait transition must
  remain serialized by the platform mutex.
- Replace condition variables with platform-specific semaphores or futexes: deferred because the atomic fast rejection
  removes the measured common-path bottleneck without adding a new backend abstraction.

## Confirmation

A change preserves this decision when every ready publication advances generation, a worker publishes park intent
before its final generation check, pending signals never exceed the parked count observed under the mutex, pool reuse
resets diagnostics only while quiescent, repeated publication-versus-park stress cannot hang, and Debug/Release tests
pass on the supported platform families.

FIBERS-0037 defines the only accepted exception to the first condition.

## Related

- [FIBERS-0010 - Use worker-owned scheduling with bounded injection](fibers-0010-use-worker-owned-scheduling-with-bounded-injection.md)
- [FIBERS-0014 - Use bounded worker idle spinning](fibers-0014-use-bounded-worker-idle-spinning.md)
- [FIBERS-0022 - Coalesce redundant worker wake signals](fibers-0022-coalesce-redundant-worker-wake-signals.md)
- [Fibers architecture](fibers-architecture.md)
- [Fibers documentation](../../Documentation/Libraries/Fibers.md)
