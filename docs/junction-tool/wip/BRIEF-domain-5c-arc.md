# BRIEF domain-5c: arc consistency before the polyline search

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. Test code only. No rerun loops. Never run git stash.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- The harness's polyline search (`src/tests/junction/planar/planar_domain_encoding.cpp`) leaves 226 polylines undecided at 50,000 nodes under `span+lead`. Typical case: `B1:0*2,1*2,0*10` with A1 about 495 representations, D1 45, C1 30. A constraint between B1 and a late track makes the depth-first search re-explore the product. Most constraints (keep-apart pairs, program rows) involve exactly two tracks: arc consistency prunes each track's representations against every other track's before the search, and an emptied list proves rejection.

## 2. The change (harness only)
- Constraints: every keep-apart pair (its two tracks; a pair on one track is unary, already filtered), every lead row and every program row by its track set (`PlanarProgramPlan`'s per-row track set; rows with one track are unary, already filtered).
- Before the depth-first search, per polyline: AC-3 over the binary constraints (track sets of exactly two). A representation r of track a survives against track b when some representation s of b makes every binary constraint over {a, b} hold with a at r and b at s (other tracks' points: unused by those constraints). Revise until no list changes. An empty list: rows reject the polyline (no search). Count each pair evaluation in the node budget with a separate cap of 5,000,000 evaluations per polyline (over it: undecided).
- The depth-first search then runs on the pruned lists with constraints of three or more tracks checked as now.
- To evaluate a binary program row for a pair of representations, use the partial check (`placed` = the two tracks) on the points of those two tracks; the other tracks' points may be anything (they are not read).

## 3. Test (fast, harness file): "planar domain: arc consistency prunes and proves"
On X1's representation lists for the base polyline with two hand constraints: one binary constraint that no pair satisfies (expect: a list empties, rejection); one that only a single representation of A1 satisfies with any of B1 (expect: A1's list pruned to one). Use `PlanarPointRow`s you build in the test as the constraints. Red: without the AC-3 pass the second expectation fails.

## 4. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5`; `tools/junction/jt gate build-rel` (one rerun if only the LL² joint case fails).
- Release `plain+lead` and `span+lead` (overwrite the probes/domain files), timeout 300.
- One commit: `Change: Junction: arc consistency in the encoding search (D428)`, Co-Authored-By Claude Sonnet 5.5.

## 5. Stop conditions
- A run over 300 s; anything not covered.

## 6. Report (short)
Commit; red evidence; per set the totals line in polylines (both accept, both reject, too strict, too loose, undecided); any new group keys with counts and first examples; run times; suites; state. When you have reported, stop.
