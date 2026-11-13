# REVIEW-opus-since-move: review of d59a4a488e..295164422f (161 commits, 44 code) after the src/junction move (at 295164422f)

Read-only review (Opus, xhigh). Read: REVIEW-opus-since-terrain-placement (verdict, top 5); DECISIONS D213-D225 (and D182, D187, D193, D204, D218 text); STATUS line 2; briefs IG1-IG6, HC1, HM1, P3b, P4b, P5b, P6b, RW1-RW6b, OF1, OF2, MJ1, TU1-TU4, LP1, TP1, CN1, QL1, QL2, CR1, RS1, GS1; CARDS-crossings; RECON-psg-hubs; every CLAUDE.md under src/junction. Code read at 295164422f: topology/junction_topology.cpp (T), placement/junction_levels.cpp (L), tool/junction_hover.cpp (H), tool/junction_gui.cpp (G), junction_layout.cpp, game/junction_game.cpp, src/rail_cmd.cpp. Nothing built or run except a flow-tracing script over the PSG hub JSON (C1-C3 section). HEAD has since moved (GS1 15d3865b94, RS1 recon 9ddf10b71f): line numbers in T and L are unchanged there. INFERRED = reasoned, not run.

## Verdict
The level solve is now close to rock solid where it mirrors the game. HC1's exact clamp, HM1's foundation via GetRailFoundation and IG3's heads via a copy of CheckBridgeSlope are all faithful. P4b is exact by an invariant and a test, and P6b is sound because the cone only ever shrinks ranges. RW1-RW6b replaced the positional per-row code with token-driven chains, and the prio element now has one source. Those are general rules, not patches.

The foundation is not solid in three places:
1. **Heights are solved on the rendered, signalled layout, not on the graph.** This contradicts D213/D201 and D218 §15.1, and the root CLAUDE.md states the opposite as a fact (F1). Consequences:
   - a height failure cannot name a graph element;
   - a ramp cannot be a token;
   - the hover cannot choose a tunnel.
2. **The graph and row model is narrower than the rules it claims.** Each of these is an honest "not ruled yet" refusal, but together they mean neither LL_RR x LL_RR nor LLL_RRR is one C1-C3 ruling away:
   - two legs per split;
   - one in, one leg, one out per row;
   - one bridge per band edge;
   - D224 applied on the join side only (F2-F4).
3. **The hover path has silent-wrong holes that no oracle covers.** An X|Y diamond counts as legal occupancy. Bridge middles are never checked against tile classes. Levels are not clamped to the map, and terraform is posted, irreversibly, without a command test. The prio length is sized on the legacy T and applied to LL_RR (F5-F9).

The topology/placement file split is safe as a code move. It is not the next structural step; F1 is.

## Top 5 changes (ranked)
1. **Levels on the graph (F1).** Build the level input from the placement's chain elements, each with an (edge, token, copy) tag:
   - heads from Heads elements;
   - prio tiles from `placement.prio`;
   - render and signals once, afterwards.

   Failures then name elements, and a ramp can become a token. This is RS1's Cut C. Do it before Cut A/B, because those would freeze the render-inside-placement write-back (RS1 write-back 2) into the new folder.
2. **Generalise the graph and row model before any more topology work (F2-F4):**
   - lane choice as a cascade of two-leg splits (k-way keys);
   - D224 mirrored on the exit side;
   - rows that hold 0..n nodes instead of exactly in/leg/out;
   - several single bridges per band edge.

   Pin each next refusal in a test.
3. **Close the silent holes on the hover path (F5-F7):**
   - shared tiles only for Upper|Lower and Left|Right;
   - a class fact on bridge middles (D175, D93, D104);
   - corner bounds clamped to [0, map max];
   - tiles over existing tunnels made unmovable;
   - terraform steps test-run, or clamped, before they are posted.
