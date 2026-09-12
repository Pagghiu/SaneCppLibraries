# Bounded asynchronous timer slots

Build `slots`, a Sane `Async` program with this interface:

`slots COUNT SLOTS WORK_MS DEADLINE_MS [--fail-index INDEX]`

`COUNT` is 1–64, `SLOTS` is 1–4, and both times are 1–2000 milliseconds. Start at most `SLOTS` numbered timer work items at once using exactly `SLOTS` address-stable work-request slots; a separate batch-deadline request is allowed. Each item normally completes after `WORK_MS`; `--fail-index` makes that item report `failed` on completion without stopping later items. At `DEADLINE_MS` after batch start, stop launching queued items, mark active items `timed_out`, and mark queued items `cancelled`. A quiet timer that would complete after the deadline must not delay cancellation.

Print exactly one newline-delimited JSON record for every item, in index order: `{ "item": N, "slot": N | null, "generation": N | null, "status": "ok" | "failed" | "timed_out" | "cancelled" }`. Slot IDs are 0 through `SLOTS-1`; unlaunched items have null slot and generation. Each new use of one slot increments its generation. Exit 0 only if all items are `ok`, 1 if any item is `failed`, `timed_out`, or `cancelled`, and 2 on invalid input or setup failure with a diagnostic on stderr.

Use Sane `Async` callbacks without STL, exceptions, RTTI, per-item request objects or heap ownership, worker-thread waits, or busy polling. Supply a `build.sh` that writes `slots` to `SC_SKILL_EVAL_BUILD_DIR`, tests, and `DESIGN.md` covering request lifetime, partial setup, cancellation, terminal completion, and slot reuse.

Do not read this benchmark's acceptance or review files while implementing.
