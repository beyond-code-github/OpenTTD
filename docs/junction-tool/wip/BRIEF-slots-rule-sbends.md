# BRIEF slots-rule-sbends: force the diverging-leg S-bend rule in stage A on T1 and T2

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes wall. Each solve capped at 15 s, as the step 2 test does. Each run once. Change no production code (nothing under `src/junction/`).

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- Evidence:
  - `docs/junction-tool/wip/probes/shape/recon-197-steps.md`: after a split, V's step goes TOWARD the sibling 35 of 36 times; before a merge, AWAY 29 of 40.
  - `docs/junction-tool/wip/probes/shape/recon-197-steps-2.md`: V steps on the diverging leg's node-adjacent stretch 59 of 61 times.
- The rule under test (R1): every diverging leg has one S-bend on its stretch next to its node, toward its sibling at a split and away from its sibling at a merge.
- Background (`wip/probes/slots/flank-lock.md`): stage A cannot choose optional S-bends quickly (HiGHS has no incumbent in 15 s on T2 with flank rows), but sizes a fixed choice in 0.1-1.5 s.

## 2. Read first (once, with offset/limit)
- `src/tests/junction/planar/planar_draw.cpp`, the `[recon197steps]` and `[recon197steps2]` tests (from about line 4300 to the end): how they find a node's legs, the diverging leg's role (from the variant's sites and `BuildPlanarChannels(topology).sites`), the sibling, the sibling's side from the track map's side table, and the stretch next to the node.
- `src/tests/junction/planar/planar_domain_encoding.cpp`, the `[slotsstep2]` test (about line 6700 onwards): its env knobs, `lock_ref` and `curve_flex_from` (an insert whose reference is >= 0.25 gets lower bound 1; any other is held at 0, or free with `curve_flex_free`), `PLANAR_FLANK`, and the pipeline after stage A.
- `src/junction/planar/planar_shape_layer.h`: `PlanarShapeTrack::flex` (middle pieces), `pieces` (lifted headings), `event_piece`.

## 3. The change (test file `planar_domain_encoding.cpp` only)
In the `[slotsstep2]` test, add env `PLANAR_STAGEA_RULE`, with value `held` or `free`, next to the other stage A knobs. Where the duplicate block appears in `[slotsstep2cdiag]` too, add it to both, as earlier knobs were. When it is set:
1. `lock_ref` = every piece 0 (shape as `[slotsstep2]`'s lock code makes it). `sa.curve_flex_from = &lock_ref`, and `sa.curve_flex_free = (value == "free")`.
2. For each node of the variant's track map with a straight leg (skip forks, nodes without site data and copy/row nodes; count each skip by reason), for each diverging leg t of that node:
   - the stretch of t next to the node (its first stretch if t leaves the node, its last if t arrives);
   - s = the sibling's side relative to t on that stretch (as recon 1 computes it; +1 right of t's travel). If s is 0 or missing, count it as `no side` and skip;
   - the wanted insert direction d = +s at a split (toward), -s at a merge (away);
   - among t's flex middles m inside that stretch's pieces, those with `(pieces[m] - pieces[m - 1])` normalised to d (+1 = the insert turns right). Take the one nearest the node: the smallest m for a leaving leg, the largest for an arriving leg. Set `lock_ref[t][m] = 1`. If there is none, count it as `no offer` and record (track, node).
3. Write one line: rule inserts forced, `no offer` (with the list), `no side`, and the skips by reason.
4. The rest of the test runs unchanged: stage A, contraction, node turns, stage B lazy loop, polish, judge and domain.

## 4. Runs (Release, one at a time, `PLANAR_FLANK=1` on all)
For each hub T1 and T2 (`PLANAR_SLOTS_HUB`), run `PLANAR_STAGEA_RULE=held` and then `PLANAR_STAGEA_RULE=free`, with `PLANAR_STEP2B_OUT=docs/junction-tool/wip/probes/slots/rule-sbends.md` (appending). That is 4 runs.

## 5. Do not
- Do not change production code, other knobs' behaviour, or the rule (no other legs, no lane counts, no other S-bends forced).
- Do not add the movement objective or anything else to stage B.
- Do not raise caps or rerun.

## 6. Stop conditions
- With `PLANAR_STAGEA_RULE` unset the test path would change.
- More than half of the diverging legs on a hub are `no offer`: report the list and stop, because then the rule cannot be expressed with today's offers.
- Anything not covered here.

## 7. Gates
- Build `build-rel` with output to a file; grep it for errors.
- Add a "Reading" of at most 8 lines to rule-sbends.md, facts only. Per hub and mode: forced, no offer, stage A status and ms, node turns, listing, stage B (converged/rounds/pairs/ms), polish, judge and domain.
- Commit: `Doc: Junction: the diverging-leg S-bend rule forced in stage A on T1 and T2`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report (short)
References read; commit; per hub and mode the line from step 3 and the pipeline lines; the Reading; stops; `git status --porcelain`; `git log -1`. When you have reported, stop.
