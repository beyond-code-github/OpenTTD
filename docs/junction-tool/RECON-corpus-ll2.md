# RECON corpus LL2: real LL_RR x LL_RR junctions in the PSG saves

Untracked. `P = /mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg`. Outputs in `P/ll2/` (`candidates.jsonl` = the L30/TOL4 run, `cand_L30_t3/t4.jsonl`, `cand_L15_t3.jsonl`, `top.json`, `bbh_stations.json`, `pic_<cid>.txt`, `tiles_<cid>.json`, scripts in `P/ll2/scripts/`: `extract.py`, `cluster.py`, `pictures.py`, `run_pics.py`, `bbh.py`). Save numbers are the PSG numbers (psg181 = `PublicServerGame_181_Final`); coordinates are the save's own tile x,y (x grows to screen SW, y to SE), so the box can be found with the in-game tile coordinates.

## Result in one line
No clean LL_RR x LL_RR exists in the C10 hub data, even as a cluster of hubs: exact 2-in/2-out on all four sides of a cluster never occurs (0 of 12642 clusters); with up to 3 per direction per side, 2 clusters; with up to 4, 15. The nearest real thing is **psg181 box x64..110 y148..192** (see Best pick). The coop BBH screenshots (psg186, 197 and others) are not LL_RR at all: their lines carry 7 to 9 tracks per direction side by side (details below).

