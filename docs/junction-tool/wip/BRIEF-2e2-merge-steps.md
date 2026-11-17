# Brief 2e-2: one-lane merge exit steps, one per pinned gap (no merge switches)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, head `{HEAD}`, build dir `build-rel` (Release). Build only `openttd_test`, never `openttd`. Never use git stash, never start a game client, never touch ports 4077/4079.
- Background: `/home/pete/OpenTTD/docs/junction-tool/wip/BRIEF-2e-merge-steps.md` (the first attempt; read it), `wip/PROBE-2e-merge.md` (why it failed). The first attempt's code is saved as `wip/2e-merge-steps-wip.patch`: apply it first (`git apply`; if it does not apply cleanly to {HEAD}, apply by hand and say so).
- What changes from the first attempt: only the rule. The first attempt stepped both out lanes into the gap between them (EO1 +1 toward EO2 and EO2 −1 toward EO1), which is infeasible. A pinned gap needs widening once. The launch head's own solves (optional steps) use exactly one step per gap: EO1, WO1, SO1 step +1; EO2, WO2, SO2 have none; that matches the T, whose exit close-ups are one per gap, stepping toward the ring successor.

## 2. The rule (replaces brief 2e §3)
For a merge node whose out line O is a port line, with its joiner arriving from `side`: O gets a required exit step `+1` exactly when `side == +1` and O's ring successor (next in `topology.ring`, no wrap) is on the same `PlanarSide`. Otherwise no step. (For an out line the successor lies at +1, its right; so the step owner of each pinned gap is the out line before it in ring order, and it steps toward the next line.)
`PlanarMergeExitStep(topology, out_line, joiner_side)` returns that (+1 or 0); it never returns −1 now. Update its doc comment.

## 3. Tests (in `src/tests/junction/planar/planar_gaps.cpp`, the first attempt's test "planar gaps: one-lane merge exit steps", rewritten; red first against the patched function's current behaviour, which returns −1 for EO2 with side −1)
LL² (`PlanarLL2Text()`): EO1 side +1 → +1; EO1 side −1 → 0; EO2 side −1 → 0; EO2 side +1 → +1 (AB lies after it on the E side); WO1 side +1 → +1; SO1 side +1 → +1; the last out line before a corner (no successor on its side) with side +1 → 0 (find one from the ring and name it in a comment).

## 4. Composer
As the first attempt: `Join`'s `int exit_step` (0, +1, −1; −1 with n > 1 fails), the site join passing its gap step, `build_nodes` passing `PlanarMergeExitStep(...)` for port out lines (0 otherwise), no `merge{n}j` opt group. `src/junction/planar/CLAUDE.md`: the one-lane merge rule (present tense, no decision ids).

## 5. Measure
- `pv` baselines at {HEAD} before applying anything: `/home/pete/OpenTTD/tools/junction/pv save build-rel T2` and `... T3`; LL² at {HEAD}: `pv run build-rel LL2 --tags <file> -j 8 --out <before>` with the tag file `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/ll2-tags.txt` (201 tags).
- After: `pv check build-rel T2` and `T3` (must be unchanged: the T has no one-lane merges); LL² with the same tag file, then `pv diff <before> <after>`. Report: solved counts, best area and tag before/after, per-variant time (median and max) before/after, every variant that newly fails with its reason, and for up to three newly failing ones the `PLANAR_IIS=1` core.
- `PLANAR_TIMES=1` on o49t0f1s0 and o1t3f0s0 after: rounds and ms.

## 6. Do not
- Do not touch `Fork`, `Fray`, `Bundle`, `Flex`, clash rules, the variant enumeration. No env knobs.

## 7. Stop conditions
- Any T variant changes: stop before committing.
- More than 10% of LL²'s previously solving variants newly fail: stop before committing, report the diff and cores.

## 8. Gates and commit
- `tools/junction/jt fast build-rel '[planargaps]'`, `'[planarregions]'`, `'[planarmodel]'`, `'[planar]'`.
- One commit: `Change: Junction one-lane merge exit steps from the gap rule (one per pinned gap)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 9. Report (short)
References read; commit; red evidence; LL² before/after summary; T check; gates; stops; `git status --porcelain`, `git log -1 --oneline`. When you have reported, stop.
