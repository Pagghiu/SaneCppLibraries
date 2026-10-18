# ASYNCEXECUTION-0012 - Embed Pool Scheduling Task

Status: Accepted
Date: 2026-10-18

## Context

ASYNCEXECUTION-0004 restored owner-loop progress using ThreadPool afterRelease, but retained a heap ticket that is no longer necessary. The pool operation is immovable, and ThreadPool copies the callback before releasing its node.

## Decision

Embed ThreadPoolTask in the connected pool operation. Its body is empty; its afterRelease callback captures the context, operation pointer and delivery function. The copied callback drops context outstanding accounting before notifying the receiver on the worker. Neither the helper nor start accesses operation storage after successful queueing/delivery. On queue failure, drop accounting and deliver the original Result; terminal error may destroy the operation too.

The pool and context remain borrowed through callback return. waitForTask joins node release only. waitForAllTasks and destroy join active notifications; destroy discards queued tasks without notification and is not cancellation for live sender graphs. Join graph work and producer threads before destroying borrowed execution resources. Running CPU continuations remain cooperative.

This supersedes the allocation/reclamation portion of ASYNCEXECUTION-0004, preserving its completion affinity and progress contract. It requires no Threading changes and does not change the allocation policy for owning conveniences.

## Confirmation

Exercise queued stop, completion/stop races, terminal value/stopped/error self-destruction, failed submission accounting, timer/TCP loop progress during gated CPU work, and node-release versus notification-tail joining. Validate the exact candidate across available platforms/configurations and record evidence in the operational log.