4. **Prio length from the junction's own geometry (F9).** D119's simulation on the graph layout, not legacy SizeJunctionPrio on the T node model. Stop building the legacy library in projection mode.
5. **A structural hover bound plus the CLAUDE.md corrections (F15, Q5).**
   - A wall-clock deadline in the near search.
   - A timed test for the main-thread seed and snapshot.
   - A fixture class where X passes but levels fail.
   - Fix the eight wrong facts in Q5; one of them is a fact that cites D218 for the opposite of what D218 says.

## The last review's top 5: status
1. **Occupancy legality without a reference: DONE** (e4e5b75913).
   - Legality is now "related by the graph, or no shared track end" (T:2266-2281).
   - Still solution-derived per state, but dormant while edits are parked: ties and order by flat columns (T:2290-2312), hug by "the flat solve's order" (T:2322-2335), Y clearance "from the flat state" (T:3110).
   - The new rule has its own hole (F5).
2. **The R7 fix: NOT DONE.** R7b is parked (b76b962b4e) and no structural edit is live.
3. **Edit-soundness sweep: DONE.** ES1/ES2 (407604f7f8), EDITSOUND at tests/junction/topology/junction_topology.cpp:2982. The hover keeps no checker. That is acceptable only because it runs no edits; the [realterrainsweep] checks rules on every planned fixture.
4. **§17 restructure: NOT DONE.** There is no negotiation code; deferred, which is fine.
5. **Height model and ramp rule: MOSTLY DONE.** HC1 (corner cones = exact), HM1, IG3, IG4 (the ramp rule sees bridges), P3b/P4b/P6b. Still open:
   - windows counted in visits, not units;
   - uniform mass in the ramp rule;
   - height clamp, flood and rate limits (F7, F10).

## Q1/Q2. Foundation and level solve: findings
**F1 [wrong, high] Heights read the render (D213/D201, D218).**
- Evidence:
  - T:3471-3484: the terraform overload of PlaceJunctionTrackGraph runs RenderJunctionTrackPlacement, then SignalJunctionTrackRoutes, then SolveJunctionLevels on `layout`, `routes` and `prio_beside`.
  - L:1497-1520: the solve takes its tile bits and heads from `layout.tracks` and `layout.bridges`.
  - Its reasons name tiles and corners, not graph elements (L:457-466, L:1458-1465).
- Effects:
  - D218 §15.1 (a ramp is a token that leans toward its feature) and D213 (a failure names the element to edit) cannot be met;
  - the hover renders and signals a second time per plan (H:34-40);
  - levels.h includes topology.h (RS1 §3).
- Fix: Top 5 item 1.

**F8 [missing, medium-low] Both heads inclined (the valley bridge) is not representable.**
- The head kind is derived, not chosen: the higher head is inclined and equal heads are both flat (L:680, L:885, L:1083; deck level L:905).
- The game accepts two heads on their valid inclines with equal z_start (tunnelbridge_cmd.cpp CheckBridgeSlope; L:92-100 mirrors it).
- So on a valley site both heads are terraformed.
- Fix: a per-bridge head-kind variable over 4 values, constrained to equal z.

**F7 [wrong, medium] Levels can leave the game's height range, and the terraform is never test-run.**
- L:352-362: levels run from lowest-3 to highest+3, with no clamp to [0, map max].
- The estimate prices terraform and planned tiles from the plan and runs no command test (junction_layout.cpp:350-371, :534-539).
- H2 posts the terraform first, and it is irreversible.
- Snapshot tiles over an existing tunnel are Buildable (junction_game.cpp:333-338). Lowering into a tunnel fails in game (INFERRED).
- Counterexample (INFERRED): a ramp-rule or tunnel lower bound on coastal ground at height 0 asks for corner -1. The ghost shows green and the build fails after the terraform.

**F10 [missing, low] Ramp rule gaps (b) and (c) remain.**
- The windows count visits (L:525), so diagonal half pieces make a window shorter than tl units.
- k assumes uniform mass (legacy/junction_generator.cpp:108-121 at 295164422f).

