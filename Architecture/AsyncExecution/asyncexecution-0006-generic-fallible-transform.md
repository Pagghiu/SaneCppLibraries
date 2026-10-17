# ASYNCEXECUTION-0006 - Generic Fallible Transform

Status: Accepted
Date: 2026-10-17

## Context

The initial Base64 example returned a checked buffer through standard then, and sendTCP alone unwrapped its Result. That couples a CPU transform's failure handling to its eventual transport and makes file/computation consumers handle the same result differently. Standard then must retain ordinary value-transformation semantics.

## Decision

Add SC::tryThen as an SC extension, with pipe-closure and direct sender/callable forms. Procedures take upstream values as rvalues, are noexcept, and return either Result (zero success values) or AsyncExecutionValueResult<T> (one success value). The simple checked carrier owns a status and T; error payload storage is still destroyed. Require an object type with nothrow move construction for T; reject reference payloads. Multi-value outputs can be represented by one application aggregate; ordinary provider algorithms still support multiple incoming values.

Compose provider let_value with a small synchronous checked sender. let_value retains incoming values through the nested operation; the checked sender owns the callable and borrows those stable inputs. It checks stop before invoking CPU work, then emits the original Result on failure, stopped after successful work if requested, or the moved output value. A running callable cannot be preempted. No storage access follows terminal delivery; move-only callables and outputs are supported. This introduces no adapter-specific allocation or exception path.

CPU placement remains standard on/continues_on composition. The demo uses on(pool, tryThen(codec)) so work returns to the loop before dependent I/O. Keep AsyncExecutionBufferResult and sendTCP compatibility overloads temporarily, while removing their use from the main example path. The generic result carrier works for TCP, file writing and further computation. Include required standard traits/utility headers explicitly inside the internal protocol boundary.

## Consequences

Fallible transforms can feed any downstream consumer without transport-specific status handling. The extension requires an explicit checked carrier and noexcept callable instead of changing standard then. The generic composition remains templated in the header and introduces no additional allocation.

## Confirmation

Exercise value and Result-only success, exact error identity including a supplied allocation-failure Result, skipped downstream work, move-only outputs/callables, stop before work and stop during work, and error/stop precedence. Verify lifetime safety through terminal destruction. Run encoded TCP demos and encoded file/computation tests, then all milestone validation gates. Injected allocation-error propagation is distinct from allocator fault injection.
