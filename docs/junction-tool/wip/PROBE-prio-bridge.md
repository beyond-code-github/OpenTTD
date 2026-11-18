# Probe: prio coverage over a doubled bridge, T2/T3 (2026-11-18, throwaway, ng1 @ 0958786f36, reverted)
Patch: wip/probe-prio-bridge.patch (env PLANAR_PRIOBRIDGE=1; unset = today). Runs via `tools/junction/pv run build-rel <hub> -j 1` (one job at a time), outputs in the scratchpad probe-pb/.

## Changes made (as briefed)
- G9c (junction_rules.cpp, 4b): skip a switch event in coverage when it is a copy rejoin (existing `IsRejoinMerge`, D302: other trackdirs carry exactly the same (from_slot, to_slot) set) or a copy split. NOTE: no split-side marking existed; I added `IsCopySplit`, the mirror of `IsRejoinMerge` (every other trackdir the lane can take at the split tile carries the same slot pairs as its own), reading the G1 same-movement notion (D322). Never exercised: nothing solved.
- Composer: 2-lane `Join` and n-lane `Join` near-lane last segment `AddSeg(.., h, 2)` plus one row, sum of that strand's segment lengths >= 2P+2.

## Result: nothing solves
- T2 mapped: baseline 28 of 32 variants solve, best o1t3f1s0 862.25 (94x29). Probe: 0 solve (28 "a clash is not separable", 4 "base infeasible", the same 4 as baseline).
- T3 mapped: baseline 28 of 32 solve, best o1t3f1s0 1933.25 (138x39). Probe: 0 solve (27 not separable, 4 base infeasible, 1 infeasible with topology sides).
- So no area/size after, no checker run, no signal pass, no ASCII picture: the end-to-end checker on mapped plans (`src/tests/junction/planar/planar_plan.cpp`) has no plan to take. The checker change (G9c) is untested.

## Clash (PLANAR_ONLY=o1t3f1s0 PLANAR_IIS=1 PLANAR_DEBUG=1; T2 and T3 give the same IIS, strand names shift: E2/D1 vs E3/D2)
IIS is 9 rows, repeated for each crossed joiner (T2: D1#8 and D1#6; T3: D2#8 and D2#6):
- Strand E2'' (copy 1 of the near lane's doubled bridge, seg #1, the deck): `E2''#1.L = v440 + 4` and `v435 + 4 <= v440` (deck at least 4 past the crossing, D145 tight head) with the cross row `E2''#1 > D1#8` (the joiner D1's diagonal crosses the deck).
- Ties: `E2#8.end = E2''#1.end` (copy rejoins) and `E2#9.end = ED2.start` (the short last segment, L>=0 after the change).
- `tie@L1761`: the prio row prio:E2 starts at `ED2.start.x - 28`, a FIXED 28 before the merge (the row length is 2P+2, set by the prio row, not by the strand segment).
- `(extra) sep D1#8 prio:E2#0 dir(-1,0)`: `D1#7.end.x - prio:E2#0.end.x >= 2`.
- Reading: the prio row (28 long, anchored at the merge) is separated from the joiner's diagonal D1 by 2, but the joiner D1 must cross the near lane's deck E2'' which now sits inside those 28 tiles. The row's start point is pinned to the merge, so the joiner cannot cross before the row starts and cannot cross the row: a clash between prio:E2#0 and D1's diagonal (segments D1#7/D1#8), not separable. This is the row (prio_rows, composer ~1761), not the coverage segment: the sum row alone does not release the row.
- No rule added (per brief).

## What it implies
- Lengthening nothing is not enough: the prio row geometry (`prio_rows`, one `h`-long row anchored at the merge) assumes the strand beside it is a plain straight 2P+2. D342/D347 need the row to be gapped at the joiner crossing (D342 allows it). A probe of that (a row split in two pieces around the doubled stretch, or the row's `sep` against joiners dropped inside the deck span) is the next lever; not done here.
- Per-variant tables (area, size, ms, reason): scratchpad probe-pb/{T2,T3}-{base,probe}.txt; every baseline variant solves at the areas above (T2 862-1455, T3 1933-2714) and none solves in the probe.
