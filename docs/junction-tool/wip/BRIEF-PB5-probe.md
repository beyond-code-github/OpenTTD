# Brief PB5 (probe, throwaway): a fray's turning legs leave with the fray's cross-section (slot fact)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`. Branch `probe-pb4` is at {HEAD}. Create `probe-pb5` from it; commit only there. Finish with `git switch wt-planar` and rebuild `openttd_test` in build-rel.
- Time box: 60 minutes.
- Read first: `wip/PROBE-PB.md` (all takes) in `/home/pete/OpenTTD/docs/junction-tool/`; `src/junction/planar/planar_slots.cpp` on probe-pb4 (the site link loop ~:160-190); the n-lane `Fray` in `planar_components.cpp`.

## Why (orchestrator, from reading `Fray`)
In `Fray`, every turning leg `V[k]` starts at the in lane's end point `sp[k]` and leaves on one diagonal heading; the in lanes' ends are spaced by the fray gap (from the fray channel's last-interval slots). So the turning legs' channel (on T3: D) starts with exactly the fray's cross-section. The slot layer does not know this: a turning leg has no link, so D's `pre` slots float, and the joiner spread (PB2) put D at 0/3/6 already in `pre` (PB4's debug print). Composer: fray gap 1 apart, bundle piece 0 pinned 3 apart: base infeasible.

## Change (planar_slots.cpp only)
1. **Fray cross-section.** For each site s with `channels.sites[s].kind == PlanarSiteKind::Fray`: fc = `channels.sites[s].channel`. For each node n of the site, its in line `topology.nodes[n].in[0]` and its turning out line = the out line that is not `channels.node_legs[n].straight`. All turning lines must be in one channel `vc`; if not, stop and report. Order the (in, turning) pairs by the turning line's position in `channels.channels[vc].lines`. For each consecutive pair (a, b) in that order: add the row `s(turn_b, vc, 0) - s(turn_a, vc, 0) = sign * (s(in_b, fc, last(fc)) - s(in_a, fc, last(fc)))`, with `sign = +1` if `in_b` comes after `in_a` in `channels.channels[fc].lines`, else `-1`. Comment: "A fray's turning legs leave with the fray's cross-section."
2. Nothing else.

## Measure (as PB4)
- pv one at a time: T2, T3, LL2 to `<S>/pb5-<hub>.txt`, S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`; compare with `<S>/s3-<hub>.txt` (T2 28/32 862.25; T3 28/32 1933.25; LL2 12/12 2230.25).
- Per hub: built/failed with reasons and counts; best and tag vs S3; better/worse/equal and median change; median ms.
- If T2 or T3 builds none: IIS on o1t3f1s0 (`pb5-iis-<hub>.txt`), rows with file:line; and print once (temporary, removed before commit) every interval's line slots for the channels named in the IIS.
- If the failures are "a slot step on a diagonal piece is not composed", report which channel and piece (temporary print), then stop measuring.
- End to end on T2's and T3's best variant: the mapped dump test as in `wip/BRIEF-PB-probe.md` (Measure, last item). Report PlanPlanarJunction success or its message, checker violations (rule, count); save output to `<S>/pb5-e2e-<hub>.txt`.

## Do not
As in `wip/BRIEF-PB-probe.md`.

## Commit and report
- Commit on probe-pb5: `Probe: Junction a fray's turning legs leave with its cross-section (throwaway)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Append a "Take 5 (PB5)" section to `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-PB.md` (do not commit).
- Reply with: tables, IIS or slot prints if any, end-to-end result, commit hash, `git status --porcelain`.

When you have reported, stop.
