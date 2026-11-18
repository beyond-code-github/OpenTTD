# Brief PB4 (probe, throwaway): a link step across a site is an S-bend at the start of the receiving bundle

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`. Branch `probe-pb3` is at {HEAD}. Create `probe-pb4` from it; commit only there. Finish with `git switch wt-planar` and rebuild `openttd_test` in build-rel.
- Time box: 60 minutes.
- Read first: `/home/pete/OpenTTD/docs/junction-tool/wip/BRIEF-PB3-probe.md`, the "Take 3 (PB3)" section of `wip/PROBE-PB.md`, and `src/junction/planar/planar_slots.h` (`PlanarSlotLink`, `PlanarSlots::links`).

## Why
On T3 o1t3f1s0 the joiner channel D has slots 0/3/6 in `pre` and `post`: the slot layer took the spread on the site links from the fray into D (`links[i].step`), not inside D. The composer builds no S-bend for a link step: the fray's legs arrive at D's start 1 apart, and piece 0 is pinned 3 apart.

## Change (planar_components.cpp, `Bundle` only)
1. Where PB3 sets `prev = this->Offs(c, {"pre"})`, subtract each lane's link step relative to lane 0: with `step(line)` = the `step` of the entry of `this->slots.links` whose `to == line` (0 if none), set `prev[j] -= step(lines[j]) - step(lines[0])` for each lane j, where `lines` are the channel's lines in the same order as `Lanes(c)`. That is the offset at which lane j arrives from its site.
2. Nothing else: PB3's unconditional `SlotBend` before each piece's lead then builds the link step as an S-bend at the start of piece 0.
3. Note in a one-line comment: a link step across a site is built as an S-bend at the start of the next channel's bundle.

## Measure (as PB3)
- pv one at a time: T2, T3, LL2 to `<S>/pb4-<hub>.txt`, S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`; compare with `<S>/s3-<hub>.txt` (T2 28/32 862.25; T3 28/32 1933.25; LL2 12/12 2230.25).
- Per hub: built/failed with reasons and counts; best and tag vs S3; better/worse/equal and median change; median ms vs S3's file.
- If T2 or T3 builds none: IIS on o1t3f1s0 (`pb4-iis-<hub>.txt`), rows with file:line, and for T3 print once (temporary, removed before commit) each bundle's `prev` and `cur` per lane on piece 0 for the channels in the IIS.
- End to end on T2's and T3's best variant: the mapped dump test as in BRIEF-PB-probe.md (Measure, last item). Report PlanPlanarJunction success or its message, checker violations (rule, count); save output to `<S>/pb4-e2e-<hub>.txt`.

## Do not
As in BRIEF-PB-probe.md.

## Commit and report
- Commit on probe-pb4: `Probe: Junction link steps become S-bends at the bundle start (throwaway)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Append a "Take 4 (PB4)" section to `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-PB.md` (do not commit).
- Reply with: tables, IIS if any, end-to-end result, commit hash, `git status --porcelain`.

When you have reported, stop.
