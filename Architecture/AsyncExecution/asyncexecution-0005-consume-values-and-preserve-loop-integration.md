# ASYNCEXECUTION-0005 - Consume Values and Preserve Loop Integration

Status: Accepted
Date: 2026-10-17

## Context

The owning runtime's initial run consumer discarded values and only exposed a stop token. Standard on needs a start scheduler to restore the original execution resource. Applications also need the same graphs to run against an externally driven AsyncEventLoop, comparable to Await and AsyncFibers, without adopting another poller or exception policy.

## Decision

Move the blocking consumer state and non-template driving code into AsyncExecutionContext. The owning runtime forwards to that context and exposes getContext; the context exposes getEventLoop. Existing direct loop access remains compatible. Keep ordinary run as a discard-values consumer. Add runInto(sender, outputs...) and runIntoWithStop(sender, token, outputs...) with caller-owned output references. Require nothrow assignment for every possible successful completion. A small recursive reference pack avoids result tuple allocation and supports move-only/multiple values. Errors/stopped never assign outputs.

The consumer's root environment supplies get_scheduler and get_start_scheduler for the owner loop, plus its stop token. Forward progress remains conservatively weakly_parallel. Sender environments only advertise value-completion affinity, since submission errors and stopped completions need not run on that resource. No custom execution domain or delegation scheduler is needed.

Preserve set_error(Result) identity; map stopped to AsyncExecutionError::Stopped. Foreign error types need an explicit upstream mapping. Publish the same consumer on borrowed contexts. Reject consumption from a foreign thread or reentrantly during an existing run or SC dispatcher/delivery. Return only after terminal completion, resuming the loop after unrelated interrupts. Closing a context during consumption is rejected. A broken event-loop driver cannot safely unwind live arbitrary operation states and retains the existing trusted-driver assertion policy.

External drivers own connected operation storage and continue using run/runOnce/runNoWait or submitRequests/blockingPoll/dispatchCompletions. Document explicit kernel-event storage and owner-thread dispatch. External sync_wait is supported only with the SC loop independently driven (or a graph needing no SC loop progress). It retains its own error/exception policy; successful interoperability does not establish Result-based failure handling with exceptions disabled.

## Consequences

Callers retain values without an allocated tuple, at the cost of providing compatible nothrow-assignable outputs. The borrowed context gains synchronous consumption state while external drivers retain control of polling. Standard consumers continue to have their own progress and exception policies.

## Confirmation

Check standard schedule/starts_on/continues_on/both on forms, thread affinity, SC error identity, stopped recovery, retained scalar/multiple/move-only values and stop-token consumption. Exercise one graph with the owning runtime, borrowed blocking consumer, direct loop modes and phased polling. Verify external sync_wait on a separate thread while the SC owner drives its loop. Use bounded watchdogs for externally driven tests. Run the milestone validation gates before marking M2 complete.
