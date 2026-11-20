# RECON: where coop hubs put their sideways steps (study S3)

Scripts and raw output: `psg/study3/` (s3.py, sum3.py, sum3.txt, out.pkl) on OneDrive. Implements wip/SPEC-slots.md §2 input. Follows RECON-corpus-crossings.md (S1) and RECON-corpus-sbend-crossings.md (S1b).

## Method and caveats
- Hub set as S1: census2.tsv, 95 classified (group 1) + 199 unclassified (group 2) = 294 hubs; 277 mesh-large boxes kept apart (context only, own table). Tiles stitched per save via census2.load; boxes are the census2 boxes, walk window = box + 3.
- Track walk: rail pieces (tiles of kind track/signals). A piece is plain when no other piece on its tile shares one of its two edges. A chain is a maximal run of plain pieces joined edge to edge across tiles (signals allowed, grade-crossing diamonds pass straight). A chain ends at a switch (a tile where pieces share an edge, or a neighbour with 2+ pieces on the joining edge), a rail bridge/tunnel head, or an open end. Chains are walked over the whole map (cap 300), a step is kept when any diagonal tile is inside box + 3.
- Step: chain order A(h)+ Q^n A(h), where A = X or Y piece, Q = half piece (Upper, Lower, Left, Right), h = exit heading, n >= 2, same h before and after, sideways offset >= 1. Size = perpendicular offset in tiles between the last A tile before and the first A tile after (n = 2 gives 1, n = 4 gives 2). Each step is counted once (first hub by group order, key = save + first diagonal tile).
- Turns: Q-run between A(h) and A(h') with h' != h (any n, including single curves); counted only. Closed loops (offset 0): none.
- Two extra shapes counted apart, not in the context tables: `jog` = curve, axis run perpendicular to h, curve, back to heading h (X Q Y.. Q X); `dform` = Q-run (>=2), axis run on one of the two diagonal headings, Q-run (>=2) with the same heading pair (a diagonal with a straight piece in it; size = axis-run tiles). The brief's definition of a step is "diagonal pieces only", so jog is not a step here; it is reported because Slots may care.
- Context, first match: `under-span` (any diagonal tile under any rail bridge span of the save); `landing` (both chain ends are bridge/tunnel heads of equal axis, nearest head within 3 tiles of the diagonal part); `port-fan` (a diagonal tile within -6..+3 of the box edge, measured Chebyshev outward from the box, and walking from the step along the chain in some direction leaves box + 3 before the chain ends, so no switch lies between step and port; side = box side with the largest outward distance); `at-switch` (a switch tile ends the chain within 6 tiles of the diagonal part; distance = tiles from nearest diagonal tile to the switch tile, 1 = adjacent); else `mid-run`. Merge versus split is not told apart (no signal logic).
- Bundle: another step in the same hub, same axis, same sideways direction and size, before-line differing by 1, along-axis ranges of the diagonal parts overlapping within 1 tile. Otherwise single.
- Caveats: (1) size has no upper cap; sizes of 9+ (104 of 935 in groups 1+2) are long diagonals, not lane steps, and they feed the "4+" columns (port-fan 4+ is 46% mainly for this reason); the histogram line gives the tail. (2) Port-fan is by geometry only: a long diagonal track crossing the hub edge zone is counted as port-fan. (3) Hub boxes may overlap; first hub wins. (4) Steps whose before or after piece is a switch tile or a head (no A on that side inside the chain) are not seen; so steps that start at a switch (fray) with n curves directly attached are in the turns column or missed. (5) Bridge decks are axis only, so under-span steps are the S1b diag-S cases seen from the track.

## Totals
- Hubs: 294 (group 1 classified 95, group 2 unclassified 199); hubs with at least one step: 156. Mesh-large boxes (context only): 277.
- Group 1: 402 steps in 59 hubs.
- Group 2: 533 steps in 97 hubs.
- Steps (axis-h, diagonal pieces only, axis-h): **935** in groups 1+2; mesh-large 79813.
- Turns (axis h, diagonal, axis h' != h), groups 1+2: 842; closed diagonal loops (offset 0): 0; mesh-large turns 84999.
- curve, axis run, curve (X, Q, Y..., Q, X), groups 1+2: 13 (size 1/2/3/4+: 0/11/1/1; size = sideways offset); mesh-large 1649.
- diagonal form: diagonal, axis run, diagonal d again, groups 1+2: 652 (size 1/2/3/4+: 60/51/59/482; size = axis-run tiles); mesh-large 67633.

- Size histogram, groups 1+2 (9 = 9 or more): 1:293, 2:163, 3:144, 4:81, 5:59, 6:30, 7:28, 8:33, 9:104.

## By context, groups 1+2 (294 hubs)

**Groups 1+2** (935 steps)

| context | count | % | size 1 | size 2 | size 3 | size 4+ | bundle | single | bundle % |
|---|---|---|---|---|---|---|---|---|---|
| under-span | 70 | 7% | 7 (10%) | 7 (10%) | 9 (13%) | 47 (67%) | 21 | 49 | 30% |
| landing | 21 | 2% | 11 (52%) | 8 (38%) | 1 (5%) | 1 (5%) | 16 | 5 | 76% |
| port-fan | 484 | 52% | 122 (25%) | 57 (12%) | 83 (17%) | 222 (46%) | 165 | 319 | 34% |
| at-switch | 178 | 19% | 92 (52%) | 43 (24%) | 14 (8%) | 29 (16%) | 22 | 156 | 12% |
| mid-run | 182 | 19% | 61 (34%) | 48 (26%) | 37 (20%) | 36 (20%) | 53 | 129 | 29% |
| all | 935 | 100% | 293 (31%) | 163 (17%) | 144 (15%) | 335 (36%) | 277 | 658 | 30% |

## By context, mesh-large (context only)

**Mesh-large** (79813 steps)

| context | count | % | size 1 | size 2 | size 3 | size 4+ | bundle | single | bundle % |
|---|---|---|---|---|---|---|---|---|---|
| under-span | 6164 | 8% | 1078 (17%) | 1181 (19%) | 1077 (17%) | 2828 (46%) | 1677 | 4487 | 27% |
| landing | 1485 | 2% | 880 (59%) | 296 (20%) | 147 (10%) | 162 (11%) | 870 | 615 | 59% |
| port-fan | 586 | 1% | 96 (16%) | 91 (16%) | 78 (13%) | 321 (55%) | 309 | 277 | 53% |
| at-switch | 28941 | 36% | 13429 (46%) | 6862 (24%) | 3699 (13%) | 4951 (17%) | 5175 | 23766 | 18% |
| mid-run | 42637 | 53% | 14091 (33%) | 9121 (21%) | 6272 (15%) | 13153 (31%) | 21577 | 21060 | 51% |
| all | 79813 | 100% | 29574 (37%) | 17551 (22%) | 11273 (14%) | 21415 (27%) | 29608 | 50205 | 37% |

## at-switch distance (tiles from nearest diagonal tile to the switch tile, 1 = adjacent), groups 1+2

| distance | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|---|
| steps | 0 | 11 | 22 | 68 | 45 | 32 |

## port-fan steps per port side (box side of the diagonal part), groups 1+2

| W (NE) | E (SW) | N (NW) | S (SE) |
|---|---|---|---|
| 131 | 141 | 91 | 121 |

Port-fan size by side:

| side | 1 | 2 | 3 | 4+ |
|---|---|---|---|---|
| W | 31 | 19 | 29 | 52 |
| E | 44 | 13 | 18 | 66 |
| N | 24 | 10 | 20 | 37 |
| S | 23 | 15 | 16 | 67 |

## Example steps (save, hub id, diagonal-part first tile x,y; groups 1+2, distinct hubs, size 4 or less)

- under-span: PublicServerGame_136_Final psg136_12 (45,225) size 1 n=2 Y bundle; PublicServerGame_134_Final psg134_9 (33,542) size 3 n=6 Y single; PublicServerGame_305_Final psg305_4 (451,82) size 4 n=8 Y bundle
- landing: PublicServerGame_09_Final psg09_13 (199,590) size 1 n=2 X single; PublicServerGame_167_Final psg167_15 (374,117) size 1 n=2 X single; PublicServerGame_314_Final psg314_5 (202,75) size 2 n=4 Y bundle
- port-fan: PublicServerGame_09_Final psg09_13 (179,588) size 1 n=2 X single; PublicServerGame_314_Final psg314_5 (186,77) size 1 n=2 X bundle; PublicServerGame_35_Final psg35_38 (861,280) size 4 n=8 X single
- at-switch: PublicServerGame_136_Final psg136_10 (507,165) size 1 n=2 X bundle; PublicServerGame_290_Final psg290_14 (34,268) size 1 n=2 Y single; PSG_173 psg173_50 (93,598) size 4 n=8 Y single
- mid-run: PublicServerGame_09_Final psg09_2 (116,417) size 1 n=2 X single; PSG_173 psg173_65 (695,857) size 1 n=2 Y single; PublicServerGame_35_Final psg35_108 (363,743) size 3 n=6 X single
## What this suggests for Slots (the agent's reading, not a finding)
In the hub set, 935 steps in 156 of 294 hubs; half are at ports (52%), and the lane-sized ones (size 1 to 3) are 63% overall. Steps right next to a switch (19%) and landing steps (2%) are overwhelmingly size 1 (52% each) and at-switch steps are mostly single tracks (12% bundle) at distance 4 to 6, never adjacent (0 at distance 1), while landing steps are bundles 76% of the time. Mid-run steps (19%) are mostly small (size 1 to 2: 60%) and single. Under-span steps are mostly long diagonals (67% size 4+), so the size-1 S-bend under a bridge of S1b is the minority here. Port-fan steps are the largest group but are inflated by long diagonals; read their size 1 to 3 part (54%).
