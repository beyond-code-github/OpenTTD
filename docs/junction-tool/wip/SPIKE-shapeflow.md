# Spike: shape as a flow (findings)

Code: spike-shapeflow.patch (on 72c03f4196). Raw outputs: /tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/shapeflow (one file per variant, PLANAR_SHAPEFLOW_OUT.<tag>; full T2 set in all/). Brief read from the main worktree (the file is not in ng1's tree).

Interpretations taken: stretch segments = event_seg range i0..i1 (zero-length segments dropped); a ring edge in an inner face is one reverse boundary dart (a ring end's rotation always turns into its line), side from the port, corners -2 each; LP 2 gives every crossing corner (per face) its own variable, magnitude [1,3], sign of the rough turn, NOT coupled to the other three corners of the same crossing; heading changes counted inside stretches only.

## Face check (does today's geometry satisfy the flow system)
All runs: 0 faces fail the -8 check (T2 32 faces/31 inner, 60 stretches; T3 65/64, 126; LL2 38/37, 78), all 28 solved T2 variants too. No cusps (turn 4) inside any stretch. The face map and the layout agree.

## Runs (LP1 = vertex turns and ring fixed; LP2 = crossing corners free; both LPs integral every time, 1-2 ms)
- T2 o0t2f1s1: LP1 8 vs rough sum|b| 12 (8 stretches differ, e.g. D1#1 -1 -> 0, D1#3 0 -> -1, F1#3 -1 -> 0, F1#5 +1 -> 0); LP2 1 (11 differ). Jogs 18 stretches, 48 of 72 heading changes.
- T3 o1t2f1s1: LP1 12 vs 12 (0 differ: rough is already a minimum); LP2 1 (11 differ). Jogs 34, 92 of 122.
- LL2 o1t3f0s0: LP1 15 vs 30 (31 differ, e.g. F1#3 2 -> 1, D1#5 2 -> 0, D1#3 0 -> 1, WO1#0 0 -> -1); LP2 2 (about 25 differ). Jogs 18, 40 of 92.
- Full T2 (28 variants): LP1 objective 6-10 vs rough 8-16 (rough already minimal in 8 variants with sum 8; LP1 below rough in 20); LP2 objective 1 (20 variants) or 2 (8). Jogs 15-20 per variant (mean 17.5), mean 45 of 68 heading changes (about two thirds).

## Reading
- The LP is integral with no branching and instant; the system is consistent with the geometry.
- LP1 often finds a lower net-turn total than the rough solve by moving a turn from one stretch to the neighbour across a face (the flow is face-sum only; where the turn sits along a line is free).
- LP2 collapses to 1-2 because uncoupled crossing corners absorb everything; its numbers say nothing until the four corners of a crossing share one angle variable (corners x, 4-x).
- About two thirds of all heading changes sit in stretches with net turn 0 (jogs): invisible to this flow, so it cannot choose or price S-bends.
