# RECON corpus anatomy: how real balanced all-way hubs are built (D277)

Untracked. OD = `/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad`; pictures, tile boxes, scan outputs and scripts in `OD/psg/anatomy/` (scripts in `anatomy/scripts/`). Wiki text extracted with `wtxt.py` (html.parser) from `OD/coop-site/site/`. All Python ran with `python3 -I`. INFERRED marks what I reasoned to and did not read or measure.

## Summary
1. **Wiki.** Coop's own words: a BBH "must be Balanced - ie: each lane entering the BBH should be able to choose any lane leaving the BBH" (Backbone_Hub), and "Balancing, Balancer or Balanced traffic is probably the most used, and the worst term" (Merging_Tracks, V453000): the coop word is merger with *choice*. Skeleton of a 3-way BBH: "one ML crossing, one crossover, 3 exits and 3 joins"; exits "don't require balancing", joins "require balancing"; prios are added last and "you can hack in priorities in any place".
2. **The wiki's teaching BBH puts joins and exits on the arms as separate blocks** around a centre of ML crossing plus crossover (`OD/coop/files/BBH_building_5.PNG`, labels EXIT 1-3, JOIN 1-3). That is D277's "added on to the ends".
3. **psg173_14 and psg327_12 are not balanced all-way hubs.** Their legs are bare 2-lane lines for 40 to 118 tiles outward (no balancer, merger or shifter); the first things met are other hubs, crossings and stations. The core is lane-keep (the review's 12 paths). Both are lines-of-2 junctions, not BBHs in the wiki sense, so there is no contradiction between "BBH must be balanced" and "real LL² is keep".
4. **Real multi-lane BBHs are one woven box, not leg blocks.** The one dense switch cluster at the hub, with plain bundles outside it: PSG197 tunnel BBH 46 x 50 (33 splits, 32 merges, 0 bridge heads, 100 tunnel heads), PSG186 Brainmelter 60 x 46 (57 S, 45 M, 126 bridge heads, 56 tunnel heads), psg327_12 32 x 36, psg173_14 32 x 23. Plain 3-7 lane bundles run 40+ tiles in and out. Where the balancing sits inside the box could not be separated automatically (Q3).
5. **Block sizes measured on the corpus** (compact all-to-all blocks, whole-map scan of 178 saves): 2 to 2 is common and tiny (1-3 tiles), 3 to 3 uncommon (73), 4 to 4 rare (3), 5 to 5 once (6 x 5 tiles, a crossover ladder); wider ones are hubs or rings (8 to 8 in 8 x 8, PSG231).
6. **V453000 and terrain.** His hubs are tunnel-based (PSG197 "Tunnel only", Furball "a flood of tunnels", merges "based on splits of the tunnels") and follow the slope (Furball: portal rows on terrace walls). ABR06 states the principle: "use as long tunnels as possible, and stuff there as much things as possible", "3 levels of track atop of each other are ... 200%".
7. **Composition rule, as I read it (INFERRED for the middle):** core = ML crossing plus crossover at the centre; per arm one exit (split, no balancing) and one join (merger with choice, prio last); joins sit on the arm next to the core, 10-20 tiles (from the wiki picture); real big hubs merge all of it into one woven box. Our generator threads the join through the core box; coop builds it per arm.
8. **Not delivered:** per-block lane tables for the three big hubs (direction inference fails inside woven boxes: PSG186 box shows 64 topology contradictions and 255 of 532 signal votes in conflict), a located Furball, a located PSG219 BBH, and any tile-measured length for a coop "join" block in a BBH (only the wiki screenshot shows them).

## Q1. Wiki anatomy

Sources: pages `Backbone_Hub` (= `BBH`), `Pivot_BBH`, `Merging_Tracks` (= Merger, Mergers, Balancer, Balancing, Mixer, Joiner redirect to it), `Prio`, `Shift_Mainline`, `SLH` (= `Sideline_Hub`), `Hub`, `Unbalancers`, `Advanced_joiners`, `Implicit_Load_Balancing`, `Junctionary_-_All_BBHs`, `Junctionary_-_Mergers_-_All`; blog `OD/abr/06-advanced-building-revue-06-hubs.txt` (ABR06). The wiki gives no tile lengths for blocks; "all of the things are constructed for trains 3 tiles long" (Merging_Tracks). The only length is the standard prio: "when a train on the mainline is within 9 tiles of the merging point" (Prio).

| block | what it does | lanes in to out | size | order and rules (quoted) |
|---|---|---|---|---|
| ML crossing | the main lines cross each other; centre of a 3-way | 2 lines cross | not stated | "start with the ML crossing ... a common mistake to start with an exit or a join" (Backbone_Hub) |
| crossover | where the joining mainlines cross, so traffic can go the other direction | n/a | not stated | "you will [have] one or more of these in every BBH" (Backbone_Hub); the "sweet-spot" where trains turn left, begin there (ABR06) |
| exit (split) | a line leaves the ML | 1 to 2 lanes | not stated | "exits don't require balancing" (Backbone_Hub); "just use anything to get those lines away" (ABR06) |
| join (merger) | lines of one direction reduce to the exit line; the choice happens here | N to M lanes | not stated | "joins ... require balancing" (Backbone_Hub); "full-balanced non-blocking all-to-all" |
| all-to-all merger | every input may take every output | 4 to 2, X to Y | large | "usually resulting in a complete rebuild" when expanded (Merging_Tracks) |
| inner mix | the inner lines choose from the outer lines, outer lines are left alone | 4 to 2 | smaller than all-to-all | "less rail crossings in total" (Merging_Tracks); works only by luck of where trains come from (ABR06) |
| 1 to 2 / joiner (SLH style) | one input chooses between two lines, with prio checks | 1 to 2 | small | "Joiner ... one direction gets priority" (Merging_Tracks) |
| prio | a check, not a "give way": a parallel prio track holds sideline trains while an ML train is near | n/a | 9 tiles standard | "Standard prios: can be made infinitely long without causing any delays" (Prio); full, half (two equal lines) or none (ABR06) |
| shifter (SML) | trains shift to the neighbouring lane to free the merging lane | lane to lane | not stated | "shifting opportunities prior to every SLH" (Shift_Mainline); in a hub: shift so "ignore those tracks in further balancing, making the mergers smaller" (ABR06, MSH01) |
| SLH | joins a sideline to a ML with prioritized mergers | sideline N to ML M | not stated | "prioritized mergers to avoid slowing down mainline traffic" (SLH) |
| pivot BBH | traffic rotates around a pivot; each input crosses over, then is crossed over by another set; the split during the first crossing gives both outputs | LL_RR 3-way | compact | "reduce the number of bridges and crossings" (Pivot_BBH) |
| waiting bay | a safe stop for every choice | n/a | one train long | "short ... but long enough to accommodate a train"; first split "technically starts" the merger and should offer all choices (Merging_Tracks) |
| 2 to 2 and 4 to 4 load balancer | station-entry balancers | 2/4 to 2/4 | picture: 4 to 4 about 14 x 6 tiles (`OD/coop/files/4_to_4_load-balancing.png`, MEASURED by eye) | crossovers at both ends and in the middle, tunnels for the cross lanes |

Sequence of merges onto one line (Merging_Tracks): one spot "best"; sequence with prio starves downstream joiners; without prio favours the last; small prio middle way; tree structure because "one spot can only take three rails maximum".

Order along a leg, as the wiki gives it: inbound arm: (far) plain line, then the join on the arm, then the crossing and crossover in the centre; outbound arm: centre, then the exit. Prios last. Bridges/tunnels: doubled bridges give "evil mode" risk; "not bridging the prioritized line overcomes this problem entirely" (Merging_Tracks). Tunnels: "tunnels are harder to place, more space-demanding, but are able to go under for example a power plant" (ABR06); ABR06 built a whole hub tunnels-only on purpose.

Junctionary BBH list (`Junctionary_-_All_BBHs`, creators): LLRR "Simple" PSG227 (mfb); LLLRRR "Bridge split" PSG188 (Vitus), "Tunnel only" PSG197 (V453000), Furball PSG248 "a flood of tunnels providing all-to-all choices" (V453000); LLLLRRRR Brainmelter PSG186 "an all to all merge from each side" (V453000), PSG219 (mfb, V453000), Snake Nest PSG227 (V453000); massive PSG115 "Little balancing" (Mark), PSG219 "Balanced"; 4-way PSG173 "Swapped over" (Mark), PSG268 "all directions are 7L_7R" (V453000, mfb).

## Q2. Real anatomy of psg173_14 and psg327_12

Method: `scripts/legs.py` (piece graph of `review-g4/py/c2.py`, directions from each piece's own signal votes with chain propagation, `LOCAL=2`; Dijkstra along track, a bridge or tunnel span counts its length). Per leg: pieces within 120 tiles, split/merge tiles clustered at gap 12, in/out = boundary pieces, reach = which entry reaches which exit. Width is constant (10 pieces per 5 tiles = 2 lanes) except where listed. Pictures: `anatomy/psg173_14_hub.txt`, `psg327_12_hub.txt`, and `<hub>_<leg>_b<n>.txt` per block.

**psg173_14** (PSG_173, box 73,92-130,133; woven core = one cluster x84-115 y98-120, 32 x 23, S7 M12, 36 bridge heads, 4 tunnel heads, 12 in 8 out lanes at its edge). Bridges-and-tunnels mixed, mostly bridges.

| leg (side, lanes) | blocks met outward, in order |
|---|---|
| E in (2) | none in 120 tiles |
| E out (2) | d41-52 (x170-183 y101-103): 2 lanes in, 3 out, partial, 2 tunnel pairs: a small crossing with a perpendicular line (picture `psg173_14_Eout_b0.txt`) |
| S in (2) | d86-99 (x99-102 y218-232): 3 in 2 out keep, 4 tunnel heads: crossing with an E-W 2-lane line (`psg173_14_Sin_b0.txt`) |
| S out (2) | none; the trace stops at d85 (direction lost, INFERRED data end or unsignalled stretch) |
| W in (2) | none; direction lost at d49 at (89,91), where the line turns north (INFERRED continues) |
| W out (2) | d100-119 (x72-90 y29-46): 2 lanes into 18: a station complex |

**psg327_12** (PublicServerGame_327_Final, box 133,403-185,449; woven core = x143-174 y408-443, 32 x 36, S15 M14, 36 bridge heads, 12 tunnel heads, 7 in 9 out).

| leg | blocks met outward, in order |
|---|---|
| E in (2) | none in 120 tiles |
| E out (2) | d118 (x233 y358-365): one split, 2 to 3 |
| N in (2) | d58-88: 12 in, 2 out, bridges 2, tunnel 1 (x140-159 y326-348): a station throat; first station at d83 |
| N out (2) | d59-61 split 2 to 4 (x143-145); d74-82 (x139-155 y326-339): 4 to 8, 4 bridge heads, station at d84 |
| W in (2) | d68-86 (x53-73 y431-446): 4 in 2 out partial, 4 bridge 2 tunnel; d108-111 (x33-67 y412-441): 6 in 4 out keep |
| W out (2) | d44-57 (x75-90 y445-450): 2 to 4 partial, 2 bridges |

**Is the hub, core plus leg blocks, balanced all-way?** No, for both. Evidence: (a) the review's census (`RECON-corpus-census2.md`: "missing" 2/4 per movement) and REVIEW-G4 Q1 trace: 12 paths, every in-lane reaches exactly one lane per destination; (b) the legs add nothing: no leg block within 40 tiles (173) or 44 tiles (327), and the blocks beyond reach 2 or more lanes only at crossings and stations, never as a balancer between this hub and its neighbours. Balancing does not happen anywhere along these two hubs. So they are what the coop call a junction of two LL lines with lane-for-lane joins, not a Backbone Hub; the census found no real LL² that is any-to-any (REVIEW-G4 Q1).

## Q3. More balanced hubs

Real multi-lane hubs located in the saves. Identification: signs are not in the map export, so each is matched to the Junctionary entry by the properties it states (INFERRED unless noted).

| hub | where | match to coop's entry | footprint of the woven box | bridges / tunnels | picture |
|---|---|---|---|---|---|
| PSG197 BBH01 (V453000, "Tunnel only", two LLLRRR and one LLRR) | PSG_197, x78-123 y146-195 (analysis box 77,148-126,190) | 3 arms, tunnels only, zero bridge heads in the cluster, lanes N 2 in/3 out, E 4/3, S 3/6 at the box edge (edge counts include extra crossings; INFERRED LL LLL LLL arms) | 46 x 50, S33 M32 | 0 bridge heads, 100 tunnel heads | `psg197_bbh01_hub.txt`, leg blocks `psg197_bbh01_*_b*.txt`; screenshot `Psg197_bbh01.png` viewed (tunnel portals on a hillside, company HQ in the middle) |
| PSG186 BBH01 "The Brainmelter" (V453000, LLLLRRRR) | PSG_186, x165-224 y232-277 (analysis box 163,243-228,274). Located by position: the only 60 x 46 woven cluster in the hotspot export (`psg186_hotspots.txt` box 168,240-215,287: 114 bridge heads, no station) | screenshot shows a bridge-heavy hub in a lake valley; four 4-7 lane bundles | 60 x 46, S57 M45 | 126 bridge heads, 56 tunnel heads | `psg186_brainmelter_hub.txt` |
| psg173_14, psg327_12 | Q2 | not BBHs | 32 x 23, 32 x 36 | see Q2 | |
| psg314_15 (census2 clean, LLL_RRR/LLLL_RRRR x LLLL_RRRR) | PublicServerGame_314_Final box 399,389-478,467 | not in the wiki list | one cluster 58 x 69: 56 splits, 41 merges, 82 bridge heads, 24 tunnel heads | | `psg314_15_*` |

Core reach (MEASURED, lower bound, `legs.py` header line; direction partly guessed): psg314_15 pairs reached over in x out: W>N 10/12, W>S 13/16, N>W 9/12, S>W 6/16, N>S 3/12, S>N 6/12 (branch movements nearly all-to-all, through lanes lane-for-lane); PSG197 BBH01: S>E 8/9, S>N 6/9, E>N 6/12, N>E 3/6, N>S 3/12, E>S 5/24 (direction errors likely lower these). So at least the branch joins of psg314_15 and the S arm of PSG197 are close to any-to-any inside one woven box.

What could not be done: (1) PSG248 Furball: not located (no sign or town in the export; the tunnel-heavy bins of PSG_248 at 24-tile bins (408,168), (288,240), (192,144), (144,240) are candidates, none checked); (2) PSG219 bbh03/bbh04: a 29 x 45 cluster (S56 M51, 16 bridge heads, 13 tunnel heads) at x1-29 y54-98 in PSG_219 is the only huge cluster there, not confirmed; (3) PSG188 and PSG115 saves are not in the corpus. `psg167_20` (census "LLLL_RRRR") is a bridge viaduct of 112 bridge heads with reach 0 to some sides: not a hub of that type.

**Inside a woven box** (PSG197, gap-1 clusters, `scripts/inner.py`): 49 small clusters of 2 x 2 to 5 x 4 tiles with 2-8 lanes in and out, each one split or merge plus 1-5 tunnel heads, in rows. So the building element of V453000's tunnel hub is a small tunnel-pair crossing repeated, not one large merger block (INFERRED from sizes; I did not trace lane by lane).

**Compact all-to-all blocks across the corpus** (`scan_all.txt`: gap 2, 54070 clusters; `scan_all_gap6.txt`: gap 6; reach by directed pieces, boundary pieces of the bounding box +1, so adjacent lines inflate in/out and reach is a lower bound):

| lanes in = out | all-to-all clusters (gap 2) | typical footprint |
|---|---|---|
| 2 to 2 | 1216 | 1-3 tiles (1 split, 1 merge); 3 x 3 in 69 |
| 3 to 3 | 73 | 1 x 2 to 7 x 2 for 1 split 1 merge; 3-5 tiles with 2-3 each |
| 4 to 4 | 3 (gap 2); 9 more with 4+ lanes at gap 6 | 5 x 3, 7 x 2; 10 x 7 (PSG287), 11 x 8 (PSG239), 8 x 42 (PSG241, 3 bridges) |
| 5 to 5 | 1 | 6 x 5, PSG137 x405-410 y100-104: five parallel lines with a ladder of crossover switches (`psg137_5x5_ladder.txt`) |
| 7 to 7 | 1 | 28 x 9, 7 bridges (PSG166 x126-153 y458-466) |
| 8 to 8 | 1 | 8 x 8 (PSG231 x130-137 y75-82): a ring where 4+4 lanes meet (`psg231_8x8.txt`) |
| 6 to 4 | 1 | 7 x 48 in 15 tunnels (PSG145 x241-247 y288-335, `psg145_6to4_tunnel_merger.txt`): a long tunnel merger |

## Q4. The pattern

| block | purpose | lanes in to out | length / width (tiles) | where on a leg | seen |
|---|---|---|---|---|---|
| plain line | carries lanes between hubs | n to n | 40 to 118 tiles bare outside every hub measured (173_14, 327_12, 314_15 N and S legs, PSG186 N, PSG197 legs) | whole leg | all |
| exit | one lane leaves the ML | 1 to 2 | 1 tile (split) in the corpus scans | next to the crossing | every hub |
| join (merger with choice, wiki) | gives all lanes the choice | N to M | not measured on a BBH; compact all-to-all in corpus: 2 to 2 1-3 tiles, 3 to 3 up to 7, 4 to 4 5-11 long | on the arm, next to the core | wiki; not separable in real woven boxes |
| lane-keep join | lane k to lane k with plain merges | n to n | 1 tile per lane | in the core | psg173_14, psg327_12 |
| crossover ladder | any lane to any lane by chained crossovers | 5 to 5 | 6 x 5 | on a line | once (PSG137) |
| ring | roundabout for crossing lines | 4+4 to 4+4 | 8 x 8 | at the crossing | once (PSG231) |
| tunnel merger | long merge inside tunnels | 6 to 4 | 7 x 48 | on a leg | once (PSG145) |
| tunnel-pair element | the V453000 hub's repeated unit | 2-8 to 2-8 | 2 x 2 to 5 x 4 | inside the box | PSG197 (49 elements) |
| prio | a check on merges | | 9 tiles standard | per merge, last | wiki |
| shifter | frees a lane for a sideline | lane to lane | not stated | before an SLH | wiki |

Composition rule: core type (ML crossing plus crossover) plus per arm one exit and one join; joins carry the any-to-any choice; prios last. Real hubs measured: either lane-keep cores of 2-lane lines with no joins (psg173_14, psg327_12), or one woven box of 46-60 x 46-50 tiles with plain bundles outside it (PSG197, PSG186, psg314_15). Nothing in the corpus shows a join as a free-standing block of the arm, 10-20 tiles from a core, except the wiki screenshot; INFERRED from the sizes of the woven boxes that the arm joins are inside them.

Does not fit the pattern: psg231's ring and psg145's 48-tile tunnel merger (neither is a hub arm block); the Pivot BBH (traffic rotates around a centre, no ML crossing); PSG173 bbh01 "Swapped over" (hand swap in the centre, ABR06); V453000's hubs, where tunnels are a merger technique ("The technique of the merges is based on splits of the tunnels", Junctionary).

## Tunnels versus bridges, and terrain (as asked)
- Tunnel-only: PSG197 BBH01 (0 bridge heads, 100 tunnel heads; wiki "Tunnel only"), Furball (wiki). Bridge-and-tunnel: Brainmelter cluster 126 bridge heads, 56 tunnel heads; psg327_12 36/12; psg173_14 36/4; psg314_15 82/24.
- Both of V453000's tunnel hubs lie on slopes (screenshots: 197 on a hillside with a lake edge, Furball on terraces with portals on the terrace walls); the tunnels cross under the plain bundles that carry on past the box. Terrain-following is by tunnel depth rather than by bridge height. ABR06: "tunnels still can go under each other if you have them in different levels".
- The balancing itself is done with tunnels only in those two hubs; I could not separate crossing tunnels from balancing tunnels (no lane trace inside the woven boxes).

## Method limits
- Directions: signal votes per piece plus chain propagation (`LOCAL=2`, falls back to the review's parity system). Woven boxes (PSG186 box: 64 topology contradictions, 255/532 votes in conflict) are not resolved; reach figures there are lower bounds.
- Block detection is by switch-tile clustering; adjacent unrelated lines add to in/out counts.
- No screenshot of PSG186/197 was matched pixel by pixel to tiles; the matches rest on arm count, tunnel-only property and position.

## Engineer's note (2026-11-14): Mark's PSG 173 BBH17
Mark's BBH17 ("Swapped over", signs "BBH17 / Mark", "gaps and weirdness on purpose (probably)", "V453000 likes") is an LL_RR line with a 10-tile gap between the directions (LL__________RR): "very different to everything else we've been looking at as it has way more space inside it". Not yet exported or traced. Note: our notation parser refuses gaps over 3 (`ParseLineNotation`), while real gaps run 3-12 (RECON-corpus-census2).
