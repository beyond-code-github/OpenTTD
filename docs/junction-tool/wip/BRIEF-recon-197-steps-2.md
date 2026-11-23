# BRIEF recon-197-steps-2: where V steps next to a node and where it does not

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. Read-only analysis plus one hidden test. Change no production code.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- Previous recon: `docs/junction-tool/wip/probes/shape/recon-197-steps.md`, from the `[recon197steps]` test at the end of `src/tests/junction/planar/planar_draw.cpp`. Of V's steps on stretches next to a node: after a split the step goes TOWARD the sibling leg in 35 of 36; before a merge, AWAY from it in 29 of 40.
- That covers only the stretches where V stepped. A rule must also predict where V does NOT step. This recon lists EVERY node-adjacent stretch.

## 2. Read first (once, with offset/limit)
- The `[recon197steps]` test and its output file (both above): reuse its loading, its stretch indexing (k = piece between crossing events k-1 and k), its sibling and side logic, and its step-side and `complex` logic.
- `src/junction/planar/planar_structure.h`: what the variant carries per site (straight leg, side, fray or lane split, join or lane merge) and how a track is a part of a topology line (`part_of`).

## 3. The change: one hidden test
`TEST_CASE("planar 197: every node-adjacent stretch, stepped or not (recon 2)", "[.][recon197steps2]")`, after `[recon197steps]`. It loads exactly as `[recon197steps]` does. For every track of the 197 variant and every node at one of its ends, take the stretch next to that node: the first stretch at a start node, the last at an end node. Write one row per (track, node end):
- track, stretch index, node index, `split` (the track leaves the node) or `merge` (it arrives);
- the node's kind from the variant's site data: fray or lane split at a split; join or lane merge at a merge; `copy/row node` when the node belongs to a copy or prio row the map added; `none` if not found;
- the track's role: `straight` (the site's straight leg), `diverging`, `trunk` (the node's single in line at a split or out line at a merge), or `fork leg` (no straight leg);
- the other end of the stretch: `node`, `crossing` (with the crossing's other track and whether that crossing is in a structure, and whether the structure has a copy) or `port`;
- whether V steps on it (a record in `psg197.steps.txt` for this track and stretch index): `step` with its side, direction relative to the sibling (`toward`/`away`/`none`) and `complex` flag as recon 1 computes them, or `no step`;
- the number of pieces of the stretch in Shape's 197 shape (`psg197.shape.txt` as loaded), and that stretch's headings.

Then cross-tabulate step / no step by (split or merge) x role x other end, and separately by node kind. Output to env `PLANAR_RECON_OUT` (default `recon-197-steps-2.md`), run with `PLANAR_RECON_OUT=docs/junction-tool/wip/probes/shape/recon-197-steps-2.md`.

## 4. Do not
- Do not change production code, `[recon197steps]` or any other test. Do not propose a rule; report the tables.
- If the site data does not give a stretch's role or node kind, write `none` and continue; do not infer it some other way.

## 5. Gates
- Build `build-rel` (Release) with output to a file; run the hidden test once.
- Add a "Reading" of at most 8 lines to the output, facts only: which feature combinations have (nearly) always a step, which (nearly) never, and how many rows are mixed.
- Commit: `Doc: Junction: every node-adjacent stretch of V's 197, stepped or not (recon 2)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 6. Report (short)
References read; commit; the cross-tables; the Reading; stops; `git status --porcelain`; `git log -1`. When you have reported, stop.
