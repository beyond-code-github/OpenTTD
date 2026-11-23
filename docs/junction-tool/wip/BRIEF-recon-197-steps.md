# BRIEF recon-197-steps: do V's S-bends follow a lengths-free node rule?

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. Read-only analysis plus one hidden test. Change no production code.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- Context: Slots cannot choose optional S-bends inside the integer program (`docs/junction-tool/wip/probes/slots/flank-lock.md`). With a choice fixed, it sizes the lengths quickly. The question: can the choice be made without lengths, by a rule?
- A tally of `tools/junction/topologies/psg197.steps.txt` (V's 81 sideways steps per stretch, D396) shows 67 of 81 on stretches touching a node (node-node 15, node-crossing 25, crossing-node 27), 7 crossing-crossing and 7 at ports.
- The hypothesis (H): a step on a stretch that STARTS at a split moves the leg AWAY from its sibling leg there; a step on a stretch that ENDS at a merge moves the leg TOWARD its sibling leg there. Direction only; the lane count comes later.

## 2. Read first (once, with offset/limit)
- `tools/junction/topologies/psg197.steps.txt` (whole, 89 lines) and the script that generated it (`tools/junction/split_shape.py` or wherever its header says). Learn exactly what the stretch index means (an event index along the track?) and what the headings are.
- `src/tests/junction/planar/planar_draw.cpp` around lines 1043 and 1323: how a test loads PSG 197's structure variant (`LoadPlanarStructureVariant` with `psg197.structure.txt`).
- `src/junction/planar/planar_regions.h`: `PlanarTrackMap`, including `tracks`, `events`, `expanded` (lines with from/to ends at nodes), `side` per stretch pair, and how a side is looked up. Also the site/straight-leg data the variant carries (`sites`).

## 3. The change: one hidden test
`TEST_CASE("planar 197: V's steps against the node rule (recon)", "[.][recon197steps]")` in `src/tests/junction/planar/planar_draw.cpp`, after the existing 197 tests. It loads the 197 structure variant as those tests do, parses `psg197.steps.txt` (`ReadTopologyFileForDraw`), and for each step record writes one line:
- track, stretch index, raw headings, kind (from the file's comment);
- step side: the sign of the first turn in the raw headings (h1 - h0 normalised to -1 or +1; -1 = toward the left of travel, +1 = right). If the list turns more than once in the same direction before returning, or does not return to its first heading, mark it `complex`, still with the first turn's sign;
- for a stretch that starts at a node (kinds `node-*`): the node, whether it is a split (the track is an out line there) or a merge, and the sibling (the other line(s) on the same side of that node: the other out line at a split, the other in line at a merge). The same for a stretch that ends at a node (kinds `*-node`);
- the sibling's side relative to the track on that stretch, from the track map's side table (+1 right, -1 left, 0 or missing = say `none`);
- the verdict for H: at a split at the stretch's start, `away` if step side == -sibling side, `toward` if == sibling side; at a merge at the stretch's end, the same words. H holds when the start-split case says `away` or the end-merge case says `toward`. Other combinations (merge at the start, split at the end, both, neither) are reported as such.

Then the counts: H holds / fails / not applicable (no node, side none, or a merge at the start or split at the end), per kind; and the 14 non-node steps listed with the nearest event on each side (crossing with which track, and whether that crossing is in a structure with a copy).

Output: env `PLANAR_RECON_OUT` (default `recon-197-steps.md`) and echo. Run with `PLANAR_RECON_OUT=docs/junction-tool/wip/probes/shape/recon-197-steps.md`.

## 4. Do not
- Do not change production code or any other test. Do not infer a rule beyond H; report what the data says.
- If the stretch index's meaning is unclear after reading the script, stop and report.

## 5. Gates
- Build `build-rel` (Release) with output to a file; run the hidden test once.
- Add a "Reading" of at most 8 lines to the output file, facts only: H's hold/fail counts per kind, and what the failures and non-node steps have in common, if anything.
- Commit: `Doc: Junction: V's 197 steps against the node rule (recon)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 6. Report (short)
References read; commit; the stretch index's meaning; the counts table; the Reading; stops; `git status --porcelain`; `git log -1`. When you have reported, stop.
