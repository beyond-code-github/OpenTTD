# Probe: ladders with the flipped-triangle face map, no copies as walls (2026-11-17, HEAD 05e0159b09)

Result: the flipped face map is built and valid, no copies as walls removes the area growth and every rung row holds on f1 and LL2; but the T2 f1 C-vs-F round-0 clashes (8) do not go. Flipping was not the cause.
Code: wip/probe-ladders3.patch (on probe-ladders2.patch; PLANAR_LADDER=3, PLANAR_CLASHES=1; PLANAR_NOFLIPMAP=1 gives the old unflipped map). Raw: scratchpad `ladders3/` (t2all-summary.txt, t3all-summary.txt).

## Construction
- Copies off: the copies-as-wall loop and PLANAR_COPYOFF are gone; walls are base strands. Box-edge faces kept (one-wall box faces skipped: 6 on the flipped T2 map, 3 on the unflipped).
- Flipped map: in the probe block of MappedComposer::Build a copy of the topology has each moved channel's line events reordered by the flipped channel crossing order (events of the moved crossings, stable-sorted into the same slots; blocks stay whole). The same copy feeds event_seg/wall_segs (stretch k to segment range), because the strands follow the flipped order too. Not touched: PlanarLineSides in the composer, lazy loop sides.
- Validation: V-E+F=2 and simple walks hold on the flipped map, every variant run (why ''). Face count equal f0 vs f1 (flips preserve it): T2 32, T3 65, LL2 38. The faces differ (T2 o0t2f1: faces with D2/C2/F stretches change; unflipped map has 3 more two-wall faces, flipped map 6 one-wall box faces). Flip moves channels ch7, ch9, ch11 (2 crossings each).
- Side effect: unusable stretches 3 (unflipped) become 0 on T2 f1; rows violated on the rough solve S: 0 on every f1 run and LL2 (brief-literal rows were violated before; now off-wall rule only).

## Four runs (rough vs ladder; ms of that solve; round-0 clashes)
| T2 o0t2f1s1 | rough 906.25, 138 ms, r0 8 | ladder 906.25, 213, r0 8 (same 8 pairs), rows 296 (98 box), 0 moved |
| T3 o0t1f1s1 | 1941.25, 196, r0 0 | 1941.25, 209, r0 0, rows 573 (204 box) |
| LL2 o49t0f1s0 | 1654.25, 1870, r0 18 | 1654.25, 866, r0 0, rows 638 (192 box), pass 3 same (2 moved in pass 2) |
| LL2 o1t3f0s0 | 1992.25, 1389, r0 21 | 1992.25, 1385, r0 0, rows 646 (260 box), pass 3 same |
- T2 surviving pairs, round 0: C1#0/#2/#3 vs F2#4; C2#1/#3/#4 vs F1#4 and F2#4. In the flipped map C1#0 sits in one-wall box face 3 (skipped) and face 24 (with B1#6, D1#0, C2#1); F2#4 in face 15 (L=D2#1 F1#5 A2#2) and the skipped one-wall face 10 with D2#0 and the box. No face holds a C line and F2#4/F1#4 together with two walls.

## Full T2 set (32; t2all-summary.txt)
- Rough round-0 clashes 8: 10 variants. Ladder removes them in all f0 ones (8 to 0, area identical) and not in the four f1 ones (o0t0f1s0/s1, o0t2f1s0/s1: 8 to 8). Every other variant 0 before and after. Area: ladder equals rough on all f1 and o0 f0 variants; o1 f0 +74 (viol 3, slack -1: a D point right after a crossing vs F1#1, 6 variants). o0t3f0*, o1t3f0* infeasible in rough at HEAD (no ladder).
- No infeasible ladder run, so no IIS.
## Full T3 set (t3all-summary.txt)
- Round-0 rough clashes: 18 (o0 f0 s1 x3), 13 (o1t2f0s1, o0t2f0s1 rough fails), 3 (o1t2f1s0/s1). Ladder removes all f0 ones; the o1t2f1 3 stay. o1 f0 +91..94 (4 violated, slack -1). All f1 areas unchanged. Not run: free bends, a C-F corridor.
