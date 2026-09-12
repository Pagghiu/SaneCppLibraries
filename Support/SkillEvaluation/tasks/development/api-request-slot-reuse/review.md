# Predeclared source-review obligations

- There are no more active request-state objects than `SLOTS`; a counter alone is insufficient.
- Request storage, callbacks, and buffers remain valid until terminal completion.
- Each setup failure cleans up every already-live resource before returning.
- A deadline request or equivalent wake-up participates in the blocking wait; clock checks around an unbounded wait are insufficient.
- Cancellation retires active requests before a slot is reused or destroyed. Callback and loop errors are not silently converted to normal completion.
- Reuse waits for a documented terminal predicate and is externally observable.
