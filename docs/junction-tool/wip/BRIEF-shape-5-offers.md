# BRIEF shape-5: k alternating S-bend offers per place, and the L89 lattice default (D442, D443)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes wall. Production change in Shape's flex placement, behind parameters whose defaults keep today's behaviour (k = 1, lattice as today) until this step's measurement is read; the wide gate once. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- D442: Shape offers S-bends; a place offers k alternating offers each way (k = 3 carries V's 197: L3 `0 1 0 -1 0 -1 0`, L85 `2 1 2 1 2 3 2`, L72 `7 6 5 6 5 6`). D443: the lattice clause applies only when both crossings belong to one structure (V's L89 `5 6 5` between T37 and T39).
- Read first: DECISIONS D383, D397, D420, D442, D443; `docs/junction-tool/wip/probes/shape/recon-steps.md` (the model, file:line refs); `planar_shape_layer.cpp` `PlacePlanarShapeFlex` (~973) and `FreeOnePieceStretch` (~963, its channel clause; shape-3 added a `lattice` parameter: reuse or replace it, say which); how today's one pair each way is laid out at a place (the order of the h-1 and h+1 pairs).

## 2. Changes
1. `PlacePlanarShapeFlex` gains `int offers = 1` (doc: "D442: alternating offers each way per place; 1 is one pair each way (flex C, D420)"). At each place it emits the existing pattern `offers` times in alternation, so that any sequence of up to `offers` excursions in any order of directions is representable by zeroing the unused ones. State the exact emitted heading sequence for a one-piece stretch of heading h with offers = 1 and 3.
2. D443: the channel clause skips a stretch only when its two crossings belong to the same structure (a crossing's structure: the structure whose `crossings` list it); a parameter `bool lattice_same_structure_only = false` (doc: "D443"), so production stays as today until switched.
3. Production callers unchanged (defaults). The test harness and probes pass the new values where named below.

## 3. Measure (no default change yet)
- V's 197 (the option-A probe in planar_draw.cpp tests, ~2182, or the domain-36 mapping in the 197 hidden test): with offers = 3 and D443 on, are all of V's steps carried (multiplicity-aware: count per place, not just "a pair exists")? List any track still not carried.
- Counts on T1, T2, T3 and 197: flex pairs placed with (offers, D443) = (1, off), (3, off), (3, on).
- Shape's own tests (`[planarshape]`) and the domain suite pass with the defaults (they must: defaults are today's).

## 4. Tests
- fast `[planarshape]`: "Shape offers k alternating S-bends per place (D442)": a one-piece stretch with offers = 3 carries `0 1 0 -1 0 -1 0` (L3's) and `2 1 2 1 2 3 2` (L85's) as zeroings of its pieces; with offers = 1 it cannot (two excursions the same way).
- fast `[planarshape]`: "a stretch between crossings of two structures on one channel gets flex (D443)": on 197's L89 (or a small fixture if the 197 load is not fast): no flex with the parameter off, flex with it on.

## 5. Gates and commit
- `tools/junction/jt fast build '[planarshape]' 5` (or its TESTS.md budget), `[planardomain]` 5, `tools/junction/jt gate build-rel` once.
- One commit: `Add: Junction: k alternating S-bend offers per place, and the lattice clause per structure, as parameters (D442, D443)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Planar CLAUDE.md flex bullet updated (D225).

## 6. Report (short)
References read; commit; the emitted sequences; red evidence; V's 197 carriage with (3, on); the flex-pair counts table; gates; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