## Method as run (and deviations)
1. **Input.** G1b's C10 `hubs.jsonl` (88133 hubs; 178 saves) reduced per save (`extract.py`: nodes, cut edges with paths, lanes, moves). Single-node components are not in that file, so a crossover node sitting alone between two hubs is invisible: INFERRED loss, would split some interchanges.
2. **Lines** (deviation): there is no separate corridor finder. A line end is read from the hub lanes (G1's ports: lane side and in/out direction) that leave a cluster.
3. **Clusters.** Single-link merge of hubs over cut edges between two hubs of length <= L, taken in ascending length, rejecting a merge whose bounding box would exceed 60 tiles on a side (cap, my knob; unbounded merging chains whole networks). Counts of clusters with 2+ hubs: **L=15: 14193, L=30: 12642** (L=30 is lower only because the cap blocks merges; clusters that reach the cap stop growing).
4. **Candidate test** (deviation from "bundles"): per side (W E N S, G1's side) of the cluster, count the lanes that leave it as in / out / unknown direction / terminal. A side is an LL_RR end when it has between 2 and TOL lanes each way (TOL=2 exact, 3, 4). Candidate: 4 such sides, on all four sides ("full") or 3 (T variant, two of them opposite). A first version grouped lanes into bundles with a 10-tile gap and required exactly 4 lanes; it found 1 cluster, so I dropped it. A side total also counts a line that merely passes by the cluster, so the movement test below is what shows the lines meet.
5. **Movements.** A directed graph over hub lanes: G1's per-hub `moves`, plus a link from an out lane of one hub to the in lane on the same cut edge in its neighbour. For each ordered pair of different sides among the chosen ends it asks whether any in lane of the first reaches any out lane of the second. Turning movements = pairs on different axes: 8 for full, 6 for T. Lane to edge mapping is exact (lane vertex = edge end vertex); direction comes from G1's lane direction, so signal-less lanes are "unknown" and count as deviation.
6. **Rank**: share of turning movements present, then deviation (lanes beyond 2/2 on the chosen sides, unknown or terminal lanes, lanes on the other sides), then area. Axis fraction (hubs flagged axis in G1) and stations in the box are listed. No cluster has all eight turning movements; the best has six.
7. **Pictures.** `P/ll2/pic_<cid>.txt` (cluster box plus 2 tiles margin; legend in the header; arrows = signalled travel direction on straight single tiles, `#` several pieces, `B`/`T` heads, `=`/`~` bridge/tunnel span) and `tiles_<cid>.json` (every rail, station, bridge/tunnel tile in the box: tracks, signals, head direction and far end). I made them for the top 10 full and the top 5 T.

## Counts per step
| step | L=15 | L=30 |
|---|---|---|
| hubs (C10) | 88133 | 88133 |
| clusters of 2+ hubs (cap 60) | 14193 | 12642 |
| TOL=2 (exact 2/2 ends): 3+ ends | 1 (T) | 1 (T) |
| TOL=3: 3 ends / full 4 ends | 27 / 0 | 36 / 2 |
| TOL=4: 3 ends / full 4 ends | 77 / 3 | 123 / 15 |
| of those, 2+ turning movements: T / full (TOL 4) | 36 / 1 | 52 / 12 |
| all turning movements (6 or 8) present | 0 | 0 |
The one TOL=2 T cluster (psg248 box x279..309 y144..173, 12 hubs) has W, N, S ends of 2/2 and 0 turning movements: three lines running past each other, not a junction.

## Top 10 (full 4 ends, TOL 4, L=30; ends listed as side in/out lanes; u = unknown direction, t = terminal)
Movement letters: first = side the train enters the cluster from, second = side it leaves by (W = tile edge NE, E = SW, N = NW, S = SE as in G1).
| # | save | tile box (x0..x1, y0..y1), W x H | turning movements (of 8) | ends | description (INFERRED from lane counts and the picture, not checked in game) |
|---|---|---|---|---|---|
| 1 | psg181 | x64..110, y148..192, 47x45 | 6: EN ES NE NW WN WS | W2/2 E3/3 N2/4 S2/2+1t | LL_RR on W and S, one extra lane each way on E, two extra N outs; 19 hubs in a staircase of ladders and bridges; the S line makes no turn to W or E (SE and SW absent) |
| 2 | psg248 | x270..307, y249..283, 38x35 | 6: ES NE SE SW WN WS | W4/3 E3/3 N3/2 S3/4 | three lanes per direction on every end (LLL_RRR-ish); 20 hubs; many turns, mostly diagonal ladders, 4 W-end lanes in |
| 3 | psg313 | x900..930, y108..147, 31x40 | 5: ES NE SE SW WS | W2/2 E3/2 N2/2+1t S3/3 | compact, near-exact LL_RR on W and N; 12 hubs; a bridge block at the north edge (rows 107..114) carries the N line over the W/E pair; ladders at y120..126 and y127..131 do the turns |
| 4 | psg243 | x128..177, y36..88, 50x53 | 5: ES NW SW WN WS | W4/4 E3/3 N4/2 S3/3 | 18 hubs, wider lines (LLLL_RRRR on W), axis fraction 0.61 |
| 5 | psg301 | x111..140, y119..168, 30x50 | 4: SE SW WN WS | W2/2 E2/2+1t N3/3 S4/2 | W and E are exact LL_RR, N is LLL_RRR, S has 4 in 2 out; a station or depot tile sits in the box |
| 6 | psg206 | x308..345, y161..183, 38x23 | 4: NE SE SW WN | W4/4 E2/4 N2/2 S3/4 | wide, axis 0.73 (most axis-aligned of the set) |
| 7 | psg142 | x110..152, y67..113, 43x47 | 4: EN NE SW WN | W4/2+1u E3/3 N4/4+1u S2/2 | terminal ports present (2) |
| 8 | psg260 | x178..213, y243..283, 36x41 | 3: EN SE WN | W3/2 E4/2 N2/4 S2/3 | 15 hubs |
| 9 | psg219 | x80..109, y390..439, 30x50 | 2: NW WS | W4/2+4t E4/2+1u1t N2/2 S3/2 | station throats (5 terminal lanes), not a main-line interchange |
| 10 | psg250 | x286..320, y251..297, 35x47 | 2: EN NE | W4/2+5t E4/4 N3/2 S2/2 | 8 terminal lanes: station throats |

Table note: row 1 turns are W to N, W to S, E to N, N to W, N to E, E to S. T variant top 5 (one line passes, one ends): psg273 box x268..320 y173..220 (4 of 6 turns, dev 13); psg100 x101..160 y8..52 (4 of 6, terminals); psg206 x87..119 y381..415 (3 of 6, W end absent, E3/4 N2/4 S2/2, dev 5); psg273 x108..150 y4..32 (3 of 6); psg262 x441..474 y148..203 (3 of 6, dev 8).

## Best three, layout in coop terms (INFERRED from the pictures, `pic_*.txt`)
1. **psg181 x64..110 y148..192.** A diagonal chain of ladders with bridge crossings, not a compact 4-way: the S line comes up a pair of single-track columns (x=68..73 direction down), the W line runs along y=148..160 as 2 and 2. Turns are done on ladders with prio merges at the ladder ends (`#` rows at y150..158). Crossings are bridged over the staircase. W to N, W to S, N to W and N to E turns exist; nothing turns from S. Reading of the sides: W 2/2, S 2/2, so this is a LL_RR x LL_RR with two extra lanes, split into one-way hubs per direction (as RECON-corpus-oneway says is usual).
2. **psg313 x900..930 y108..147.** The most compact; N (x~96..100 col in picture, bridged block at y107..114 spanning 8 tiles of x) crosses the W/E pair on a bridge block; W-in turns S via a ladder at y120..131; E has an extra lane each way (LLL_RRR end), probably a station feeder joined here (one terminal lane on N).
3. **psg248 x270..307 y249..283.** Three lanes per direction on every end: the corridor of 6 tracks, so closer to an LLL_RRR x LLL_RRR; diagonal staircases (axis fraction 0.25) with bridges.

## Leads from the coop wiki (BBH screenshots)
- Signs are not in the exported tile data (only station names), so a BBH cannot be found by its sign. `bbh.py` found these named station tiles: PSG 173 `WP BBH12-BBH17` x604..606 y695..708, `penbbh20` x899 y513; PSG 9 `BBH 01 south` x217..219 y334; PSG 184 `BBH01 Override S` x155..163 y159..173; PSG 260 `BBH01` x257 y217 and `BBH02` x257 y221; PSG 268 `BBH Woodsupply` x179..183 y202..204. None of those are the screenshot hubs (those PSG 186/197/102/115/173/219/227/248 saves have no BBH-named station in the box that I could match), so none is confirmed.
- PSG 186 "The Brainmelter" (screenshot read by eye: a 4-way of dense multi-lane lines, a lake to the NW). The hotspot export `psg186_1.json` box x264..311 y24..71 (top score) is INFERRED to hold it: tracks crossing its border: W 15, E 11, N 18, S 13 (plain tile links plus bridge/tunnel spans, no direction split; `pictures.py border`). That is about 7 to 9 tracks per direction side by side: far beyond LL_RR. Hubs in it (G1b) are fragments of 3 to 20 nodes; the best cluster test finds no 2-in/2-out end. Not an LL_RR x LL_RR.
- PSG 197 `BBH01` (screenshot: lines of 2 to 3 tracks per direction, interchange sprawled over a staircase of bridges): hotspot boxes x144..191 y144..191 border W 15 E 12 N 7 S 6; x72..119 y240..287 W 5 E 8 N 3 S 11; x96..143 y240..287 W 21 E 3 N 0 S 12. The best cluster in psg197 is c41 (T, x106..161 y362..414, 3 of 6 turns, dev 13) and c8/c165; none is exact. INFERRED: this BBH is a multi-line interchange of about LL_RR to LLL_RRR lines whose turns are scattered over 50x50 tiles; my hub clustering does not recover it.
- PSG 102, 115, 173, 219, 227, 248, 206: no hotspot exports; not located. Cluster candidates in those saves (TOL 8 scan, L=30): psg219 c420 x300..340 y50..91 (full, 3 turns, wide), psg227 c699 x246..301 y201..238 (T, 4 turns), psg248 c328 (above, row 2), psg206 c189 (row 6). All are 3 to 9 lanes per side, none LL_RR.

## Pointers (names only)
- Junctionary mirror (`junctionary-mirror/index.md`): "6-tuple Branch-Merge Junction (4-way 12-track)", "4-Way Triple Axial" and "4-Way Triple Axial 2", "Double Cloverleaf", "Deepblue2k8 4-way Junction", "Basic 4-Way Junction": designs for LL_RR-class 4-ways; no real LL_RR x LL_RR page I could confirm by name.
- Coop files (`coop/files/`): `JunctionaryBBH_4way.png`, `All_BBHs.png`, `JunctionaryBBHs_other.png`, `BBH_building_1..5.PNG`, `LL_RR_SLH.png`, `Crossjoin.PNG`, `Crossjoin_full.PNG`, `4way.PNG`, `Psg186_LLLLmerger01.png`, `206_BBH03merger.png`, `219_bbh04.png`, `800px-227_bbh03.png`, `Psg248_bbhfurball.png`, `Psg102/115/173/186/197_bbh*.png`. I looked at only Psg186_bbh01.png and Psg197_bbh01.png.

## Best pick
**psg181, x64..110 y148..192** (47x45). Reasons: 6 of 8 turning movements (the most of any cluster; none reaches 8), two ends exactly 2/2 (W, S) and the other two within one lane per direction (E 3/3, N 2/4), 19 hubs in one box with a single terminal lane, so it is the least contaminated by station throats; psg313 (31x40, 5 turns, W 2/2 and N 2/2, deviation 4) is the more compact runner-up and has the lowest deviation of the top set. Caveats: axis fraction 0.58 (partly diagonal staircases), and lines pass by the cluster that the side counts include. Treat it as the best available lead to inspect in game, not as a clean LL_RR^2: a clean one is absent from this corpus under every cluster setting I ran (INFERRED, bounded by the invisible single-node components and the 60-tile cap).
