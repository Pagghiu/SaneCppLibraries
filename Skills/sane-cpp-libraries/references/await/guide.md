# Sane Await

## Quick Use

Choose `await` when the task is specifically about the draft C++20 coroutine layer over `AsyncEventLoop`.

- Treat `Await` as Draft: inspect `Documentation/Libraries/Await.md`, `Libraries/Await/Await.h`, and the tests before
  making API claims.
- Use `AwaitEventLoop` as a wrapper around an existing `AsyncEventLoop&`; it does not own or close the async loop.
- Keep Sane style: coroutine functions return `AwaitTask`, completion returns plain `Result`, and extra outputs use caller-provided objects.
- Use `SC_CO_TRY(co_await ...)` inside coroutine bodies instead of `SC_TRY`.
- Keep callback-style `Async` compatibility: `Await` operations share the same underlying event loop.
- Prefer `AwaitArena` examples when discussing no-allocation coroutine frame storage, but call out that the library is still experimental.

## What To Watch

- `SCAwaitTest` is C++20 and standard-library enabled; it is intentionally separate from `SCTest`.
- `AwaitTask` is caller-owned, movable, non-copyable, and must not be destroyed while active.
- Child tasks currently need explicit `spawn()` before `co_await child`.
- Cancellation is cooperative and routed through the currently suspended awaiter.
- The no-stdlib coroutine story is not solved yet; do not present `Await` as ready for normal `-nostdinc++` use.

## References

- Public docs: `Documentation/Libraries/Await.md`
- Public header: `Libraries/Await/Await.h`
- Tests: `Tests/Libraries/Await/AwaitTest.cpp`
- Test target: `Tests/SCAwaitTest/SCAwaitTest.cpp`
