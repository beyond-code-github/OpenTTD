# Probe: ladders 5, stretches within a run and bridge copies as walls with deck middles exempt (2026-11-18, HEAD 05e0159b09)

Result: both gaps close. Every first-pass AND area-pass round-0 clash is gone on T2, T3 and LL2 (T2 80/112 to 0/0, T3 91/200 to 0/0, LL2 4740/254 to 0/0), no area worse anywhere, rows violated on the rough solve 0 on all 244 solved variants. The two gap cases (T3 o1t2f1s1, T2 o1t0f1s1) go to 0 clashes in both passes with area equal to rough (1975.25, 967.25).
Code: wip/probe-ladders5.patch (the full diff on HEAD, PLANAR_LADDER=3). Raw: scratchpad `ladders5/` (summary.txt, T2/ T3/ LL2/, parse.py, run1.sh). Note: probe-ladders4.patch is already cumulative (it contains ladders3), so it was applied alone; applying 3 then 4 fails.

## Changes
- Stretches: each run's segment list is cut into groups of consecutive segments of one strand (= one line, consecutive stretches of a line merged). Every ordered pair (X,Y) of groups in a run that are not adjacent in the list (cyclic only when the face has one run) goes through the same emit routine as run-vs-run (nearest facing, off-wall drop, RungSpacing, sign from the run's direction). One-run faces are no longer skipped (the composer now skips only faces with no runs; one-wall box faces skipped 0).
- Copies: a copy joins its run exactly as probe 2 (side test on d1.a vs base b0.a along Right(h) on the rough solve), with probe 2's COPYOFF end-point drops always on (the literal rows were infeasible in probe 2). A copy is also added to the group of its base line (the group holding base seg `first`, else the first group of that line) so stretch pairs see it; CopyCand got a `run` field. Copies in no group: 0.
- Deck middle: the lazy loop marks decks per segment (M.decks, DeckRec n/f on a segment; Clashes skips the pieces between the heads, heads stay track). A copy segment with a DeckRec is a deck middle: it is no target for points of segments without a DeckRec, and its own points get no rows against such segments. Deck-vs-deck, ramp/head and base segments are as before.

## Per hub (variants solved; ladder pass 2)
| hub | rough/ladder solved | first-pass r0 rough to ladder (variants at 0) | area-pass r0 | area b/s/w | ladder ms med/max |
| T2 | 28/28 of 32 | 80 to 0 (18 to 28) | 112 to 0 | 0/28/0 | 33/252 |
| T3 | 27/27 of 32 | 91 to 0 (20 to 27) | 200 to 0 | 0/27/0 | 68/84 |
| LL2 (201 tags) | 189/189 | 4740 to 0 (0 to 189) | 254 to 0 | 16/173/0 | 1438/2493 |
- Rows (pass 2, summed over variants): total / change 1 stretch-vs-stretch / from copies / against copies: T2 6097/697/258/71, T3 10035/576/452/113, LL2 64866/9125/2213/817. Violated on the rough solve: 0 everywhere.
- Area better (LL2, all f0, same as probe 4): o1t5f0s1/s3 -39; o3t{0,1,4,5,6,7}f0s1/s3 -154; o3t3f0s1/s3 -183. No worse. Rough infeasible at HEAD (no ladder): T2 4, T3 5, LL2 12.
- Surviving clash pairs: none, first pass or area pass, on any hub.
- Timings are from 6 parallel jobs, so the ms are upper bounds on a quiet box.
