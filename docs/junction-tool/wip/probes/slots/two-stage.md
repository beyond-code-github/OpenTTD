# Two-stage Slots measured on T1, T2, T3 and 197 (D441, D442)

Measurement test `[.][slotsmeasure]` (planar_domain_encoding.cpp), Release build, head c3e94e4b46 plus the test. One solve at a time, 15 s per solve (set `PLANAR_SLOTS_HUB=T1|T2|T3|197` to run one hub). Shape's variant: flex-stripped, then `PlacePlanarShapeFlex(v, why, true, d443, offers)`; 197 from `tools/junction/corpus/out/psg197`.

Rules (the suite's DEFAULT_RULES as the judge builds them): pairwise + pairwise_all_faces, no cell rows, corridor on, strict_span, node_lead, end_node, cross_piece, bays and curves off, tidy. Stage 1: grid off, no node gaps (no reference yet), `continuous = point, axis, diagonal, param, cross` (so the integer columns are the keep-apart side binaries and the "other" group). Stage 2 (never reached): `final_contract`'s mapping of stage 1's lengths and points onto the contracted variant, integer, grid and node gaps on, sides from stage 1 (a) and, if that failed, sides free (b); judged by `DrawingProgramJudge(DEFAULT_RULES)` and `CheckPlanarDomain`. "Used" = an offer's middle length above 0 in stage 1.

| hub | offers, D443 | flex pairs | stage | status | ms | rows | columns | binaries | offers used | judge | domain |
|---|---|---|---|---|---|---|---|---|---|---|---|
| T1 | 3, on | 123 | 1 | timeout (Error, no incumbent) | 15059 | 90965 | 21960 | 21116 (5279 pairs) | - | - | - |
| T1 | 1, off | 41 | 1 | timeout | 15113 | 14333 | 3596 | 3244 (811 pairs) | - | - | - |
| T2 | 3, on | 345 | 1 | timeout | 15117 | 227700 | 55114 | 52772 (13193 pairs) | - | - | - |
| T2 | 1, off | 99 | 1 | timeout | 15085 | 34472 | 8646 | 7780 (1945 pairs) | - | - | - |
| T3 | 3, on | 711 | 1 | timeout | 15397 | 456500 | 110582 | 105756 (26439 pairs) | - | - | - |
| T3 | 1, off | 169 | 1 | timeout | 16293 | 60442 | 15170 | 13596 (3399 pairs) | - | - | - |
| 197 | 3, on | 1980 | 1 | timeout | 61223 | 1817583 | 436297 | 423188 (105797 pairs) | - | - | - |
| 197 | 1, off | 654 | 1 | timeout | 15185 | 276479 | 68277 | 63124 (15781 pairs) | - | - | - |

Stage 2 was not run for any hub: it needs stage 1's drawing, and no stage 1 solve returned a solution in 15 s. (197 with offers 3 spent 61 s: model build before the solver's 15 s.)

Reading:
- Stage 1 picks nothing, because it does not finish: not even T1 at offers 1 / D443 off (41 pairs, 3244 binaries) returns a feasible point in 15 s, so the offers are not what blows it out.
- Stage 2 is untested; the contraction mapping is in the test and runs only once a stage 1 drawing exists.
- Time goes to the keep-apart sides: binaries are 4 per wall pair (about 85 percent of all columns), and pairs grow with flex (T1 811 to 5279 pairs from offers 1 to 3), so "continuous where it can be" leaves a binary program of the same size as the full one. Stage 1 needs its sides fixed or few (sides by construction, D431) before the offer question can be measured.
