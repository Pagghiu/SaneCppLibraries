# Experimental code

Experimental code maintained for evaluation. APIs, layout and availability may change or be removed without
compatibility guarantees. Inclusion in this repository does not make it part of the supported library set.

- [AsyncExecution](AsyncExecution/README.md): optional C++ sender/receiver adapter over SC::Async.
- [AsyncUV](AsyncUV/README.md): experimental source-compatible subset of the libuv C API over SC::Async.

Experiments are self-contained, with explicit build targets and their own examples/tests. They may depend on
core libraries; core libraries must never depend on experiments. Default SC.cpp, core dependency discovery,
supported-library documentation and normal single-file output exclude experiments. A diagnostic category may
remain reserved in the shared registry solely to prevent collisions; that reservation implies no support promise.

Keep experimental validation separate from core tests. The AsyncExecution targets require an explicitly supplied
stdexec checkout or a compatible native standard library; normal builds do not download a provider.
