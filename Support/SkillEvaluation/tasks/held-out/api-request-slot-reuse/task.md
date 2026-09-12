# Bounded asynchronous request slots

Build `slots`, a program that submits `COUNT` numbered local timer work items, with at most `SLOTS` active at a time: `slots COUNT SLOTS`. `COUNT` is 1–64 and `SLOTS` is 1–4. It prints one JSON record per item in index order: `{ "item": N, "slot": N, "generation": N, "status": "ok" | "failed" }`. Use Sane `Async` callbacks and exactly `SLOTS` address-stable request-state slots; do not create one request object per logical item. Each item writes one result record after the work and its completion callback are terminal.

The program must report an error for invalid input or setup failure, continue after a per-item work failure, and permit a second batch after the first. A deterministic slot id and generation counter must let acceptance prove that logical items reused physical slots. Use no STL, exceptions, RTTI, per-item heap ownership, or blocking wait inside an event-loop callback. Supply a `build.sh` that writes `slots` to the directory named by `SC_SKILL_EVAL_BUILD_DIR`, tests, and `DESIGN.md` covering partial setup, cancellation, terminal completion, and slot reuse.

Do not read this benchmark's acceptance or review files while implementing.
