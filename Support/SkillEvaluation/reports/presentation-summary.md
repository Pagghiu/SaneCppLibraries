# Presentation-ready factual summary

| Topic | Observed fact | Limitation |
| --- | --- | --- |
| Evaluation 1: original async case study | One Luna High run built a bounded callback-based Sane runner; corrected evaluation passed 5/6 suites, ordinary and ASan/UBSan. | Not a skill comparison; one model/task/platform run. |
| Missed obligations | Large child output exposed nonblocking inherited pipe endpoints; source review found no real `N`-slot reuse and untested cleanup/error paths. | Some findings were review-only, not fault-injected. |
| Evaluation 2: old versus updated | Old API skill: 3/6 suites; updated API plus Style: 4/6. Neither completed the runner task. | One attempt per condition; updated condition combines two skills; old condition had a disclosed prohibited-source exposure. |
| New API skill revision | Source-backed guidance now covers timed loop wake-ups, nonblocking I/O strategy, state reset on reuse, and checking reaping on the selected backend. | This revision has not been tested on held-out subjects. |
| Transferable style | A separate Style skill teaches explicit ownership, capacity, errors, cleanup, headers, dependencies, and timed waits without requiring Sane APIs. | Transfer evaluation is staged but not yet run. |

Suggested talk framing: advertise Sane C++ Libraries as the concrete API layer first; present explicit-resource principles as transferable architectural practice second; describe agent workflow as bounded experimentation third. Do not claim an improvement percentage, full-task success, or token saving from these trials.
