# PROBE: LL_RR x LL_RR with PSG 227's choice pairing (2026-11-14)

Branch `wt-choice-probe` in `.claude/nodes` from 80ada077bb. Patch: `docs/junction-tool/wip/choice-probe.patch`. Full dump: `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/choice-probe/probe.txt`.

## Result in one paragraph
Expansion gives the 18 paths, every out slot has 3 sources (test green: 9 assertions). The track graph builds (24 nodes, 42 edges, 23 crossings). The seed then stops in `OrderJunctionTrackGraph` with "a row edge to a node the row has not made is not ruled yet" (junction_topology.cpp:812). That is not one of the refusals named in REPRESENT's list (about 890, 959, 970, 976), so per brief step 5 the probe stopped there; nothing past ordering ran (no structure, programs, placement, render, checker, prio). The whole run takes well under 1 s (run under `timeout 15`, no timeout).

## Stages per run
| Run | Stage reached | Failure |
|---|---|---|
| 1 (the only one) | expand OK, graph OK, seed fails in ordering | "a row edge to a node the row has not made is not ruled yet" |
No refusal from the named list was reached, so none was skipped.

## The 18 paths (slot = g<group>l<lane>(s<index>); groups g0 W-in, g1 W-out, g2 S-in, g3 S-out, g4 E-in, g5 E-out; slot indices by JunctionPortSlots)
Movements: m0 g0>g3, m1 g0>g5, m2 g2>g1, m3 g2>g5, m4 g4>g1, m5 g4>g3.
```
p0  g0l0 -> g3l0 m0   p1  g0l1 -> g3l0 m0   p2  g0l1 -> g3l1 m0
p3  g0l0 -> g5l0 m1   p4  g0l1 -> g5l0 m1   p5  g0l1 -> g5l1 m1
p6  g2l0 -> g1l0 m2   p7  g2l1 -> g1l0 m2   p8  g2l1 -> g1l1 m2
p9  g2l0 -> g5l1 m3   p10 g2l1 -> g5l0 m3   p11 g2l1 -> g5l1 m3
p12 g4l0 -> g1l1 m4   p13 g4l1 -> g1l0 m4   p14 g4l1 -> g1l1 m4
p15 g4l0 -> g3l1 m5   p16 g4l1 -> g3l0 m5   p17 g4l1 -> g3l1 m5
```
Lane 0 of each in group is the fixed lane, lane 1 the choice lane (per brief; his actual fixed/choice lane per arm differs, REPRESENT 2.1).

## Graph
24 nodes (12 splits, 12 merges), 42 edges (In 12, Leg 18, Out 12), 23 crossings: (e2,e6) (e2,e21) (e6,e9) (e6,e24) (e9,e12) (e9,e13) (e9,e14) (e12,e14) (e12,e16) (e12,e22) (e12,e24) (e12,e38) (e13,e24) (e14,e24) (e14,e28) (e17,e22) (e18,e32) (e19,e21) (e19,e33) (e21,e22) (e21,e25) (e21,e32) (e27,e28). Edge list is in probe.txt.

Splits: each in group's two lanes: the fixed lane splits once by direction (2 paths, 2 destinations); the choice lane splits by direction first (n1: 4 paths into 2+2), then each pair by out lane (the line split). So the late line split is a plain second-level split, as REPRESENT 3 expected.

## Merge cascades per out slot (first merge listed first)
Every out slot merges the same-arm fixed and choice lanes first, then adds the other arm's choice lane. Choice legs do NOT merge first in any of the six. Sources written as in-slot of the path.

| Out slot | First merge | Second merge |
|---|---|---|
| g1l0 (W-out lane 0) | g2l0 fixed + g2l1 choice (p6, p7) | with g4l1 choice (p13) |
| g1l1 | g4l0 fixed + g4l1 choice (p12, p14) | with g2l1 choice (p8) |
| g3l0 | g0l0 fixed + g0l1 choice (p0, p1) | with g4l1 choice (p16) |
| g3l1 | g4l0 fixed + g4l1 choice (p15, p17) | with g0l1 choice (p2) |
| g5l1 | g2l0 fixed + g2l1 choice (p9, p11) | with g0l1 choice (p5) |
| g5l0 | g0l0 fixed + g0l1 choice (p3, p4) | with g2l1 choice (p10) |

So his order (the two choice legs of different arms merge first, then the fixed lane, REPRESENT 3) is not what `Keys` produces: it merges by source group first. That is REPRESENT's "merge order inside each join: not held", now confirmed by dump. Note the pairing makes each source arm's fixed+choice pair share a first merge, one two-source node plus one three-path edge per out slot.

## Failure
`OrderJunctionTrackGraph`: "a row edge to a node the row has not made is not ruled yet". Stage reached: graph built, order failed. Not fixed or skipped (not a named refusal).

## Code changes (patch)
ChoicePerLine enum member; ExpandJunctionLanes branch (through movements follow it; the pairing case in the inner switch is a no-op `break` because that branch handles it first); JUNCTION_BRIDGE_MAX_CROSSERS 4; probe test `src/tests/junction/placement/junction_probe_choice.cpp` tagged `[.probe]`, registered in placement/CMakeLists.txt (the registration point for placement tests; src/tests/CMakeLists.txt only adds the subdirectory).
