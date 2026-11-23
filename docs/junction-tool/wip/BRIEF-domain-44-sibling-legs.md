# BRIEF domain-44: F8, sibling legs keep distinct headings at their node

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. Test code only (the judge's harness checks). Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- F8 (domain-38): on P1, with the lane A1#3 at 32 and the row A1#4 at 28 half tiles and C1's heading-2 piece 8 at 22, the geometry is connected and C1 runs ON the lane A1#3 along y = 3 into their merge (node 0). The domain refuses (`connection`: track ends meet with no node, tiles (5,1)..(15,1)); the judge ACCEPTS: too loose. Reading: at a node the judge checks each leg against the trunk (`NodeTurnsHold`, F6) but never leg against leg; after contraction both sibling legs arrive with heading 2, and their walls are consecutive in the face walk, so keep-apart never pairs them.
- The rule (the node shape, D400/H3, on contracted headings): at a node, the legs' headings at the node are pairwise distinct (and each within one eighth of the trunk, as F6 already checks). Zero tracks are transparent as in F6.
- Read first: `NodeTurnsHold` and the F6 tests; `DrawingProgramJudge`; the P1 fixture; DECISIONS D400 (node rule), D436.

## 2. Changes
1. `NodeTurnsHold` also refuses when two legs of one node have equal headings at the node (mod 8), with the same zero-track transparency (a zero leg is skipped; a zero trunk resolves as now). The judge names it `node turn` (as F6). Update its doc comment: "F6 and F8".
2. Pinned behaviour test (two-sided): "planar domain: sibling legs arriving on one heading are refused (F8)": the geometry above (domain-38's numbers; build it on P1 and confirm the domain refuses with `connection`): the judge refuses with `node turn`. Counterpart: P1's base (C1's last contracted piece one eighth off A1#3's): judge and domain accept. Show the test fails with the new check removed (scratch run), then restore.
3. Rerun the eight fixtures' exactness (and their hidden reports in Release): all stay 0/0; a NEW too strict (a legal geometry with equal sibling headings) is a finding: report it in full, do not change the check.

## 3. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5` (Debug), `tools/junction/jt gate build-rel` once.
- One commit: `Fix: Junction: sibling legs keep distinct headings at their node in the domain suite's judge (F8, D436)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. The Slots item: STATUS will carry it (no production change here).

## 4. Report (short)
References read; commit; the F8 geometry as built and both verdicts; the switched-off evidence; exactness after; gates; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
