# Presentation-ready factual summary

| Topic | Observed fact | Limitation |
| --- | --- | --- |
| Evaluation 1: original async case study | One Luna High run built a bounded callback-based Sane runner; corrected evaluation passed 5/6 suites, ordinary and ASan/UBSan. | Not a skill comparison; one model/task/platform run. |
| Missed obligations | Large child output exposed nonblocking inherited pipe endpoints; source review found no real `N`-slot reuse and untested cleanup/error paths. | Some findings were review-only, not fault-injected. |
| Evaluation 2: old versus updated | Old API skill: 3/6 suites; updated API plus Style: 4/6. Neither completed the runner task. | One attempt per condition; updated condition combines two skills; old condition had a disclosed prohibited-source exposure. |
| Evaluation 3: updated-v2 follow-up | Updated-v2 API plus Style: 5/6 suites; quiet deadlines now pass, but a failed launch strands queued work. | Still no full-task success; GPT-6 Luna replaced GPT-5.6 Luna, so the score difference is not an isolated skill effect. |
| Evaluation 4: updated-v3 follow-up | Updated-v3 API plus Style: 6/6 predeclared functional suites; queue continuation now passes. | Supplemental probe found an unreaped zombie under the live runner, so the task still fails its reaping requirement. One in-sample attempt, not general skill proof. |
| New API skill revision | Source-backed guidance now covers timed loop wake-ups, nonblocking I/O strategy, state reset on reuse, and checking reaping on the selected backend. | This revision has not been tested on held-out subjects. |
| Transferable style | A separate Style skill teaches explicit ownership, capacity, errors, cleanup, headers, dependencies, and timed waits without requiring Sane APIs. | Transfer evaluation is staged but not yet run. |

Suggested talk framing: advertise Sane C++ Libraries as the concrete API layer first; present explicit-resource principles as transferable architectural practice second; describe agent workflow as bounded experimentation third. Do not claim an improvement percentage, full-task success, held-out success, or token saving from these trials.
