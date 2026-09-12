# Presentation-ready factual summary

| Topic | Observed fact | Limitation |
| --- | --- | --- |
| Evaluation 1: original async case study | One Luna High run built a bounded callback-based Sane runner; corrected evaluation passed 5/6 suites, ordinary and ASan/UBSan. | Not a skill comparison; one model/task/platform run. |
| Missed obligations | Large child output exposed nonblocking inherited pipe endpoints; source review found no real `N`-slot reuse and untested cleanup/error paths. | Some findings were review-only, not fault-injected. |
| New API skill | A source-anchored Sane API skill now emphasizes lifecycle composition, `Result` handling, pipe semantics, and observable slot reuse. | No controlled before/after subjects completed yet. |
| Transferable style | A separate Style skill teaches explicit ownership, capacity, errors, cleanup, headers, and dependencies without requiring Sane APIs. | Transfer evaluation is staged but not yet run. |

Suggested talk framing: advertise Sane C++ Libraries as the concrete API layer first; present explicit-resource principles as transferable architectural practice second; describe agent workflow as measured, bounded experimentation third. Do not claim an improvement percentage or token saving from the current data.
