# Brief PB3 (probe, throwaway): a bundle S-bends from its pre slots at its start

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`. Branch `probe-pb2` is at {HEAD}. Create `probe-pb3` from it; commit only there. Finish with `git switch wt-planar` and rebuild `openttd_test` in build-rel.
- Time box: 60 minutes.
- Read first: `/home/pete/OpenTTD/docs/junction-tool/wip/BRIEF-PB2-probe.md` and the "Take 2 (PB2)" section of `wip/PROBE-PB.md`.

## Why
PB2's IIS: the joiner channel D is one bundle piece from the fray to the join. The slot layer puts D's lanes 1 apart in `pre` (the fray's legs) and 3 apart in `post` (the joiner spread), a legal slot step between `pre` and `post`. But `Bundle` (planar_components.cpp, the `Bundle` method) only makes S-bends between pieces (`if (k > 0 && !this->SlotBend(...))`), and a one-piece channel's piece takes its `post` offsets, so the step from `pre` has no S-bend.

## Change (planar_components.cpp, `Bundle` only)
1. Initialise `prev` to the channel's `pre` offsets, `this->Offs(c, {"pre"})`, instead of all zeros (return if `M.error` is set after it).
2. Make the per-lane S-bend before each piece's lead unconditional: drop the `k > 0 &&` so that piece 0 also S-bends from `pre` to its own offsets. (On piece 0 whose intervals include `pre`, the delta is 0 and `SlotBend` adds nothing.)
3. Remove PB2's unused `backs` leftovers in `Join`.
4. Nothing else.

## Measure (as PB2)
- pv one at a time: T2, T3, LL2 to `<S>/pb3-<hub>.txt`, S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`; compare with `<S>/s3-<hub>.txt` (T2 28/32 862.25; T3 28/32 1933.25; LL2 12/12 2230.25).
- Per hub: built/failed with reasons and counts; best and tag vs S3; better/worse/equal and median change; median ms vs S3's file.
- If T2 or T3 builds none: IIS on o1t3f1s0 as in PB (`pb3-iis-<hub>.txt`), rows with file:line.
- End to end on T2's and T3's best variant: the mapped dump test as in BRIEF-PB-probe.md (Measure, last item). Report PlanPlanarJunction success or its message, checker violations (rule, count); save output to `<S>/pb3-e2e-<hub>.txt`.

## Do not
As in BRIEF-PB-probe.md.

## Commit and report
- Commit on probe-pb3: `Probe: Junction bundle S-bends from its pre slots at its start (throwaway)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Append a "Take 3 (PB3)" section to `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-PB.md` (do not commit).
- Reply with: tables, IIS if any, end-to-end result, commit hash, `git status --porcelain`.

When you have reported, stop.
