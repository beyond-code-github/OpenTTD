# Probe PB: bendy prios, joiner bridges straight, prio lanes S-bend under it inside their coverage (2026-11-19, throwaway, branch probe-pb, commit 5a38cb4d9b on bc47f12f73)

## Change
- planar_slots.cpp: Row tracks restored as at 20f98229e0 (last interval of a prio join's straight channel, beside each line on side -site.side); the prio entry-gap block removed.
- planar_components.cpp, n-lane Join, prio case: joiners k+1<n are a 2 run, a straight deck (jdeck) and a 4 turn-in; full-share joiner fails "a full-share joiner over a prio is not composed yet"; far lane one 2 seg; near lanes 2, out(Turn sigma), 2, back(Turn -sigma), 2 with Eq(out.L - back.L, 0); coverage Ge(sum of L from first_k, 2P+2) tagged "prio coverage"; row strands "prio:<S>" real strands (parallel per segment at -sigma*2, end level with the merge; the box loop over M.order covers them, so no separate box rows; not in M.related); jdeck decks over out_j and the row segment beside it; out lanes use SlotBend with delta from the slot layer; no prio_rows pushed in this case. Non-prio Join case kept as before (restructured only so the prio branches sit inline).

## Mapped full sets (pv run build-rel, one at a time)
| hub | built | failed | best (S3 best) | change | median ms (S3) |
|---|---|---|---|---|---|
| T2 | 0/32 | 28 "infeasible with topology sides", 4 "base infeasible" (o0t3f0s*, o1t3f0s*; same 4 as S3) | none (862.25) | n/a | n/a |
| T3 | 0/32 | 28 "infeasible with topology sides", 4 "base infeasible" | none (1933.25) | n/a | n/a |
| LL2 | 12/12 | none | 2230.25 o0t1f0s1 (2230.25) | equal on all 12 (0 better, 0 worse, 12 equal) | 4483.5 (5102) |
LL2 has no n-lane prio join, so it is unchanged in area; ms differences are noise. Files: /tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/pb-T2.txt, pb-T3.txt, pb-LL2.txt.

## IIS (PLANAR_ONLY=o1t3f1s0 PLANAR_IIS=1; files pb-iis-T2.txt, pb-iis-T3.txt)
Both fail as "infeasible with topology sides": the lazy clash loop's separation rows made the solve infeasible, so each IIS contains "(extra) sep" rows (SepOptions, planar_components.cpp:2121).
T2 (13 rows): seg B1#13..#16 (AddSeg, planar_model.h:383); L1072 = Eq(outs.L - backs.L) (components:1072) on B1; tie@L1088 = Tie J/S ends (:1088); cross C2#5>B1#13@L1117 = jdeck over out_j (:1117); (extra) sep C1#5 C2#5 dir(0,-1) (twice); bundle offset C1#3/C2#3 (:1235-1236); seg C1#4,C1#5,C2#4.
T3 (28 rows): flex E1#2/E2#5/E3#5 (Flex, :1154); seg E2#6..#12, E3#6..#12, ED3#0,#1; L1072 on E2 and on E3 (out = back equality); (extra) sep ED2#0 ED3#2 dir(0,1); bundle offset C1#3/C2#3/C3#3 (:1235-1236); seg C2#4..#6, C3#4; (extra) sep C2#6 C3#5 dir(0,-1). The L1072 equalities on E2/E3 tie the S-bend out and back lengths, and with the flex offsets and the separations of ED2/ED3 and C2/C3 there is no solution; the core is the S-bend equality plus clash separations, not the new decks.

## End to end
Not run: no variant of T2 or T3 builds, so there is no best tag.

## Fast tests (jt fast build-rel '[planar]')
3 cases fail, all slot tests as expected: "planar slots: T2 o1t3f1s0 gap changes", "planar slots: a prio join's straight channel keeps the entry gaps", "planar slots: tracks". Not fixed.

## Take 2 (PB2, branch probe-pb2, throwaway)
Change: slots add J[k]-J[k-1] >= 3 (side-signed) in the joiner channel's last interval per prio join; near lanes S, out(Turn sigma), S with no back and no out=back equality; joiner turn-in at least 2, no greatest; out lanes 2 then an optional Turn(-sigma) of 0 or >= 2 (no SlotBend).
Result: T2 0/32 and T3 0/32 build, all 32 "base infeasible" (S3: 28/32; PB: 28 "infeasible with topology sides" + 4 base). LL2 12/12 best 2230.25 o0t1f0s1, equal on all 12 (median ms about 1180 per pv column vs S3). No clash rows are involved: the base model (before any lazy separation) is infeasible.
IIS (o1t3f1s0; pb2-iis-T2.txt, pb2-iis-T3.txt; the same 4-5 rows twice): the core is on the D lines, not the join: fray gap (planar_components.cpp:1008) fixes the fray's D lanes 2 apart (C2.start.x - C1.start.x = 2); seg D#0 and D1#0/D1#1 (AddSeg) run straight with no sideways piece from the fray to the bundle; bundle offset (planar_components.cpp:1230) demands D2 - D1 = 6 (T2) or D3 - D1 = 12 (T3), which is the slot layer's gap 3 from the new joiner spread. So the joiner channel (D) is the fray's out channel: its lanes leave the fray 2 apart and have no room to spread to 6 before the join. The spread rule needs a sideways piece (a fan) between the fray and the bundle, or it must apply only after a step.
End to end: not run (no variant builds).

## Take 3 (PB3, branch probe-pb3, throwaway)
Change: Bundle starts `prev` at the channel's `pre` offsets and S-bends before every piece including piece 0; PB2's unused `backs` removed.
Result: T2 0/32 and T3 0/32 build, all "base infeasible" (identical to PB2). LL2 12/12, best 2230.25 o0t1f0s1 (tag same as S3), equal on all 12 (median change 0); median ms 4521 vs S3 5102 (pv column, noise-level).
IIS (o1t3f1s0; pb3-iis-T2.txt, pb3-iis-T3.txt): identical to PB2: fray gap (:1008) 2 apart on D lanes, seg D#0/D1#0/D1#1 straight, bundle offset (:1231 now) D3-D1 = 12.
Why the change is a no-op: a debug print of (prev, cur) per lane per piece on T3 o1t3f1s0 showed prev == cur on every piece of every bundle, including the joiner channel D (c8: offsets 0/3/6 in `pre` as well as `post`; 3 pieces, no crossings). So `pre` is already 3 apart there, not 1 apart as the PB2 reading of the slot layer assumed, and no S-bend is ever added. The contradiction is between the fray gap (D lanes leave the fray 2 apart) and the slot layer's 3-apart `pre` for the joiner channel: the spread is demanded from the first piece with no step anywhere. Probe debug print removed before commit.
End to end: not run (no variant builds).

## Take 4 (PB4, branch probe-pb4, throwaway)
Change: in Bundle, `prev` (the channel's `pre` offsets) is reduced per lane by its link step relative to lane 0 (the `step` of the `slots.links` entry whose `to` is the lane's line).
Result: T2 0/32 and T3 0/32 build, all "base infeasible" (identical to PB2/PB3). LL2 12/12, best 2230.25 o0t1f0s1, equal on all 12 (median change 0); median ms 4461.5 vs S3 5102 (noise).
IIS (o1t3f1s0; pb4-iis-T2.txt, pb4-iis-T3.txt): identical to PB2/PB3: fray gap (:1008) D lanes 2 apart, seg D#0/D1#0/D1#1 straight, bundle offset D3-D1 = 12.
Why it is a no-op: the premise is wrong. Printing prev/cur/link step per lane on piece 0 of each bundle (T3 o1t3f1s0; pb4-dbg-T3.txt, pb4-dbgc.txt): bundles are built only for channels c6 (A), c8 (C), c9 (D), c11 (F); every lane has link step 0, prev == cur (D: 0/3/6). The six links with a non-zero step go AB->B (c3->c7), EF->E (c0->c10), B->CB (c7->c1), E->ED (c10->c2): all into core channels (c7, c10) or the fray channels (c1 CB, c2 ED), none into D, which no link targets (D is entered from the fray with no site between). So the slot layer's 0/3/6 spread on D is in `pre` itself, with no link step to take it from; the step between the fray's ED lanes and D is a channel boundary with no site (ED slots not printed here).
End to end: not run (no variant builds).

## Take 5 (PB5, branch probe-pb5, throwaway)
Change: planar_slots.cpp, per Fray site, a row tying consecutive turning legs' interval-0 slots (ordered by position in the turning channel) to the difference of their in lanes' last-interval slots in the fray channel. The turning out line is the out line whose channel is not sites[s].straight (the first commit used node_legs.straight, which ties at a fray node and selected both outs: all 76 variants failed "not in one channel"; superseded).
Result: T2 0/32, T3 0/32: 16 "slot step on a diagonal piece is not composed", 16 "infeasible" (base/sides). S3 28/32 each. LL2 12/12 built but worse on all 12: best 2374.25 (S3 2230.25), median change +183, median ms 5726.5 (S3 5102).
Diagonal step (temporary print, removed): channel 9 (D, the joiners' channel), piece 1 (heading 1, a 45-degree piece), lane D2, prev slot 1 (piece 0, which now matches the fray cross-section) and cur slot 3: the joiner spread 1 -> 3 is demanded at the start of piece 1, which is diagonal. T3 and T2 same lane/piece. Stopped measuring per the brief. IIS and end-to-end not run.

## Take 6 (PB6, branch probe-pb6, throwaway)
Change: planar_slots.cpp: PB5's turning-leg rows removed; per non-prio Fray site, consecutive straight legs' interval-0 slots (ordered by position in the straight channel) tied to the difference of their in lanes' last-interval slots in the fray channel. planar_components.cpp Bundle: SlotBend before the lead only for piece 0.
Result: T2 0/32, T3 0/32, all 32 "base infeasible" each (S3 28/32). LL2 12/12, areas equal to S3 on all 12 (best 2230.25 o0t1f0s1; median change 0); median ms 4557 vs S3 5102 (noise). No "slot step on a diagonal piece" failures any more.
IIS (o1t3f1s0; pb6-iis-T2.txt, pb6-iis-T3.txt): now at the prio join, not the fray. T3 (10 rows): bundle offset C1#3/C2#3 = 6 (components:1238) i.e. slot gap 3 on joiner channel c8 (pre and post 0/3/6); seg C1#4, C1#5, C2#4 (AddSeg); tie@L1085 C1#5 end to B1#14 (merge point); prio row offset (components:1105) B1#14 vs prio:B1#2 = 2; seg prio:B1#1, #2; cross C2#5>prio:B1#1@L1115 (jdeck over the row, 2 rows). T2 the same with D1/D2 and E2 (D2-D1 = 6).
Line slots: T3 c8 pre/post L24:0 L25:3 L26:6; c7 (prio straight, the row's channel) post L21:0 R21:1 L22:2 R22:3 L23:4 R23:5 (x0/mid0/x1 C21:0 L21:1 C22:2 L22:3 L23:4 C23:5); c9 pre L27:2 L28:3 L29:4, post L27:0 L28:3 L29:6; c10 post R30:0 L30:1 R31:2 L31:3 R32:4 L32:5. T2 c8 0/3, c9 post L18:0 L19:3, c7 post L14:0 R14:1 L15:2 R15:3.
End to end: not run (no variant builds).
