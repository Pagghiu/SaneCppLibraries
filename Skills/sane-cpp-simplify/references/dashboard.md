# Persistent SC simplification dashboard

The user reviews `_Plans/Simplification/index.html`, not a long chat list. Keep `opportunities.json` as the canonical current state and `Log.md` as append-only evidence/history. All three remain ignored. The reusable renderer/template are tracked in this skill. Render with:

```sh
python3 Skills/sane-cpp-simplify/scripts/render_dashboard.py
```

The HTML is self-contained and works as a local file. To preview in Codex, serve only the dashboard directory (`python3 -m http.server 8764 --bind 127.0.0.1 --directory _Plans/Simplification`) and open `/index.html`. Preserve that URL/port when possible so browser selections survive. Do not serve the repository root. The preview server is session-bound; the persistent artifact is the HTML file. Source references are repository-relative text with line anchors, not broken HTTP links to unserved files.

## Data contract

Top-level fields: `project` (stable storage key), `updated`, `revision` (audited source revision), `source_reduction` (achieved historical net maintained-source reduction), `summary`, `items`. Optional `suggested_batch` lists ready IDs and `suggested_description` explains their non-overlapping estimate. Update them each round; the selection button does not submit approval.

Every item requires:
- `id`: stable proposal ID; use `aliases` for prior investigation IDs instead of duplicate active rows.
- `area`, `title`: library/function grouping and concrete action.
- `purpose`: what the touched code is used for, in plain language.
- `change`, `why`: bounded proposal and maintenance reason.
- `saving`: honest display estimate including overhead; say clarity/additions/unknown where appropriate. `loc_max` is optional numeric sorting metadata, never a promise or achieved result.
- `benefit`: LOC, clarity, coverage, correctness, or a combination.
- `status`: `ready` (developed proposal), `explore` (unresolved technical design), `opinion` (user choice), `blocked`, `deferred`, `completed`, or `rejected` (closed).
- `priority`: smaller numbers first; `confidence` high/medium/low; `risk` plain description.
- `sources`: array of `{path, line, note?}`; verify references against the current revision.
- `evidence`, `caveat`, `validation`: source basis, behavior/compatibility to preserve, and checks needed after approval.
- `question`: required for `opinion`, phrased as a concrete choice. Optional `commit`, `outcome` for completed items.

Separate estimates outside historical Libraries/Tests/Examples/Tools roots (Extra/Deprecated, Support). Moving code between roots is not savings. Explain overlaps and never aggregate gross deletion or overlapping proposals.

## Round lifecycle

Read current JSON and journal before auditing. Update existing items in place; preserve IDs and user decisions. New findings should be useful as a batch, not drip-fed as one proposal per turn. Update the board after every investigation and implementation round. On completion, record measured savings, validation limitations and area-specific commits; set `completed`. Completed/rejected items automatically move into a collapsed bottom archive. Keep the detailed evidence journal rather than deleting history.

The UI saves selection/notes in localStorage, supports area/stage/search filtering, grouping and several sort orders, and exports a readable batch request plus JSON. Browser choices are not approval until the user sends them in chat. Read the actual sent request; do not infer approval from a selection or copied draft. Preserve distinctions between implementing, investigating, answering a question and deferring. An unresolved/opinion item cannot be selected for implementation without first resolving the question; the exported opinion/constraints provide that response.

For technical uncertainty, investigate with source and workers first. If a consequential question remains unresolved, use the authorized persistent same-project strong-tier advisor, with low or moderate reasoning effort where supported, or put an amber `opinion` item with an explicit question for the user. Do not quietly label speculative work ready. Record advisor reasoning and remaining uncertainty. Product/organization preferences belong to the user; avoid paying a strong-tier advisor to guess them.

## Verification

After renderer/template changes, generate the board, inspect it in a browser, and exercise search, filters, sorting, a batch containing multiple actions, persistence across reload, completed archive and export. Remove only your test selections afterward. Check small-panel layout and no script errors. Validate duplicate IDs, required fields, opinion questions and safe embedded JSON; the Python renderer rejects malformed basic records and escapes `<` in data. No C++ build is required for dashboard-only changes. Run repository formatting as required before committing the skill area; do not stage ignored operational data.
