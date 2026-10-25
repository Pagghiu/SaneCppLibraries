# ASYNCEXECUTION-0011 - Embed Pool Scheduling Task

Status: Accepted
Date: 2026-10-18

## Context

The pool operation is immovable, and ThreadPool afterRelease copies the notification before releasing its node. Scheduling can therefore retain operation-local task storage while permitting receiver-driven destruction.

## Decision

Embed ThreadPoolTask in the connected pool operation. Its body is empty; its afterRelease callback captures the context, operation pointer and delivery function. The copied callback drops context outstanding accounting before notifying the receiver on the worker. Neither the helper nor start accesses operation storage after successful queueing/delivery. On queue failure, drop accounting and deliver the original Result; terminal error may destroy the operation too.

The pool and context remain borrowed through callback return. waitForTask joins node release only. waitForAllTasks and destroy join active notifications; destroy discards queued tasks without notification and is not cancellation for live sender graphs. Join graph work and producer threads before destroying borrowed execution resources. Running CPU continuations remain cooperative.

Pool scheduling uses no separate heap ticket. Preserve owner-loop progress and affinity from ASYNCEXECUTION-0002; owning conveniences retain their explicit allocation policy.

## Confirmation

Exercise queued stop, completion/stop races, terminal value/stopped/error self-destruction, failed submission accounting, timer/TCP loop progress during gated CPU work, and node-release versus notification-tail joining. Validate the exact candidate across available platforms/configurations and record evidence in the operational log.
