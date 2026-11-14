# REVIEW-G4-overprescribed: why LL² does not place, and what we over-prescribe (at 0167e2d59d)

Read-only review at 0167e2d59d. No solve, build or test was run (engineer's scope change); `.claude/nodes` was not touched. Corpus facts come from read-only Python over the PSG map exports (scripts and outputs in SP, below). INFERRED marks what I reasoned to but did not read or run.

Abbreviations: jj = `src/junction/placement/junction_joint.cpp`; tjj = `src/tests/junction/placement/junction_joint.cpp`; topo-C / place-C / sig-C = `src/junction/{topology,placement,signals}/CLAUDE.md`; OD = `/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad`; SP = `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/review-g4`. LL² = LL_RR x LL_RR, LLL³ = LLL_RRR x LLL_RRR.

## Summary
1. **Root cause.** We place a bigger and stricter LL² than coop players build, in a representation where tracks may overlap unless a family of rows forbids it. G4 adds those families one piece class at a time, and LL² is the first hub that needs them all, so every recon finds more. Nobody ever had a known-legal LL² to test the rules against.
2. **Bigger.** Our LL² uses pairing any: 20 lane paths, 28 nodes, 48 edges, 9 single-bridge crossings and 40 Unders. Both full-movement LL² hubs in the PSG census (psg173_14, psg327_12) are **lane-keep**: 12 lane paths each (traced). Under keep, LL² is the T with every track widened to a 2-lane bundle.
3. **Stricter.** Our LL² has 4 prio elements of 23 tiles (D119, D159); psg173_14 merges with plain merges. Our ports sit at gap 1; real line ends sit 3-12 apart (gap 1 at 7 of 79 ends). Our turning traffic runs only vertically in bands and moves sideways only at 45°; coop runs turning pairs horizontally between the direction groups, diagonally, through 90° corners, and on three levels.
4. **Size.** Our LL² joint optimum spans 128 x 88 tiles and is still illegal (D274). psg173_14 does all 6 movements in a 46 x 30 core.
5. **Rows and bands** (Q2 #1): rows match coop, because main lines run straight through. Bands do not: coop never forces turning traffic to run north-south between rows.
6. **Existence under our rules: unknown.** The non-occupancy groups are jointly feasible (268600, DESIGN-G4 §11.0). No solve with complete occupancy has ever run. The decisive probe is specified in Q3 for a Sonnet step.
7. **Recommendation: do not continue G4's static families** (2b2b2-4, 2b3). Path "real hub first":
   - (1) a corpus oracle (CheckJunctionRules on imported real LL² hubs);
   - (2) LL² keep probes;
   - (3) three cards for Pete;
   - (4-7) build LL² keep as the T of 2-lane bundles. Bundle edges were designed (DESIGN-nodes.md:48, FABLE-nodes-perf.md:54) and never built. Each bundle piece is overlap-free by construction, and the top level is T-sized.
8. **Change course if** the current pipeline seeds LL² keep cleanly at a realistic gap within D253. Then add lazy exact separation (Q4 b) instead of bundles.

## Q1. Real LL² layouts

| | psg173_14 (PSG 173) | psg327_12 (PSG 327) | BBH building (coop wiki) | 227_bbh03 (Junctionary "Simple Classic LLRR hub") |
|---|---|---|---|---|
| Source | census2.tsv row; box 73,92-130,133; render SP/psg173_14.txt, SP/z.txt; trace SP/trace-psg173_14.txt | census2_m6.tsv; box 133,403-185,449; SP/psg327_12.txt, SP/trace-psg327_12.txt | OD/coop/files/BBH_building_1-5.PNG; RULES-hubs.md H20-H21 ("LL5RR to LL5RR 3-way") | OD/coop/files/800px-227_bbh03.png (sign "MSH03/mfb") |
| Footprint | box 58x42 = core 46x30 (6-tile margin, RECON-corpus-census2 Method) | box 53x47 = core about 41x35 | not measurable (image); modules 10-30 tiles out along each arm | not measurable (thumbnail) |
| Movements and lanes | all 6; every in lane reaches exactly one lane per destination (**keep**), 12 paths | all 6, **keep**, 12 paths (branch on N) | 3 exits, 3 joins, 1 ML crossing, 1 crossover (H20) | all-turns hub; lines as separate 2-track pairs |
| Direction-group gaps | W 7, E 10, S 10 | 3 / 7 | LL _5_ RR | wide (5-15, by eye) |
| Crossings | 18 bridges (middles 2:8, 3:7, 4:3) and 4 tunnels (middle 4), SP/stats.txt. A ladder of 12 X-axis bridges at x95-100, rows 99-112, over the branch's incoming 2-lane trunk. Doubled Y bridges at x109-114 over the eastbound-in area, with doubled tunnels underneath (rows 115/116): 3 levels | 18 bridges (2:10, 3:8): 9 over diagonal pairs (the coop `Bv^B` S-bend domino), 4 over perpendicular pairs; 6 tunnels (2-3) | bridges and tunnels at the crossing and crossover (images) | long diagonal ladders of short bridges, one per crossed track |
| Doubling | every main-traffic bridge doubled per lane, stagger 1: westbound lanes 113 (copies on rows 107/108) and 114 (rows 109/110, 2 bridges in series per copy); both eastbound exits (rows 103-106) | copy split/merge pairs around bridges (e.g. (156,429)/(156,430)) | doubled (images) | doubled pairs of ramps |
| Splits, merges, prios | 6 splits on lanes, 6 merges; main-lane merges (97,98), (106,98), (85,108), (89,107) have **no prio** (no join switch upstream on the traced lane, e.g. W98→E101 meets only (83,98) and (97,98)); copy merge (86,109) is 1 tile before the joiner merge (85,108) | 32 switch tiles; prio not checked | joins are "all-to-all non-blocking merging", prios "last" (H21, H37: BBH prio optional) | not readable |
| Diagonal share | 204 of 596 rail tiles | 269 of 608 | high | high |

Also in the census (not used): psg325_8 (3 of 6 movements), psg41_90 (21x17, tunnels only, E↔S missing), psg173_16 and psg154_34 (partial). Every LL² candidate is keep or partial; none is any-to-any (census2.tsv `missing` column, 2/4 per movement).

**psg173_14 in coop terms.**
- Two main lines, each a pair of lanes. The pairs sit 7-10 tiles apart at the edges. The westbound pair arrives 12 rows south of the eastbound pair.
- The branch's incoming pair comes up the middle as a ground-level trunk under a ladder of short doubled bridges. Each lane of the trunk splits once, into a west joiner and an east joiner, which drop into the main lanes on plain merges.
- The eastbound lanes' exits leave on doubled bridges over the trunk. They run east between the two main pairs, cross over the eastbound-in area on doubled bridges, and merge pairwise at the branch's outgoing pair near the S edge.
- The westbound pair dives through doubled tunnels under the branch's outgoing side.
- No shifters, no balancing inside the box (keep), no prios.

**Our generator's LL² for comparison** (any, form 100100000):
- graph: 48 edges, 28 nodes (wip/recon-o3/dump-LL2_100100000.txt); 9 single-bridge band crossings and 40 Under tokens of doubled row bridges (RECON-O3 Q3);
- 4 prio elements of 23 tiles each (RECON-O1-item-rows.md:7, test spec);
- ports at gap 1;
- joint points spanning 128 x 88, with the X solve failing (D274).

The T is 12 edges and 6 nodes (dump-T.txt). LL² keep is that graph at bundle width 2 (INFERRED from keep = lane k → lane k on the T's six movements). Its extra lane crossings are one inside each 2-lane split or merge, where the far lane crosses the near one: psg173_14's W98 exit crosses W99's doubled tunnel at x82-87.

## Q2. Over-prescriptions (ranked by how much each plausibly blocks LL²)

| # | Our rule (where) | Real evidence | What ours forces | Who rules |
|---|---|---|---|---|
| 1 | **Rows and bands.** Every main lane is a row through the box. All other traffic is a band edge: y-monotone, starting south or north (place-C:14). Sideways moves are 45° S-bends; a Corner appears only at a row node exit or a joiner's arrival (REVIEW-joint-loop F5). One global stack (src/junction/CLAUDE.md:32, topo-C:37); rows change only by an edit (place-C:23) | Rows fit all four hubs: main pairs run straight with row S-bends. Bands fit none. psg173_14's exits run **horizontally** for about 10 tiles between the direction groups, on their own doubled rows (103-106). psg327_12 weaves diagonally (44% diagonal tiles). BBH and 227 run turning pairs as long diagonals between modules. Coop crosses under a Corner's diagonal in 45% of cases (RECON-corpus-sbend-crossings) | Height grows with sideways distance (e1 needs 26 band rows, REVIEW-joint-loop F5). Bands interleave every arm's turning tracks, so occupancy, event-order and form binaries scale with lanes² | Pete (structure by rule, D207; D256 took only the crossing case) |
| 2 | LL² target uses **pairing any** (D221 default A, the orchestrator's recommendation; tjj:1395 `MakeLoopStart("LL_RR", "LL_RR")`) | Both full-movement hubs are keep; no candidate is any-to-any. Coop balances outside hubs (load-balancer pages, OD/coop/files/2_to_2_load-balancer.png) | 20 paths and 30 crossings instead of 12 paths (RECON-nonadjacent-crossing.md:11-12). D224's leg merges and D204's line-choice splits exist only under any | Orchestrator default, reversible |
| 3 | **A prio on every main-lane merge** (D119 ML delay zero, D159; sig-C:19, 25) and no split in its coverage (D108) | psg173_14: 4 plain merges, one 1 tile after a copy merge. BBH: prio optional (RULES-hubs H37) | 4 prio items of 23 tiles; split-to-merge on each lane at least the coverage plus a segment | Pete (D119/D159 were made for the SLH) |
| 4 | **Ports at the notation spacing** (place-C:26, topo-C:37, D38 "on flat ground the legs use it exactly"). Tests use gap 1 | Gap 1 at 7 of 79 two-way ends; 4-10 at 30 (RECON-corpus-census2, gap table); psg173_14 7/10/10 | The widening happens inside the box through fans. S port spacing pins the trunks (the RECON-dense-crossers cycle) | Pete (D38); the test target is a default |
| 5 | **Per-lane graph**: lanes are expanded to paths before geometry (`ExpandJunctionLanes`) | Coop routes lane pairs as bundles: psg173_14's trunk, the exit pairs and the westbound pair | Lanes of one movement are independent edges, so crossings and occupancy pairs grow ×4 | Design; DESIGN-nodes.md:48 said "one bundle edge" |
| 6 | **Crossing forms**: a single bridge over a band edge only as an S-bend (span exactly 2) or a square run (place-C:30, D256/D260). A single bridge over several band crossings is refused (topo-C:32) | Coop: Corner diagonal 45%, doubled pair 13%; the tool admits 35% (RECON-corpus-sbend-crossings Part C) | Forms that fit fewer layouts, plus the t, w and q machinery | Pete for new forms; the refusal is a scope gap |
| 7 | **Over/under**: a joiner goes over a turning leg on a single bridge; the through lane goes over (topo-C:28, D223, D227) | psg173_14: main-traffic exits go **over** the branch's incoming trunk, on doubled bridges (rows 103-106), and over the eastbound-in area | Branch-on-bridge single crossings in bands (LL² has 9) | Pete; one hub only, low confidence |
| 8 | **No tunnels in placement** (topo-C:65) | psg173_14 has 2 doubled tunnel pairs; coop overall has 775 tunnels to 2237 bridges (RECON-corpus-crossings) | Every crossing is a bridge with heads and hug | Pete (D220 allows tunnels); needs terraform on flat, so later |
| 9 | Exact parallelogram stagger (place-C:7) | Symmetric pairs 31% (RECON-corpus-crossings §2) | One fewer shape | Default, minor |
| 10 | Default cut of 2 crossers (topo-C:32, 53), although D228 removed the cap | Bridges with 3-4 tracks under them in psg173_14 and psg325_8 | More series heads; cut search | Default, minor |

Agreements, not over-prescriptions:
- D188 (four bridges per LL crossing): psg173_14 rows 107-110.
- D80/D174 (main bridges doubled): exits included.
- D142 (lanes of one direction adjacent).
- D24 (splits before merges).
- Series gap of 1 tile (73%, RECON-corpus-crossings §3).
- D187 (both lanes split to the exit).

Rules no real LL² needs once the hub is keep:
- D224 (legs merge before joining);
- D204 (line-choice split by lane);
- D205's leg merges for one main direction.

## Q3. Does a legal LL² exist under our rules?

**What is known:**
- (i) The joint's recorded groups without full occupancy are feasible: 268600 at 100100000 (DESIGN-G4 §11.0); q free gives 233050 (STATUS line 10).
- (ii) The fixed path at that point fails, and the loop's round 1 is infeasible ("X solve: ... bridge over S-bend", §11.0).
- (iii) The joint has no rows for band pieces against fans, nodes, row pieces or prio tiles (RECON-O3 finding 3). Its O1/O2 static rows are recorded but not read (place-C:56).

So existence has never been tested. INFERRED: with an unbounded box, most groups are floors that stretching satisfies. The risk lies in pinned spacings: ports at the notation, exact span 2, exact stagger, heads hugging. These can make a contradiction that holds at any size, as RECON-dense-crossers found.

**Probe E: the elastic joint, for the Sonnet step.** It is a test-only patch on jj, in `.claude/nodes`.

Rows that get a slack (one nonnegative continuous slack per row, added to the side that binds):

| Row (jj) | Group |
|---|---|
| relation rows (:343) | `relation.reason` up to the first " (", e.g. "S port spacing", "chain run", "hug crossers", "lane split run", "bridge heads", "prio element", "stack rows", "band gap", "S-bend size", "Corner size" |
| clash rows (:376) | "occupancy" |
| over rows (:279, :289) | "bridge over S-bend t" |
| curve-gap counter rows (:120, untagged today) | "curve gap" |

Solve in two phases:
- phase 1 minimises the sum of slacks, with all other costs at zero;
- phase 2 caps that sum at the phase-1 value and minimises the flat cost.

Report per group: the rows with slack, the sum and the maximum, and every slack row's reason string (each names its edges and nodes).

Instances:

| | LL² | at |
|---|---|---|
| (1) | any, form 100100000, port offset 24 | round 1, recorded at the round-0 joint point (tjj:1395-1420 helpers `Ll2Start`, `ChoicesWithForms`, `RecordAt`) |
| (2) | keep (`MakeJunctionHubTopology(llrr, llrr, JunctionLanePairing::LaneKeepsLane)`, as tjj:1312-1322) | round 0 and round 1 |
| (3) | keep at LL_______RR on both lines (psg173_14's W gap) | round 0 and round 1 |

At zero total slack:
1. Place at the joint's choices (`PlaceJunctionTrackGraph`, as tjj:1291).
2. Render.
3. Run `CheckJunctionRules(layout, spec, topology)`.
4. Also run the drop-one-group variant (as tjj:1297-1298).

Cost: about 60 lines of test code. Each LL² start takes about 40 s (RECON-O3 Q8) and each joint 0.4-10 s (§11.0), so under 15 minutes of runs.

What it shows:
- (a) **Zero slack and a clean checker**: a legal LL² exists under our rules. Then G4 is only about speed; record the footprint.
- (b) **Slack only in point-recorded groups** (occupancy, hug, clearance): the point rows block, not the rules.
- (c) **Slack in static groups that persists at gap 7**: a rule contradiction at LL², named by group. These are the cards for Pete.
- (d) **Zero slack but checker violations**: the joint misses families, and the checker's list is G4's remaining family list. This measures how far 2b2b2-4 is from complete.

The near-solve rendering for the rig (the coordinator's earlier note) belongs to this step.

**Probe C: the corpus oracle (recommended first).**
1. Convert the psg173_14 and psg327_12 tiles (map export JSON in OD/psg/maps; read with `python3 -I`, script outside the data dirs) into a `JunctionLayout`: tracks, bridges, tunnels, signals.
2. Call `CheckJunctionRules` with an LL² keep topology whose notation matches the hub (LL_______RR and so on).

Its violation list is the measured version of the Q2 table: which of our checker rules real, working coop hubs break. It answers the engineer's "the save games can point us". Cost: a converter of about 150 lines plus port mapping, about 2-3 h. There is no layout importer today (only `JunctionTerrainFromJson`).

## Q4. Model choice

The engineer's question first: why can tracks run through each other?
- Placement is points plus relations. Two tracks stay apart only if some row separates them.
- Our rows exist per piece class: item rows (O1), fans (O2), bands (O3), then nodes, fans and prio tiles in band rows (2b2b3) and clearance (2b2b4).
- A class without rows lets tracks overlap. That is how the LL² joint is "Optimal" yet illegal (D274), and why each recon finds a class (RECON-O3 finding 3).
- The pipeline is already topology, then shape (order, structure, programs), then metrics (placement). The faults are that the shape stage is T-derived (Q2 #1-7), and the metrics stage separates by class instead of by face.

| | Sound | Complete vs checker | LL² seed | LLL³ seed | Hover | Keeps |
|---|---|---|---|---|---|---|
| (a) G4 static families (2b2b2-4, 2b3) | yes, if the families are complete | rests on a proof sketch (DESIGN-G4 §10); every recon has found a missing class | about 125 binaries, about 2600 rows; INFERRED 1-10 s | about 489 binaries, about 7600 rows; INFERRED ≥ 10 s, plus a 15.6 s arrangement (D263) | difference system at the seed's choices, INFERRED < 100 ms | all merged work; 6 steps left |
| (b) one MILP plus lazy exact separation: render, find overlapping piece pairs, add for each a disjunction "separated along x, y, x+y or x−y" (the separating axes of octilinear convex pieces), re-solve | yes: each disjunction holds in every legal layout, so the model stays a relaxation | yes for occupancy at the fixed point; other rules stay families | 689 clashes at one LL² point (RECON-O3 Q1); INFERRED hundreds of disjunctions × 3 binaries, so tens of seconds to minutes | INFERRED minutes or more | same as (a) | jj, tile forms, O1/O2 as warm cuts, render, checker |
| (c) tile grid (a binary per tile, piece and edge) | yes, overlap impossible | yes for occupancy; curve gaps, prio and bays are long path sums | 1.4k tiles (real size) to 11k (ours) × 6 pieces × 24-48 edges = 10^5-10^6 binaries; INFERRED minutes to hours | no | no | checker and render only |
| (d) bundles and modules: LL² keep as the T of 2-lane bundles. Bundle split, merge, crossing and bend are small hubs solved by today's pipeline and checked once; the top level is the T | yes; overlap is impossible inside a checked piece, and the top level is the T's working relations | per piece by the checker; at the top, the T's families, which place today | pieces T-sized (T 10-16 ms, LL_RR x L_R 89-120 ms, DESIGN-G4 P5); INFERRED < 1 s | T of 3-bundles; INFERRED seconds | cached pieces plus a T-size difference system, INFERRED ≪ 100 ms | topology to placement, render, signals, levels, checker; parks G4 2b2b+ and the loop |
| (e) fixed embedding plus compaction: event order, forms, bends and fans fixed as data; separation rows with known sides; revisit a choice on infeasibility, named by the IIS | yes, if the map lists every occupant | yes only when the arrangement is one planar map over all occupant classes; today it is per class (`row_pairs`, `band_neighbours`, topology.h:327-345) | no binaries: INFERRED ms to 100 ms per compaction, times revisits; the sweep's order differs from the LL² relaxed optimum in 13 of 21 event pairs (RECON-O3 Q5), so revisits are INFERRED frequent | same | same | the sweep (topology.h:150-155), ARR1-3, A4-A6, programs, hover |

Does fixing the order (e) lose real layouts? It loses whatever our shape stage cannot express. psg173_14's embedding has exits over the trunk, horizontal turning rows, no prio rows and tunnels, and none of that is our sweep's. So compaction of our current embedding cannot reach it at any order. The over-prescription is in the shape, not the compaction.

**Plainly: overlap impossible by construction is feasible within D253 and the hover in two ways.**
- **Bundles and modules (d).** Pieces are checked tile patterns, and the top level is the T. INFERRED seconds for the seed and milliseconds for the hover. Cost: steps 4-7 below.
- **A planar map over every occupant (e).** The rows are generated from faces, so no class can be forgotten. Cost: one occupant list over all classes (today's occupants plus fans, prio, heads and nodes in band rows), a face builder extending ARR1, compaction at fixed choices, and IIS-driven flips. About 4-5 steps, and it keeps the T-derived shape.

The tile grid (c) is not feasible in time.

## Q5. Recommendation

**DESIGN-G4's static-family plan should not continue.** Keep what is merged: the arrangement, A4-A6, O1/O2, the form binary. Do not merge 2b2b1b on its own. Do not build 2b2b2-4 or 2b3 until steps 1-2 report.

Path "real hub first, then bundles":

| Step | What | Red test |
|---|---|---|
| 1 | Corpus oracle (Probe C) | "imported psg173_14 has 12 lane paths and keep pairing"; then its `CheckJunctionRules` list, as data |
| 2 | LL² keep probes: today's seed at LL_RR and LL_______RR, then Probe E on whichever fails | "LL² keep at gap 7 seeds clean or names its slack groups" |
| 3 | Cards for Pete: rows and bands for turning traffic (Q2 #1), prio on LL² merges (#3), port gap (#4). Orchestrator default: LL² tests and presets target keep (#2, D227) | none (rulings) |
| 4 | Bundle graph: under keep, one bundle edge per lane group per movement | "LL² keep's bundle graph equals the T's (12 edges, 6 nodes, same crossings) at width 2" |
| 5 | Bundle pieces: split and merge (staggered lane nodes plus the inner crossing), crossing (k x k, main lanes doubled per D188), bend (inner lane binds CL) | "each piece at k = 2 is CheckJunctionRules-clean on flat ground" |
| 6 | Compose: the T's placement over bundle edges, pieces as blocks | "LL² keep seeds, checker clean, seed < 5 s; [hoverplan] < 100 ms" |
| 7 | LLL³ keep (k = 3); LLL_RRR x LL_RR needs an N→M adapter piece, since keep refuses unequal counts (topology.h:72); card for Pete on balancing | "LLL³ keep seeds clean within D253" |

**What would change it:**
- If step 2 seeds LL² keep cleanly at a realistic gap within D253, skip steps 4-6. Add (b) as the completeness mechanism on the per-lane graph and go to LLL³.
- If step 1 shows that real LL² hubs break a rule Pete keeps, the bundle pieces carry that rule and grow.
- If a bundle piece cannot be made legal at k = 2, fall back to (b) with keep.

## Evidence files (SP, read-only corpus scripts; nothing in the repo changed)
- Renders: `psg173_14.txt`, `z.txt` (core with column digits), `psg327_12.txt`, `psg41_90.txt`, `psg325_8.txt`.
- Lane traces: `trace-psg173_14.txt`, `trace-psg327_12.txt`. Bridge, tunnel and switch lists: `feat-*.txt`. Counts: `stats.txt`.
- Scripts in `py/`. `c2.py` is a copy of OD/psg/census2/census2.py with MAPS set absolute and a `DUMPMV` lane-reach print.
