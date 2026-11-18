# Brief PB6 (probe, throwaway): a plain fray's straight legs keep its cross-section; bundle corners absorb slot changes

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`. Branch `probe-pb5` is at {HEAD}. Create `probe-pb6` from it; commit only there. Finish with `git switch wt-planar` and rebuild `openttd_test` in build-rel.
- Time box: 60 minutes.
- Read first: `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-PB.md` (all takes); `src/junction/planar/planar_slots.cpp` and the n-lane `Fray` and `Bundle` in `planar_components.cpp` on probe-pb5.

## Why (orchestrator, measured with a temporary heading print on wt-planar)
- T2/T3 fray site 1 (channel 4, non-prio, side -1) is a tie site whose variant-chosen straight channel is 9 (D, headings 0 1 0 1 2) and other 8. Its nodes have `node_legs[n].straight == UINT32_MAX`, so the slot layer's site-link loop links nothing into D and D's slots float. In `Fray` a non-prio fray's straight legs continue on the fray heading from the in lanes' ends: rigid, at the fray's cross-section.
- PB5's rule (turning legs keep the cross-section) is false: the turning legs leave on `Turn(h, sigma)` (a diagonal) and the turning channel's first piece is on `h` (e.g. LL2 "other headings: 2 3 4" with fray heading 2), so unequal leg lengths are a free fan. It made LL2 worse on all 12.
- In `Bundle`, consecutive pieces always differ by 45 degrees; the bundle-offset rows at a piece's start and the previous piece's tail are two projections of the same corner points, so a slot change there is met by staggered corners (as a fan). The explicit `SlotBend` at a piece start is not needed between pieces and refuses diagonal pieces (PB5: D piece 1, heading 1).

## Changes
### `planar_slots.cpp`
1. Remove PB5's turning-leg rows (both commits' version).
2. **Plain fray straight legs.** For each site s with `channels.sites[s].kind == PlanarSiteKind::Fray` and `!sites[s].prio`: fc = the fray channel, sc = `sites[s].straight`. For each node n of the site: in line `topology.nodes[n].in[0]`, straight out line = its out line whose channel is sc. Order the pairs by the straight line's position in `channels.channels[sc].lines`; for consecutive pairs (a, b) add `s(str_b, sc, 0) - s(str_a, sc, 0) = sign * (s(in_b, fc, last(fc)) - s(in_a, fc, last(fc)))`, `sign = +1` if `in_b` comes after `in_a` in `channels.channels[fc].lines`, else `-1`. Comment: "A plain fray's straight legs leave with its cross-section." If sc is not a valid channel or a straight line is missing, stop and report.
3. Nothing else.

### `planar_components.cpp`, `Bundle` only
4. Keep PB3's piece-0 `SlotBend` from the arriving offsets. For k > 0, do not call `SlotBend` before the lead (the corner between pieces k-1 and k absorbs the change through the bundle-offset rows). The doubled piece's tail `SlotBend` stays.
5. Nothing else.

## Measure
- pv one at a time: T2, T3, LL2 to `<S>/pb6-<hub>.txt`, S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`; compare with `<S>/s3-<hub>.txt` (T2 28/32 862.25; T3 28/32 1933.25; LL2 12/12 2230.25).
- Per hub: built/failed with reasons and counts; best and tag vs S3; better/worse/equal and median change; median ms.
- If T2 or T3 builds none: IIS on o1t3f1s0 (`pb6-iis-<hub>.txt`), rows with file:line, plus (temporary print, removed before commit) the line slots of every interval of the channels in the IIS.
- End to end on T2's and T3's best variant: the mapped dump test as in `wip/BRIEF-PB-probe.md` (Measure, last item). Report PlanPlanarJunction success or its message and the checker violations (rule, count); save output to `<S>/pb6-e2e-<hub>.txt`.

## Do not
As in `wip/BRIEF-PB-probe.md`.

## Commit and report
- Commit on probe-pb6: `Probe: Junction plain fray straight legs keep cross-section, bundle corners absorb slot changes (throwaway)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Append a "Take 6 (PB6)" section to `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-PB.md` (do not commit).
- Reply with: tables, IIS if any, end-to-end result, commit hash, `git status --porcelain`.

When you have reported, stop.
