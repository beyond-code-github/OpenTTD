# Brief S1b: the slot layer models the doubled bridge as a parallelogram (D351)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar` at {HEAD}. Commit on `wt-planar`.
- Ruling D351 in `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` (grep `^| D351`). Background: `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-S2.md`.

## Read first
- `src/junction/planar/planar_slots.{h,cpp}` and `src/tests/junction/planar/planar_slots.cpp`.
- `src/junction/planar/CLAUDE.md`, its "Slot layer" section.
- `CODINGSTYLE.md` (C++ part).

## Rule to add
For every interval `x<k>` that holds a `Copy` track of line L, and for the interval that follows `x<k>` in the same channel (`mid<k>` or `post`):
- add the row `s(L, next) - s(copy of L, x<k>) = 0`: the line carries on along its copy's track;
- do not charge L's step from `x<k>` to `next` in the objective. It belongs to the bridge, so leave out that pair's two step variables, or fix them at 0 cost; pick whichever keeps the code smallest.

Nothing else in the model changes.

## Tests (first, red, then green)
1. **New:** "planar slots: a doubled line carries on along its copy's track". On T2 o1t3f1s0 and LL² o17t0f1s0: for every `Copy` in an `x<k>`, the line's slot in the next interval equals the copy's slot. Red before the rule exists: record the failure text.
2. **Existing gap-change tests (T2, LL²) and the objectives:** run them after the rule. Where they fail, update each expected value to the new output, and list every change as `pair: old -> new` in your report, objectives included. These are the measured values under D351, not tuned ones.
3. The "tracks" and "fast" tests must still pass unchanged.

## CLAUDE.md
In `src/junction/planar/CLAUDE.md`'s "Slot layer" section, add the fact: a doubled line's slot after its crossing equals its copy's slot (a parallelogram; the bridge's move is not charged). Present tense, no D-refs.

## Gates
- Debug: `tools/junction/jt fast build '[planar]'`.
- Release: `tools/junction/jt gate build-rel`. The gate is near its 30 s budget and flakes under load (the CPU is unstable). Rerun once on failure, and name any failure in code you did not touch.
- Commit: `Change: Junction slot layer models the doubled bridge as a parallelogram`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
- the commit hash;
- the red text;
- the list of expectation changes;
- gate results;
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