**F11 [wrong, low] std::abort is reachable in the descent (L:1425).**
- A level where a tile has no choice costs NO_CHOICE = 1e12 (L:712, :1170-1172).
- A held state with violation over 1000 levels costs more (VIOLATION 1e9, L:711).
- The descent can then pick a level inc_apply refused, and the restore aborts the game.
- Fix: compare (no-choice count, violation, cost) lexicographically, and return a reason.

**Checked and correct against the game:**
- GetBridgeHeadGround uses the post-foundation slope, as CheckBridgeSlope does (L:92-100).
- The span bound [z - max_bridge_height + 1, z] per corner equals the game's min/max tests (L:714-715).
- Tunnel portal: raw incline, no foundation (L:148-169). A middle corner >= L+1 equals CmdBuildTunnel's exit search, because tile min-z changes by at most 1 per step.
- The foundation-after check (L:875-901) and the P3b steep-slope guard (L:889, :1096).

## Q1/Q3. Graph, order, structure: findings
**F2 [missing, high] D224 is join-side only.**
- T:398-404 moves branch-only merges into the band.
- On the exit side, a main lane with legs to two branch lanes (pairing any) gets two split nodes at its main slot: Partition by group, then by slot (T:282-307). Both sit at level 2*row+1 (T:396).
- Its row therefore has two legs and is refused: "a lane other than in, one leg, out" (T:979-987, :1281-1297).
- Evidence: the CX1 dump (scratchpad/dump.txt, pairing 0) shows n1L1 -> n20L8 and n1L1 -> n23L8 (e14, e15), and n3L3 -> L8 twice (e16, e17).
- LL_RR x LL_RR (any) therefore stops here after C1-C3 (INFERRED: structure refuses first).
- The same row-shape rule refuses partial-movement topologies, which is D222's "later movement toggles".

**F3 [missing, medium] A band edge may carry only one bridge.**
- T:1402-1404: "a bridge over several band crossings is not ruled yet".
- In the dump, joiner e5 crosses exit legs e40 and e43; structure gives each its own single bridge (T:909), so e5 is refused.
- This contradicts topology/CLAUDE.md: "one bridge or tunnel may straddle several crossings of one path".

