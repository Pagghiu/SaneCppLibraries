# Sane C++ Contributing Guidelines

> Note: this document will be updated regularly clarifying existing rules and adding missing guidelines that emerge from discussions, issues, Prompt Requests, or pull requests being reviewed.

Before deep diving in this document, please take some time to read the [Principles](https://pagghiu.github.io/SaneCppLibraries/page_principles.html) and [Coding Style](https://pagghiu.github.io/SaneCppLibraries/page_coding_style.html).

More importantly, browse the existing codebase.

## TLDR

Sane C++ Libraries is now maintained primarily through agentic development. Contributions are most useful when they provide clear intent, reproduction steps, constraints, and prompts that can be replayed or adapted by the maintainer.

In this project, a well-written issue can be the whole contribution.

Preferred contribution path:

1. Open an issue.
2. Describe the desired change, bug, or investigation clearly.
3. Include a reusable agent prompt when it helps.
4. Let the maintainer recreate, adapt, validate, and merge the final change.

Code pull requests are not the preferred path. Human-written code-only pull requests are not accepted. Agent-written pull requests may be considered only when they include a tested reproduction prompt and have been sanity-checked by a human.

Tiny documentation-only fixes, such as typos, broken links, or grammar corrections, may still be submitted directly. Substantial documentation changes that alter technical meaning should follow the issue or Prompt Request workflow.

## Maintainers

Currently the only maintainer of the library is [Stefano Cristiano (@Pagghiu)](http://github.com/pagghiu).

## Discussing

GitHub issues are the canonical place for actionable contributions. If a discussion on Discord, X, or GitHub Discussions leads to something that should be implemented or remembered, please capture it in an issue.

Please try your best to be nice when interacting with other people about this project.

- [Sane Coding Discord](https://discord.gg/tyBfFp33Z6)
- [X](https://x.com/pagghiu_) `@pagghiu_`
- [GitHub Discussion](https://github.com/Pagghiu/SaneCppLibraries/discussions)

Alternatively I am also reading the following Discords too:

- [Italian C++ Discord](https://discord.gg/GPATr8QxfS) (`@Pagghiu` from any appropriate channel or just a DM, English and Italian are both fine)

## Contribution model

Historically this project has received little external code contribution, so these guidelines are optimized for the way the project is now actually maintained.

The project prefers:

- Bug reports with reproduction details.
- Feature requests explaining desired behavior and constraints.
- Prompt Requests: issue-first contributions that give a maintainer or agent enough context to investigate, design, or implement a change.

The name "Prompt Request" is intentionally a little playful. In these guidelines, however, "pull request" always means a GitHub branch with code.

### Issue types

Use the GitHub issue templates when possible:

- Bug Report: broken behavior, regressions, platform issues, or unexpected failures.
- Feature Request: desired APIs, missing functionality, missing platform support, or behavior changes.
- Prompt Request: agent-ready context, investigation notes, implementation direction, and optionally a reusable prompt.

A Prompt Request does not need to include the solution. It can ask the maintainer or an agent to investigate. The useful part is the durable context: what should change, why, how to reproduce or validate it, and which constraints matter.

A reusable prompt is optional but preferred, especially for bugs and feature requests where an agent can reproduce the problem or generate a similar fix.

Issues may include small code snippets, API sketches, logs, or examples. Large generated patches should not be pasted into issues; either summarize the intended change or submit a code pull request that follows the stricter code PR rules. Code snippets in issues are treated as examples or sketches, not as patches to apply verbatim.

## Writing a good issue

A good issue usually includes:

- The library, tool, or platform affected.
- The desired change or bug to fix.
- Why it matters.
- Reproduction steps or a motivating example, when applicable.
- Constraints that are specific to this request.
- Expected validation: tests, platforms, commands, logs, or screenshots if relevant.
- A suggested agent prompt, if it helps.

If an issue or pull request was prepared with an agent, ask the agent to read `AGENTS.md` and any relevant local `AGENTS.md` files before producing prompts, examples, or code. The prompt should mention any extra constraints specific to the requested change.

Do not include secrets, private keys, proprietary code, private transcripts, or confidential context in issues, prompts, or pull requests.

By submitting an issue, prompt, code example, or pull request, you agree that the contribution may be used under the project license.

### Example Prompt Request

```text
Title: HttpClient: Add a request timeout option

Objective:
Add a timeout option to HttpClient requests so callers can fail requests that take too long.

Context:
The API should fit the existing streaming-first HttpClient design and avoid allocations in the request path.

Constraints:
Ask the agent to read AGENTS.md and Libraries/Http/AGENTS.md first. Preserve existing platform backends. Do not add STL, exceptions, RTTI, hidden allocations, or public system headers.

Suggested prompt:
Starting from a clean checkout of Sane C++ Libraries, inspect the existing HttpClient request options and timeout-related async primitives. Propose and implement the smallest API addition that lets callers set a per-request timeout. Add focused tests that cover timeout firing and the no-timeout default behavior.

Validation:
Run SCTest for the relevant HttpClient tests in Debug. If platform-specific behavior is touched, explain which platforms were tested.
```

## Code pull requests

Code pull requests are treated as worked examples plus reproduction prompts. They are not the preferred way to contribute.

If you submit a pull request with code:

1. Find the fix or feature however you prefer.
2. Once the fix is known, ask your agent to produce a precise prompt that can recreate the same or a substantially equivalent fix from a clean checkout and clear context.
3. Try that prompt yourself.
4. Submit the code only if the reproduction prompt works well enough to guide a fresh agent run.
5. Include the reproduction prompt, validation performed, and agent disclosure in the pull request.

If producing a useful reproduction prompt is too much work, open an issue instead. Issues are the preferred and lighter contribution path.

The reproduction prompt does not need to produce byte-identical code, but it must be specific enough to recreate the same fix or feature direction. The maintainer may replay or adapt the submitted prompt using a different agent, model, thinking level, or local workflow. The submitted prompt is evaluated as a reproduction aid, not as a requirement to produce identical output.

The maintainer will normally revert, discard, recreate, or adapt the submitted implementation before merging. Contribution credit is preserved through commit trailers and acknowledgements, not by necessarily merging the submitted commits as-is.

Code-only pull requests submitted through the traditional workflow may be closed with an invitation to open an issue or resubmit with a tested reproduction prompt. Pull requests missing the required prompt, human review statement, or agent disclosure may also be closed and can be reopened or resubmitted after conversion.

### Human sanity-check

Code pull requests must include a human review statement.

The contributor is not expected to deeply understand every line of generated code, but must have looked over the generated change at a high level and believe it is reasonable, not obviously nonsensical, and aligned with the requested behavior.

For code pull requests, the contributor should also:

- Understand the intended behavior change.
- Check that the reproduction prompt can recreate a similar fix from a clean checkout.
- Run relevant tests when possible, or clearly state why tests were not run.
- Verify that the change does not obviously violate project constraints.

Fully agentic pull requests submitted without human sanity-checking will be closed.

### Agent disclosure for code PRs

Agent disclosure is required for code pull requests. It is optional for issue-only Prompt Requests, unless the issue includes generated code intended as a concrete implementation sketch.

For code pull requests, disclose:

- Agent or tool name.
- Model, if visible.
- Thinking or reasoning level, if configurable or visible.
- Whether the final patch was produced by the agent, manually edited, or regenerated.
- The reproduction prompt used for the final attempt.
- Whether the prompt was tested from a clean checkout.

Full private transcripts, hidden reasoning, screenshots, and vendor-specific metadata are not required.

### Sensitive areas

Extra care is required for changes touching:

- `SC::Async`, `SC::Await`, or `SC::Process`.
- `Libraries/Foundation` or `Libraries/Common`.

These areas affect core runtime behavior, async/process correctness, or widely included header-only code where size and compile-time impact matter. Pull requests touching them must include a clearer explanation of intent, risk, and validation.

## Credit and acknowledgements

Accepted issue, prompt, and investigation contributions can be credited in two ways:

- Commit trailers, such as `Reported-by:`, `Prompted-by:`, or `Suggested-by:`.
- `ACKNOWLEDGEMENTS.md` for prompt-only, issue-only, or investigation contributions that led to merged changes or accepted project decisions.

Acknowledgements are added after a contribution leads to a merged change or an accepted project decision. Opening an issue does not automatically create an acknowledgement entry.

The default public identity for acknowledgement entries is the GitHub handle. Real names are used only if they are already public in the linked issue or pull request, or explicitly requested.

Maintainers may summarize or lightly edit submitted prompts when referencing them in commits, acknowledgements, or follow-up issues, while preserving the linked original.

## Finding something useful to report

- Take a look at the [good first issue](https://github.com/Pagghiu/SaneCppLibraries/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22) list.
- Grep the project for `TODO:` and open an issue or Prompt Request around one that matters.
- Take a look at each specific [Library](https://pagghiu.github.io/SaneCppLibraries/libraries.html) page **Roadmap**.
- Report platform gaps, missing validation, unclear APIs, and documentation holes.

## Do not expand project scope

For now project scope is defined by the existing set of libraries.

The main objective is making them production ready on all supported operating systems.

New library proposals are out of scope for implementation contributions. You may open an issue to discuss future ideas, but do not submit a code pull request adding a new library unless the maintainer has explicitly requested it.

In general, likely useful issues or Prompt Requests include:

- Bug fixing.
- Implementation of existing libraries on unsupported operating systems.
- Wrapping existing libraries to make them easily usable from C.
- Focused documentation improvements.
- Focused validation and regression tests.

## Avoid feature creep

Even for existing libraries, there is no intention of covering [everyone's use cases](https://xkcd.com/927/).

The general guideline is focusing on a small enough subset of features covering `90%` of common use cases.

## No external dependencies

This project encourages a _from scratch_ attitude when writing new code.

It's also positively accepted any use of existing operating system API, properly wrapped, if such functionality is readily available on all supported operating systems.

`macOS` and `Windows` are easy to support in this regard as each version identifies a precise set of APIs that are available.

> Exceptions will need to be made on `Linux` as some functionality is not part of the kernel, and presence of a given user-land library is not guaranteed.
> The exact mechanism to deliver such dependencies is still to be investigated, defined, and regulated.
> In any case Linux dependencies will need to be wrapped so that they will still not require or impose any build system.

## No accidental internal dependencies

This project tries hard to keep each library independent from others, so that it can be more easily consumed as a Single File Library.

Do not add new internal dependencies unless they've been widely discussed in any of the discussion channels or issue tracker and approved prior to implementation.

An automatic check on CI prevents the accidental inclusion of unneeded dependencies.

## OS-specific code

Operating system specific code should be isolated in dedicated libraries and not spread throughout the codebase.

## Testing

New behavior should include focused tests when practical. For bug fixes, prefer a regression test that fails before the fix. Larger changes need broader validation.

For code pull requests, run relevant tests when possible and always disclose what was and was not validated.

Agent-generated code is not exempt from the project rules. Generated or regenerated changes must follow the Principles, Coding Style, no-allocation/library-independence constraints, and the normal test/build expectations.

## Language

- The project official language is American English.
- Try to use proper spelling and grammar.
- Please avoid any offensive wording.

## Commit message format

Every commit should:

- Start with the name of the library being touched followed by colon (`:`) and a short description, starting with a capital letter, without any dot at the end.
    - In some cases it's possible to use `Everywhere` if a group of changes is not related to a specific library.
    - Use `Documentation` for changes to the `.md` files.
    - Use `CI` for changes to the Continuous Integration files.
- Avoid use of past tense (`Use` instead of `used` etc.).

Where needed add two newlines and write a further paragraph, starting with a capital letter, describing the details of the change.

Examples:

Good:

- `SerializationBinary: Use pointer to field directly instead of reinterpret_cast-ing`
- `FileSystem: Add option to skip empty entries to Path::join`
- Example of multi-line commit text with description paragraph:

```text
Everywhere: Support Visual Studio 2019

This is needed in order to build and run tests on the GitHub Windows Server 2019 runner (that has Windows 10 kernel)
```

Bad:

- `Fix stuff` (missing name of the library getting touched and meaningless description)
- `FileSystem: Added Path::join option` (use of Added instead of Add)
- `FileSystem: Add method to list files.` (dot at the end of short description)

## Git workflow and commit squashing

The project uses a `rebase` workflow to keep linear history.

For the rare code pull request that is accepted for review:

- Related commits should be squashed when that clarifies intent.
- Formatting-only fixes should usually be folded into the commit that introduced the formatting issue.
- Pull requests will be rebased on top of latest `master` when merged.
- If the branch cannot be automatically rebased, the contributor may need to rebase it before the pull request can be considered.

The maintainer will normally recreate or adapt code changes before merging, so the submitted branch history is mainly a review and reproduction aid.

## Naming things

Naming things is one of the hardest tasks in Computer Science, together with cache invalidation, and is also extremely important to make a library easy to use.

For this reason, consider renaming structs, methods, and variables after you have finished building the functionality.

Often their real essence will then be evident.

## Code guidelines

Generated, regenerated, or manually edited code must:

- Harmonically blend with existing code.
- Follow the project [Principles](https://pagghiu.github.io/SaneCppLibraries/page_principles.html).
- Follow the project [Coding Style](https://pagghiu.github.io/SaneCppLibraries/page_coding_style.html).
- Avoid STL, exceptions, RTTI, hidden allocations, public system headers, and accidental dependencies in library code.
- Use the same patterns you can recognize being already used in existing code.
- Write code that adheres to the project [License](License.txt).

Do not:

- Do not make large impacting changes without prior maintainer discussion.
- Do not pointlessly move existing code around for no reason.
- Do not leave commented-out code in commits.

## Issue guidelines

- Keep one issue per bug, feature request, or Prompt Request.
- Clearly identify the objective with a meaningful title.
- Include enough context for a maintainer or agent to reproduce the issue or understand the request.

## Maintainer workflow for code pull requests

For code pull requests, the maintainer will normally:

1. Review the linked issue, reproduction prompt, human sanity-check statement, agent disclosure, and validation.
2. Replay or adapt the prompt locally, possibly with a different agent or model.
3. Recreate, adapt, or rewrite the implementation.
4. Preserve contribution credit with commit trailers and/or `ACKNOWLEDGEMENTS.md`.
5. Close pull requests that do not meet the reproduction prompt, disclosure, or human sanity-check requirements.

## Comparisons

Comparisons are often carriers of negative sentiments that this project wants to avoid.

They are sometimes made to demonstrate _superiority over others_ or to _crush the competition_ and express similar negative thoughts that are not in the interest of this project.

`Sane C++` doesn't compete but offers an alternative vision on how to program in a subset of C++ that is simple and quick to compile. Anyone liking this vision will use the library, with or without other libraries.

Those who feel particularly inspired might support the project with an issue, Prompt Request, pull request that follows these guidelines, star, or like.

It would also be very difficult to compare libraries that have very different and distant scopes, goals, constraints, and levels of maturity.

It's always possible to create a test or to identify some criteria where one or the other might be better.

[Benchmarks](#benchmarks) are the only allowed form of comparison.

## Benchmarks

Benchmarks are the only allowed form of [comparison](#comparisons).

They should be without bias and built with objectivity and completeness.

It's not yet the right time to do them as everything is still very much in draft state.

They will be considered in the future for some specific libraries (`SC::Async` and `SC::Http` for example) when they will be mature enough.

It's preferable adding a `Sane C++` implementation to an existing benchmark, already comparing other libraries, rather than creating a new one.
