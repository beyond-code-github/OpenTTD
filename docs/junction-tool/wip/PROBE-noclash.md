# Probe: no clash loop (PLANAR_NOCLASH=1), rough solve may overlap, ladders up to 6 passes
Worktree .claude/ng1 at 72c03f4. Patch: wip/probe-noclash.patch. Raw logs (per variant stderr): scratchpad noclash/{T2,T3,LL2}/*.log. IIS cores: wip/probe-noclash-iis-*.txt.
Count: legal = final clashes 0 and exported. Baseline = same binary without env.

| hub | variants | solved today | legal now | ladder infeasible (first pass, "base infeasible") |
|---|---|---|---|---|
| T2 | 32 | 28 | 22 | 6 (the other 6 solved today end with clashes) |
| T3 | 32 | 27 | 27 | 1 (o0t2f0s1) |
| LL2 | 201 | 189 | 0 | 189 (every one, pass 0) |
Rough solve fails (also today): T2 4, T3 4, LL2 12.

Area vs today, legal and solved today: T2 22 same; T3 5 better, 18 same, 4 worse (all +8: o1t0f1s1 o1t1f1s1 o1t2f1s1 o1t3f1s1).
Passes used (ladder solves, histogram 0..4): T2 {0:6,1:18,2:4}; T3 {0:1,1:18,2:5,3:2,4:2}; LL2 {0:189}. Never 5 or 6 passes. Passes 0 = ladder solve failed straight away.
Time (ms per variant, median/max): T2 85/436 (today 113/847); T3 157/534 (today 174/588); LL2 1562/3166 (today 4199/8638).
Mean rough clashes per variant: T2 6.8, T3 11.2, LL2 25.3. Rung rows not holding on their source solution (summed): T2 88, T3 89, LL2 3272 (expected, rough overlaps).
Example (T3 o1t3f1s1): rough 26 clashes area 1907; pass0 0 clashes 4 moved; pass1 0 clashes 0 moved; final 1968 (rungs add area).

Why the failures: every infeasible core (2 read: T2 o1t0f0s1, LL2 o3t7f0s2, first 30 lines in the iis files) holds rung rows that
contradict the geometry rows. A rung is read from the overlapped rough ("this segment faces that point"); where the rough overlapped, the
rung demands an order the strand/flex/deck rows cannot give (LL2: rung E1#2.end.y vs E2a#3.end.y with a flex fixing E2a to start at +2 from
E1#2; T2: two rungs on A2#3.end.x against a deck chain). The clash loop used to resolve those overlaps before rungs were read.
Variants that end with clashes keep the rough's clashes (the failed ladder solve leaves the rough solution). Pairs (strand#seg~strand#seg):
 T2 o1t0f0s1: A1#1~B2#2 A1#2~B2#4 A1#2~B2#5 A1#2~B2"#0 A1#4~B1"#0 A2#1~B1#5 A2#2~B2"#0 A2#4~B1#8 A2#4~B1"#1 (T2 f0s1 variants share A1/A2~B1/B2 pairs)
 T3 o0t2f0s1: A1#1~B2#5 A1#1~B3#2 A1#4~B1"#0 A1#4~B2#8 A1#4~B2"#0 A1#4~B3#5 A1#4~B3"#0 A2#1~B1#5 A2#1~B3#2 A2#4~B1#8 A2#4~B1"#1 A2#4~B2"#1 A2#4~B3#5 A2#4~B3"#1 A3#1~B1#5 A3#1~B2#5 A3#4~B1#8 A3#4~B2#8 AF3#1~D3#1 AF3#2~D3#0
 LL2 o3t7f0s2: A1#4~A2a#0 A1#4~A2a#4 A1#4~F1a#2 A1#4~F1a#4 A2a#0~prio:A1#0 A2a#3~prio:A1#0 A2a#4~prio:A1#0 B1a#2~C1#4 B1a#4~C1#4 C1#3~C2a#0 C1#4~C2a#4 C2a#4~prio:C1#0 D1#10~E2b#1 D1#10~E2b#2 D1#11~E2b#1 D1#11~E2b#2 D1a#5~E1#3 D1b#0~D2#9 D1b#3~D2#9 D1b#4~D2#9 E1#3~E2a#0 E1#3~E2a#3
Answer: on T2/T3 the re-read converges in 1-2 passes (max 4) to the same area as today (22/22 same on T2; T3 +5 better/-4 worse by 8), and faster. It fails
where rungs come from a rough with overlaps on prio/flex-fixed strands (T2 f0s1 only 6 of 28, T3 1 of 27, all LL2). So rungs cannot be read from an overlapping rough on LL2 as the model stands.