**F4 [missing, high for LLL_RRR] Splits have two legs.**
- Partition refuses more than two keys (T:289-291).
- So LLL_RRR x L_R fails at the graph: the S lane splits to 3 W-out lanes, and 3 W-in lanes merge into the S-out lane (NM1's "3-way splits (30)").
- The general rule is a cascade: split the ordered key list into two contiguous halves, recursively.

**F12 [wording, low] An order comment contradicts the code.**
- The comment says that when every candidate defers, the leftmost goes first (T:594-597).
- The code fails instead (T:641). The failure is right: nothing else can bring the partner into view. Fix the comment.

**General and sound:**
- OF1 rules A and B (T:567-707);
- OF2's "between its row neighbours" (T:726-772);
- MJ1's in-band merge of adjacent feeds (T:598-675);
- the LP1 pairings (T:86-118; dealing both groups equals D221's text while each main group has one turning movement);
- RW5's stack from per-edge needs (T:861-891).

The row shape stays a hard assumption: `in_edge/leg_edge/out_edge[r]` (T:1281) is only a lookup, but the "exactly one each" refusal is the rule (F2).

## Q1. Hover-path correctness: findings
**F5 [wrong, medium] Occupancy admits an X|Y diamond.**
- Two unrelated pieces on one tile are illegal only if both reach a common tile edge (T:2272-2281).
- Beyond Upper|Lower and Left|Right, the one pair this admits is X|Y: an at-grade crossing with no node (D182; D217 "never share a tile").
- Render merges the bits silently (T:3801-3810).
- Fix: allow only the two parallel-diagonal pairs.

**F6 [missing, medium] Bridge middles are never checked against tile classes.**
- JunctionFacts has no span fact (junction_facts.h:32-39), and placement never tests middle tiles. The bridge loop in L only bounds heights (L:389-395).
- The estimate skips the command test for any bridge with a planned span tile (junction_layout.cpp:350).
- Example (INFERRED): a house in an LL_RR gap row under a joiner's bridge is cleared by CmdBuildBridge, or the bridge fails after the terraform. D175: bridges never span blocked tiles.

**F9 [wrong, medium] The prio length comes from the legacy T.**
- G:842-844: SizeJunctionPrio (the legacy node-model T) sets `prio_tiles` for every topology.
- D102/D137 tie the length to the junction's own timing, and nothing on the graph pipeline checks it: G9b checks pre-signal holding, not D119 delay.
- G:845 also builds the legacy library on every spec change, though projection is always on.
- GS1's new legacy/CLAUDE.md fact names the dependency but does not rule it.

## Q4. Crossings C1-C3: evidence from the PSG hubs
**Method:**
- Script: /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/c13/flows.py.
- Directed lanes: a signal allows travel only in its own trackdir, and 90-degree turns are forbidden.
- Over the hub JSON crop box: every bridge/tunnel's carried flows and the flows on its middle tiles, as (entry side > exit side).
- Run on the 13 three-distinct-side T candidates with 2+ tracks per bundle (227_3, 137_1, 154_3, 165_2, 216_2, 248_3, 271_3, 278_1, 279_3, 310_1, 312_3, 315_2, 329_1).
- Main-lane sides come from the entry rows.
- Caveats: crops of meshes include other lines; counts are per bridge direction. Treat this as indicative.

**C1, exit leg x exit leg from opposite main directions: mixed.**
- psg310_1 (branch N): Y bridges (265..268,211)->(265..268,206) carry E>N over W>N. The E-in lanes (y 213-215) are on the far side and the W-in lanes (y 206-211) on the near side, so far over near: card (b).
- psg329_1 (branch N; near W-in y 224-250, far E-in y 253-258):
  - near over far 3 times: bridges (52,238)->(56,238) and (55,234)->(55,231), and tunnel (57,239)->(57,236), where far dives;
  - far over near once: tunnel (55,241)->(55,236).
- Weak: one hub each way.

**C2, a shared through+turn edge x a turning leg: supports (a), one hub only.**
- psg312_3 (branch E): tunnels (26,52)->(30,52) and (26,53)->(30,53) carry N>E exit legs.
- The surface Y tracks at (27,52) and (27,53) carry both N>S and N>E: one lane before its exit split. It stays on top and the turning leg dives.

**C3, joiner x joiner: 2 of 3 hubs support (a), read as "the joiner bound for the farther lane goes over".**
- psg227_3 (branch W; near lanes x 233-242, far lanes x 261-268): bridges (236..239,275..272)->(240..243,...) carry W>S, the far-lane joiner, over W>N.
- psg329_1: bridges (61,236)->(61,239), (62,237)->(62,240) and (64,230)->(61,230) carry N>W, the far-lane joiner, over N>E.
- Counterexample, psg271_3 (near x 306-315, far x 325-327): bridge (311,339)->(311,334) carries W>N, the near-lane joiner, over W>S.

**Pattern:**
- Coop builders follow no fixed over/under rule for leg x leg crossings.
- Tunnels carry the diving leg in 7 of the 13 hubs (227_3, 248_3, 271_3, 278_1, 312_3, 315_2, 329_1).
- The evidence fits the cards' defaults: C1 (b), C2 (a), C3 (a), all expressible as "the leg that crosses more main lanes goes over; through never dips".
- Add a cost refinement later (D223, D189), with tunnels as the cheap diver (TU5).
- The MSH03 image (227_bbh03) cannot be matched to a candidate (as RECON-psg-hubs says), and I read no leg x leg crossing from it.
- Doc nit: RECON-psg-hubs' "15 best-looking hubs" table has its rows joined on one line.

## Q5. The src/junction layout and CLAUDE.md
**Facts wrong against the code or the rulings:**
- (a) Root: "no stage reads a rendered layout as its input (D201, D213)". False (F1).
- (b) placement: "a ramp is a tile, not a graph token, and a height failure is not a graph edit (D218, G1 §15)". D218 and G1 §15.1 say the opposite. This is Z2c's unruled first cut promoted to a "decided fact", against D225.
- (c) placement: "a levelled foundation applies only across an incline and to diagonal pieces (D152b)". Wrong: a lone diagonal is Leveled only on a three-raised slope; otherwise it gets a halftile or special foundation (rail_cmd.cpp:369-376). The code follows the game (L:200), and terrain/CLAUDE.md is right.
- (d) game: "the estimate is the exact sum of test-only runs of every command". False for planned tiles and terraform (junction_layout.cpp:350-371).
- (e) topology: "only build, pricing, JSON, ASCII and the rig script branch on `tunnel`". Placement (T:3431), render (T:3671) and levels (L:380, :507, :569, :1509) also branch on it.
- (f) topology: "one bridge or tunnel may straddle several crossings of one path". Refused for band edges (F3).
- (g) Root: "the checker is never an acceptance step (D193)". This drops D193's condition, "until RC0 shows full coverage"; RC0 found 2 of 11 rules covered (STATUS:40) and no re-run is recorded. Either record the ruling or restate the fact.
- (h) legacy: "an unreachable non-projection branch". EnsureLibrary calls SizeJunctionPrio and BuildJunctionLibrary live (G:842-845).

**The split.** RS1 shows the two write-backs that matter:
- CutJunctionTrackBridges places to choose the structure (T:4262);
- the terraform overload renders, signals and levels inside placement (T:3462-3486).

A Cut A/B file move keeps both. Do Cut C first, carried through to F1's level input built from chains, then move files. Do not export the per-row in/leg/out lookup as an interface: F2 removes it.

## Q6. The hover bound: brittle
**F15 [missing, medium] The < 100 ms bound is measured, not structural.**
- Measured: [realterrainsweep] requires `near_max < 100` over 48 fixtures, one centred spot each (tests/junction/tool/junction_real_terrain.cpp:385, :390, :396). P6b: LL_RR mean 11.0 ms, max 37.1 ms.
- The near search has no deadline (H:85-108). Its worst case is 152 full solves at offsets where X passes, so P5b's reject (X-only) cannot skip them, but the level solve fails. A single LL_RR solve took up to 28 ms after P3b (INFERRED: rough mountains or dense coop maps reach seconds; CN1 only stops it when the cursor moves).
- Outside the bound, on the main thread:
  - MakeJunctionGraphSeed including CutJunctionTrackBridges on every spec change (G:1205);
  - the snapshot with margin 16 and a QueryJunctionClear per tile (G:1229).

  Neither is timed in a test.
- LL_RR x LL_RR and LLL_RRR, the bigger boxes, are untimed.
- Fix:
  - a wall-clock budget in PlanJunctionHoverNear that returns the best so far;
  - a timed test of seed and snapshot;
  - a fixture class that passes X but fails levels.

## Decisions for the engineer
1. Ramps and levels on the graph (D218 as written) versus per tile on the render (as built, F1).
2. C1-C3 defaults per the PSG evidence (C1 (b), C2 (a), C3 (a)), with a later cost or tunnel refinement.
3. Whether D224 mirrors on the exit side (F2).
4. D193: is the checker out of acceptance by ruling?

## Suggested tests
- `[topology]` occupancy: an unrelated X piece and Y piece on one tile are refused (F5).
- `[topoproj]`:
  - a house on an LL_RR gap tile under a bridge: the placement moves or fails naming the bridge (F6);
  - LL_RR x LL_RR under each pairing pins its refusal string. tests:4351-4440 now only WARN, and `CHECK(routes || why)` always passes.
- `[hoverplan]`:
  - ground at height 0 with a forced lower bound: no corner below 0 (F7);
  - a valley site with both head tiles on their valid inclines: no terraform on the heads (F8);
  - the descent never aborts on a state with violation > 1000 (F11).
- `[topology]` graph for LLL_RRR x L_R: a two-leg cascade, no "more than two legs" refusal (F4).
- `[realterrainsweep]`: the near search on a ridge fixture where X passes everywhere stays under 100 ms (F15).
