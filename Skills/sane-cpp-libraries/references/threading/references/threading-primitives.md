# Threading Primitives

Use this reference when a user needs a concise map of the Sane threading surface.

## Teach First

- `Atomic` for low-level atomic operations.
- `ThreadPool` for shared worker execution.
- Mutex, semaphore, barrier, rw-lock, and condition-variable usage for synchronization.

## Best Files To Inspect

- `Libraries/Threading/Threading.h`
- `Libraries/Threading/Atomic.h`
- `Libraries/Threading/ThreadPool.h`
- `Tests/Libraries/Threading/*`

## Good Advice To Give

- Match the primitive to the coordination pattern.
- Explain wakeups, ownership, and shutdown when threads are involved.
- Route I/O-driven coordination to `async` instead of broad threading advice.
