# Code pull request checklist

Code pull requests are not the preferred contribution path. Before submitting, make sure this PR includes a tested reproduction prompt and human sanity-check. If not, please open an issue instead.

Tiny documentation-only fixes, such as typos, broken links, or grammar corrections, may be submitted directly.

## Related issue

- Related issue:

- [ ] This PR links an issue, unless it is a tiny documentation-only fix.

## Summary

Describe the change and why it is needed.

## Reproduction prompt

Include the prompt that can recreate the same or a substantially equivalent fix from a clean checkout and clear context.

```text

```

- [ ] I tested this prompt from a clean checkout, or explained below why I could not.
- [ ] I understand this implementation may be discarded or recreated by the maintainer using the submitted prompt, while preserving contribution credit through trailers or acknowledgements.

## Agent disclosure

- Agent/tool:
- Model, if visible:
- Thinking/reasoning level, if configurable or visible:
- Was the final patch produced by the agent, manually edited, or regenerated?

## Human sanity-check

- [ ] I have looked over the generated change at a high level and believe it is reasonable, not obviously nonsensical, and aligned with the requested behavior.
- [ ] I understand the intended behavior change.
- [ ] I asked the agent to follow `AGENTS.md` and any relevant local `AGENTS.md` files.
- [ ] By submitting this prompt, code example, or pull request, I agree that the contribution may be used under the project license.

## Sensitive areas touched

- [ ] `SC::Async`, `SC::Await`, or `SC::Process`
- [ ] `Libraries/Foundation` or `Libraries/Common`
- [ ] None of the above

If this PR touches a sensitive area, describe the intent, risk, and validation in more detail.

## Validation

List commands, platforms, and checks performed.

- [ ] I ran relevant tests locally and listed the commands/output below.
- [ ] I could not run tests locally and explained why below.

```text

```
