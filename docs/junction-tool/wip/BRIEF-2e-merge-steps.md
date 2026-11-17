# Brief 2e: a one-lane merge's exit step from the gap rule (no merge switches)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, head `{HEAD}`, build dir `build-rel` (Release).
- Plan: `/home/pete/OpenTTD/docs/junction-tool/DESIGN-pieces.md` §3 source 3; `wip/REVIEW-channels.md` Q4 (LL²'s exits are one-lane joins per node, so the neighbour across a pinned gap can be the other lane of the same out port).
- Today a site composed per node (`PlanarSitePerNode`) builds each merge node as a one-lane `Join` called with `exit_step = std::nullopt` inside `M.opt_group = "merge{n}j"`: an optional `Turn(h, +1)` exit step with a solver switch capped at `OPTMAX` (planar_components.cpp `build_nodes`, ~1356-1386; `Join` ~930-995). This step decides that exit step before the solve from the gap rule and removes the merge switches. Forks (`Fork`) are NOT changed.

## 2. Read first (once, offset/limit)
- `CODINGSTYLE.md` (C++ part); `src/junction/planar/CLAUDE.md` (the gap-steps section and the mapped-composer facts).
- `src/junction/planar/planar_gaps.h` and `.cpp` (whole), `src/tests/junction/planar/planar_gaps.cpp` (whole).
- `planar_components.cpp` ~930-995 (`Join`), ~1340-1390 (the site join call and `build_nodes`).
- `src/junction/planar/planar_plan.cpp` ~438-475 (`PlanarLL2Text`).

## 3. The rule (one-lane merges)
For a merge node whose out line O is a port line: its joiner arrives from `side` (+1 right of travel, -1 left; the composer's resolved value in `build_nodes`, tie bits included). Look at O's neighbours in `topology.ring` (predecessor and successor, no wrap) on the same `PlanarSide`, any port, including the other lanes of O's own port. For an out line the predecessor lies at -1 (left) and the successor at +1 (right). If a neighbour lies at `side`, the joiner lands in that pinned gap: O gets a required exit step toward `side`. Otherwise O gets no exit step.

## 4. Changes
1. `planar_gaps.h/.cpp`:
```cpp
/** The exit step of a one-lane merge onto a port line (DESIGN-pieces §3 source 3): toward the joiner's side when a ring neighbour on the same box side lies there, else 0. */
int8_t PlanarMergeExitStep(const PlanarTopology &topology, uint32_t out_line, int8_t joiner_side);
```
2. Tests in `planar_gaps.cpp` (red first, with the function returning 0), test **"planar gaps: one-lane merge exit steps"** on `PlanarLL2Text()`:
   - `EO1` with side +1 → +1; `EO1` with side -1 → 0 (no neighbour north of EO1 on the E side).
   - `EO2` with side -1 → -1; `EO2` with side +1 → +1 (`AB2`'s line lies south: confirm from the ring which AB lane is adjacent, and assert the step, not the lane).
   - One W side and one S side out line, each both ways, with the expected values worked out from the ring by the rule of §3 in the test's comments.
3. `Join`: the parameter `std::optional<bool> exit_step` becomes `int exit_step` (0: no exit piece; +1 or -1: a required `Turn(h, exit_step)` piece). The optional branch goes. The `Ge` ordering row of the pairing loop is added only when `exit_step != 0`, unchanged otherwise (the `Eq` row stays unconditional). When `exit_step == -1` and `n > 1`: `M.Fail("an exit step to the left is not composed")` (this moves the existing check from the call site into `Join`).
4. Site join call (~1350): pass `+1` when the gap step exists (its `toward`, which is +1 on every fixture; the existing left check goes into `Join`), else 0.
5. `build_nodes`: compute `const int8_t step = PlanarMergeExitStep(t, node.out.front(), side)` when `node.out.front()`'s `to` is a port, else 0; call `Join(..., step, 2)`; remove the two `M.opt_group` lines.
6. `src/junction/planar/CLAUDE.md`: the one-lane merge's exit step rule (present tense, no decision ids); "the only optional pieces left in the mapped composer are a fork's legs".

## 5. Measure (before at the launch head, after the change), output under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/2e/`
- T2 and T3, every variant singly (`o{0,1}t{0..3}f{0,1}s{0,1}`, `PLANAR_ONLY`), the INFO line or failure message: before and after must be identical (the T has no per-node merges).
- LL² (`"planar components: LL² from its topology text (mapped) at the probe's area"`), `timeout 60` each, with `PLANAR_TIMES=1`: o49t0f1s0, o49t0f0s0, o49t1f0s0, o1t0f0s0, o0t0f0s0. Record area, size, solve ms, round count and the first round's clash count, before and after.
- Also with `PLANAR_MAPPED_DUMP`, after, for o49t0f1s0: the `seq` lines of every out port line (EO1, EO2, WO1, WO2, SO1, SO2).

## 6. Do not
- Do not touch `Fork`, `Fray`, `Bundle`, `Flex`, `OPTMAX`, curve rows, the clash rules, the hand composer.
- No env knobs.

## 7. Stop conditions
- Any T2/T3 variant line differs: stop before committing, report.
- An LL² variant that built before fails construction after (`M.Fail` reason): stop, report.
- LL² areas may go up or down: report them, do not stop for that.

## 8. Gates
- `tools/junction/jt fast build-rel '[planargaps]'`, `'[planarmodel]'`, `'[planar]'`; `'[planarcomponents]'` (about 6 s against 5 s at the launch head in hand-composer tests: report its time).
- One commit: `Change: Junction one-lane merge exit steps from the gap rule`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 9. Report (short)
References read; commit; red evidence; functions/parameters added or changed; T identity result; the LL² before/after table; the out-port `seq` lines; gates with count and time; stops; `git status --porcelain`, `git log -1 --oneline`. When you have reported, stop.
