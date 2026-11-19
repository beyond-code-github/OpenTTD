# ROADMAP: every idea designed, planned or agreed, and its state in the code

The single list of planned work. Rules: every idea that is designed, ruled or agreed but not fully built lives here until it is built (then it leaves, its facts going to CLAUDE.md) or explicitly dropped (then it leaves with the deciding Dnnn). Check this file before designing anything. STATUS.md holds only the current state; DECISIONS.md holds rulings; DESIGN-tracks.md holds the current design. Status checked against wt-planar on 2026-11-18 (survey: grep evidence, file:line in ng1); `PC` = planar_components.cpp. Sources named here that were removed in the 2026-11-18 docs cull (DESIGN-G1, DESIGN-width and other legacy files) are in git at a7f10e9af0. Status words: built, partly built, not built, rejected, superseded, unknown.

## Top 10: the SSS critical path (D362, DESIGN-tracks §17.6; reordered 2026-11-20)
1. **Frame experiment (§17.3, §17.6 step 1).** Slots frames over channels, node groups and port-side corridors; count base-infeasible variants on T2, T3, LL². Absorbs the old slot layer item (§3a, §15, D327) and replaces D359 (D363). Not started.
2. **Shape capacity rows and deck groups (§17.2).** Every structure spans its crossers within its kind's limit; G2 cut to node geometry (D360); D346's break-up rule lands here. Kills f0.
3. **Lengths with no lazy rows (§17.4).** One solve, ladders from a one-shot phase read, optional segments gone, `SolveLazy` an assertion. Absorbs "retire the lazy clash loop" and "free bends" (D338: bends come from Slots).
4. **Rows as tracks and the Signals walk (§17.5, D349, TM5).** End to end checker clean on T2, T3, LL² (D357); `MappedComposer` retires; D345's search retires (D363).
5. **Variant ranking on cheap scores (§17.1).** Turning + step cost + LP bound; full solve for the top few; seed under 5 s (D297).
6. **Remove the by-name branch:** T2 DONE (97d664dc1a, 2026-11-20: T2 goes to `SolvePlanarMapped`, 862.25 o1t3f1s0, checker clean end to end); LL² with SSS (engineer, 2026-11-19). The hand `Composer` keeps T2-only helpers (`Fray`'s t2 parameter always false; `Join`, `Doubled`, `DoubledB` callers unchecked): they go with LL²'s branch.
7. **Heights and tunnels (§17.5, D365, D364).** After item 4. Separator and kind per structure; Heights after Lengths; the hover runs Heights only.
8. **n-lane / LL³ and the multilevel lift (§14).** Same Structure decisions, wider Slots.
9. **Topology as a complete artefact, pinnable crossing order, builder UI (D343/D344).** Required by "any hub".
10. **Corpus growth (§17, D270):** a hub = topology + Structure decisions (separator and kind per joiner, D364) + cross-sections.

(Parked under SSS: the grid layer (§11, D334), revisit only if Lengths' integrality fails; shape-as-flow §8.5; organic terrain repairs (D365 deferral). The G3 track composer is stopped, §16.7.)

## Architecture layers (shape, slots/width, ladders, grid)
| idea | source | status | evidence | depends on |
|---|---|---|---|---|
| Topology -> shape -> rough positions -> grid layering (chip-routing split) | DESIGN-tracks §11, 2026-11-18 | partly built: shape and rough positions exist, grid layer absent | shape: planar_shape.cpp:8, PC:2578; no grid code in planar/ | slots, ladders |
| Windowed grid legaliser around clash sites | §11, §8 end ("windowed grid pass stays an option") | not built (probe queued "after the bends probe") | no hit; PROBE-noclash-* in wip are inputs only | rough layout with no clashes |
| Capacity rows per corridor (global-routing check) | §11 | not built | no hit | face map, slots |
| Slot layer: per-stretch track slots, steps from slot changes | §3a, D327, D349; SPIKE-slots, REVIEW-sideways-modelling (2026-11-18) | spike passed; not built | wip/SPIKE-slots.md run 4: all gap changes match outside staggered prio-join exits and turn geometry | the variant's channels, sites, copies, rows |
| Headings first-class, width measured across own travel, diagonal hubs | §3a | partly built: 8 headings in pieces; the box/axis-port limit stays (D313) | planar_model.cpp AddPiece (DESIGN-pieces step 1b); axis-only refusal PC:1250/1256 per DESIGN-tracks §12 | slots |
| Track-assignment layer between shape and lengths (layered drawing / Brandes-Koepf) | REVIEW-sideways-modelling 2026-11-18 (advice, not a ruling) | not built; = the slot layer idea | wip/REVIEW-sideways-modelling.md | slots |
| Regions/faces as corridors; non-overlap per corridor | §3, §6, D339 | built (as ladders) | L1 face map planar_regions.cpp; `PlanarLineSides::Faces`, planar_components.cpp:758-807 | - |
| Ladders: rungs from a rough solve, two-pass solve, re-read | §7, §9, D339 | built | LadderRungs PC:2422; passes PC:2626 (2 passes; design said up to 3); PLANAR_NOLADDER PC:2626 | - |
| Ladder rungs for bridge copies as walls | §8.1, probe 5 | built (as copies with deck-middle exemption) | PC:758 `LadderCopy` | - |
| Build the face map from the flipped triangle's events | §8 fix found on the way, §9 L1 | built per STATUS (L1 39bcbf9255); not re-verified in code | STATUS.md line 40; PC:2887 flipped channels | - |
| Retire the clash loop; refined solve only, lazy loop as test oracle | §8.2, §9 (c) | not built | SolveLazy still the solve path PC:2381, PC:2172 | ladders complete on T3 (T3 left 6 clashes at probe 4; probe 5 fixed) |
| Free bends on every free stretch; step rules and step bits go | §8.3, D338 | not built | candidate-step bit `steps` still present PC:751, Flex jog PC:1043 | clash loop retired; slots or shape program |
| Shape program (least turning MIP, S0-S3b) | §12, D340/D341 | built | planar_shape.cpp; PC:2578-2594 scores default and program shape | - |
| S4: absorb variant bits into the shape layer (ties as headings, steps derived) | §12 S4 | rejected: ties are not headings, ties and steps are real area choices (S4 recon and offline check) | wip/RECON-ties-steps.md; §12 "S4a/S4b NOT built" | - |
| Shape as flow over faces (bends + corner angles sum to one turn) | §6, §8.5 "later" | not built | spikes only: SPIKE-shapeflow*, PROBE-flowturns | face map |
| Hard-coded shape hit list cleared or taken to engineer as coop conventions (fork-leg jog, perpendicular-under-prio, detour tries, join `lo`, copy sides, merge exit step, 2-lane and axis-only refusals) | §12 S0 | not built (list recorded, no clearing step done) | RECON-shape-seam.md; refusals PC:1261 | shape program S2/S3 |
| Fix: no-U-turn row (channel from a node turns at most 135 deg net) | §12 judgement call 3 | unknown: stated as "row added in S2"; not grepped | planar_shape.cpp not read past header | - |
| Hard anchor (pin one port coordinate) | §8.4, D348 | built | PC:1695-1700, shift PC:2661 | - |
| Tight per-piece big-M bounds on optional lengths | §8.4, §10.3 | not built | OPTMAX cap present per STATUS; no window code | rough solve |
| Left+right <= 1 per bend slot | §8.4 | not built | no hit | bend slots (free bends) |
| Shared switches per bundle stretch | §8.4 | unknown: switches removed in DESIGN-pieces step 3; whether any remain not checked | - | - |
| Keep rows local / measure box-plus-track score instead of dense outline | §8.4, D336 | rejected for the score (outline stays, D336) | DECISIONS D336 | - |
| Variant symmetry cut (mirror pairs, dead candidate bits) | §8.4 | not built | no hit | variant list |
| Whole-hub tile-grid solve | §8.4 "not adopted" | rejected (D281's freer joint model did not finish in 18 min) | DESIGN-tracks §8.4 | - |
| Pieces model: required pieces, curve rules as gap sums, no switches/caps | DESIGN-pieces §2-§4, D335 | built | planar_model.cpp `AddPiece`, `PieceCurves` (STATUS: 1b 1abf2b91b1) | - |
| Candidate steps as yes/no variant bits (zero-length jogs forbidden) | DESIGN-pieces §3, D335 | built | `steps` PC:751 | superseded in intent by D338 once built |
| Gaps between neighbouring channels (jog rule) | D335 | built per STATUS; not re-verified | STATUS 2026-11-17 | - |
| Grid lives in the geometry layer (legality by construction) | D334 | partly built (grid rules in pieces); the grid layer idea itself not built | DESIGN-pieces §2 | grid layer |
| Doubled lane reserves two lanes in the channel | D333 | unknown: composer reserves copy lanes (ladder copies PC:758) but width accounting not checked | - | slots |
| Doubling as a layout decision (D309); doubling policy by throughput, D331/D332 | D309, D331, D332 | unknown: doubled copies exist in composer; no policy code found with a throughput knob | grep "throughput" planar/ finds nothing | user options |
| Components inside bundles, atomic primitives only (no Block227) | D310, D330 | partly built: composer uses primitives; fixed shapes remain (see hit list) | PC:307 fray, 428 Flex | shape layer |
| Blocks as constraint contributors on role-tagged pieces, not by index | DESIGN-tracks §2.3, §4 step 4 | not built | components still index pieces (`segs[lanes[j]][4]` PC:445) | slots |
| Region check on T2/T3/LL² (step 1 of §4) | §4 | built as spike | wip/SPIKE-regions2.md (253 sides, 0 contradictions) | - |
| Regions at a doubled bridge (four tracks wide); one-lane merge stacking order | §5 Open | not built / open | - | slots |

## Search and scaling
| idea | source | status | evidence | depends on |
|---|---|---|---|---|
| Variant local search past the 6-bit cap (one-bit flips, bounded threads) | §14, e4b50580e1 | built | PC:2886-2940; SEARCH_THREADS PC:52 | - |
| Better search starts: LP ranking, narrow-hub winner, today's defaults; two-bit and perturbation moves | §14 "offline work", STATUS 3b/4e | not built (design note only; "no consumer until new bits land") | search starts from all-zero PC:2890 | offline sweeps |
| Offline full sweeps for ground truth, bit interactions | §14, engineer 2026-11-18 | not built | wip/localsearch/ls.py is a driver only | - |
| Prune variants by LP-relaxation bound (exact branch-and-bound) | §10.1 | not built | no hit | rough/LP solve |
| Rank variants by default-shape solve; program-shape solve only on the top few | STATUS timing correction 2026-11-18 | not built (plan) | both solves per variant PC:2585-2594 | ranking |
| Warm start of ladder solve from the rough solution | §10.2 | built | PC:2640 `M.last.assign`; planar_model.h:534; STATUS 7073aacd56 | - |
| Search near the rough layout (windowed bounds, widen on edge) | §10.3 | not built | no hit | tight big-M |
| Better layouts by repeat rough->rungs->re-solve while area falls | §10.4 | partly built: two fixed passes | PC:2627 `pass < 2` | - |
| Windowed local re-solve (free one window at a time) | §10.4 | not built | no hit | grid or ladder windows |
| Choices decided in the rough solve and fixed in the refining one (over/under, ties, lane splits) | §10.5 | not built | no hit | - |
| Multilevel lift: narrow hub solved, decisions lifted to wide hub | §14, engineer agreed | not built | no "lift" in planar/ | topology families, n-lane composer |
| Topology family = skeleton + widths (LL³ as widening of LL²; check variant bits match) | §14 | not built | SPIKE-LL3: no variant builds (two-lane refusal PC:1261) | n-lane composer |
| n-lane composer (lift the two-lane limit) | STATUS NEXT (3); SPIKE-LL3 | not built | PC:1261 `full != 2` refusal | - |
| LL³ lane-paired variant, 11 open crossings, LLL² semantics | D323, D325, D326 (parked) | not built (parked) | STATUS "LLL² PARKED" | n-lane composer |
| Main line first (higher turn cost for main lines in the shape program; main-line crossing bits first in search; bits ranked by impact) | §14 Later, engineer 2026-11-18 | not built | no hit | local search consumer |
| Rebuild LL² tag list (misses best variant o49t0f1s0) | §12 S4 offline check, STATUS Later | not built | wip/ll2-tags.txt stale | - |
| Over/under decided once; LL² three exits are rotations (6 bits off) | STATUS 2026-11-17 NEXT | unknown: status not tracked after local search landed | - | - |
| Triangle flips stay variants | §12 S4 | built | `flips` PC:750, FlipPlanarTriangles PC:2888 | - |
| Interleaving derives crossings; embedding fixed by topology | D305-D307 | built | planar_topology (CLAUDE.md planar) | - |

## Solve speed
| idea | source | status | evidence | depends on |
|---|---|---|---|---|
| Per-variant deadline | d2ee981bcc, BRIEF-deadline | built | planar_model.h:537-555; PC:2037 | - |
| Hover < 100 ms from seed binaries fixed; seed < 5 s | DESIGN-planar §3, D297 | partly built: seed on flat/terrain (planar_plan.cpp:299-333); gate flakes show hover near_max 107.9 ms under load | STATUS "gate flakes" | - |
| One-lane T seed 616 s | STATUS Later | not built (open defect) | STATUS | - |
| Shape-program regression (two solves/variant, 66 -> 504 ms on T2) | STATUS timing correction | not built (plan: rank, then program shape on top few) | PC:2585-2594 | ranking |
| LL² solve 74 s (listing 8192 variants) | STATUS | open; see search theme | - | search |
| Gate at ~30 s, over budget | STATUS Later | open | STATUS | - |
| DRY mode on bounded threads (segfault under 8192 threads; CPU suspect) | STATUS 4b | not built (low priority; ASan did not reproduce) | PC:2886 DRY handled apart from the search | - |
| Fix optional bends after one free solve; integral-LP clash rounds (~1 s a variant) | STATUS 2026-11-16 (parked, "chasing our tail") | rejected/parked (different local search; LL² worse) | STATUS line 179 | - |

## Prio
| idea | source | status | evidence | depends on |
|---|---|---|---|---|
| P1: CheckPrio clause 1 per join piece; drop single-row and axis-only clauses | §13 P1, D342 | built | junction_rules.cpp:1213 CheckPrio; STATUS 2cb6129623 | - |
| P2: prio signals walk the ML pieces; prio joins and signals by search judged by the checker | §13 P2, D345 | built (P2s 0958786f36) | signals/junction_signals.cpp:107-110, 418-420 | - |
| P3: composer stops pinning the ML under a prio straight (ML an ordinary bendable stretch) | §13 P3, STATUS NEXT (1) | not built | PC:246 fixed `AddSeg(...2P+2)`; PC:369/980 | row model |
| Prio row as a gapped offset copy of the lane's coverage pieces (row model) | §13.1 | not built (probes failed to place) | PC:293/409 fixed rows; PROBE-prio-row, PROBE-prio-spread | slots or free bends |
| Prio rows are real bundle lines; bundle widens like a doubled copy; neighbours bend around the prio | D349 | not built | STATUS top line; SPIKE-slots run 1 IIS (rows vs port pins) | slots, free bends |
| Retire prio "reservation": row becomes real track in the composer | §13.1 "Reservation revisited" | not built | PrioRow struct planar_model.h:198 still a reservation | D349 |
| Freed multi-lane prio join: options (a) row gaps beside neighbour's copy, (b) fan wider, (c) inner copy steps toward joiner, (d) keep stagger | STATUS top (engineer decision pending) | not built; awaiting engineer by eye | PROBE-prio-row/-spread: (a) A3 failed, (b) A4 failed | engineer ruling |
| Prio coverage over the main line's own doubled bridge | D347, D347b | partly built as probes only; A2 composer -21% but no row room | PROBE-prio-bridge2 | row model |
| Prio crossing over/under as a searched bit (start main-line-over) | D347b | partly built: channel-level crossings are open bits and prio-over is the default; channel-level under never wins (probe C); join-level joiner-over fails (probe B) | PC:1517 PLANAR_NOUNDER; PROBE-prio-open, PROBE-joiner-over | row model |
| "Bridge the prio line" user toggle | D347 | superseded by D347b | DECISIONS D347b | - |
| Prios that bend (row follows lane through bends; signal spacing along the curve) | §8.6, D342, engineer "Even Prios can bend" | not built | no hit; P1/P2 only free the checker and signals | row model |
| Prio stagger as T3's length (chained coverages) | PROBE-prio-stagger | open finding; T3 137x38 vs old 128x44 | §12 rig verdict | prio freedom |
| Prio side assumption (opposite the joiner) | D311(2) | built | prio_rows side `-sigma` PC:293/409 | - |

## Bridges and coop rules
| idea | source | status | evidence | depends on |
|---|---|---|---|---|
| Bridge-length threshold: break up by a signalled tile or triple | D346, RESEARCH-bridge-length | not built; rule found, break-up vs triple unsourced | wip/RESEARCH-bridge-length.md facts 1-4 | rig test of the rule |
| Bridge rule rig test | STATUS Later | not built | STATUS | rig |
| Series bridges with cap (~2 crossers / 4 middle) | D209 | legacy-era; planar status unknown | no hit in planar/ | D346 |
| Tunnels | D323 (3) "no tunnels in LLL", old pipeline TU1-TU4 | not built in planar for hubs; terrain-use only in old pipeline | grep "tunnel" planar_components/planar_layout: none | terrain |
| Doubling policy knob (by throughput) | D331, D332 | unknown (see layers theme) | - | - |
| Bridge over bridge / tunnel under tunnel forbidden | DESIGN-planar §2 | unknown: not grepped | - | - |
| Prio-aware checker with lane-path prio (D319 patch) | D319 | unknown: patch was "not yet committed in C++" at 2026-11-16; checker now has per-piece CheckPrio | wip/checker-d301-mainline.patch | - |
| Join type: fewer block types, one Join and one Fray from primitives | D311 | partly built | PC fray/join composer | - |
| 2x2 diagonal cross | D311 "come back to this later" | not built | - | - |
| Throughput / balancing patterns | DESIGN-modules §3/§7, D279 | legacy-era; not in planar | - | - |

## Topology and builder UI
| idea | source | status | evidence | depends on |
|---|---|---|---|---|
| Topology complete artefact owning crossing order | D343 | partly built: text format carries ports, nodes, prio; event order derived from chords | planar CLAUDE.md "Event order" | - |
| Crossing order pinned or open (default open) | D344 | unknown: no code hit for a pin syntax | grep planar_topology: none | - |
| Topology builder UI; topologies shipped as NewGRFs | D343 (engineer's envisaged), §10.6 | not built | no hit in planar/ | topology data format |
| Topology window and presets for game hub types | D222 (TP1), DESIGN-planar §4 (client setting) | legacy-era built; planar mapping "T first, then LL²": unknown for current game wiring | - | - |
| Interactive port moves: last layout's rungs as rough start | §10.6 | not built | no hit | rough start API |
| Organic hubs: ports free along a boundary or diagonal arms, non-rectangular score, chord drawing as rough start | §8.7, STATUS Later 2026-11-17, D313 | not built | axis-only/box refusals PC:1250/1256 | shape layer, grid layer |
| User score options (square, thin, cheapest) | D329, §8.7 | not built | PLANAR_OBJ experiment env only (PC:2176) | - |
| Corpus growth: represent more PSG hubs. Goal (engineer, 2026-11-19): "recreate any old hub from first principles - or at least build a hub 'in the style of mfb'". Under SSS (D362) a hub is its topology plus its Structure decisions (bridge or tunnel per crossing, form per joiner, straight legs) plus component cross-sections (227 three-input joins, 197 split-then-tunnel bundles, MFB bridge stacks); a style is a preset over those. Recreation test: read a save's topology and Structure decisions (mapx exports, piece graphs, REPRESENT-psg227), run Shape/Slots/Lengths, compare with the original; a gap names a missing layer promise | D270, D362 | not built in planar | - | SSS layers, n-lane, tunnels as an under form |
| PSG 193 hub (V453000), mfb 227 | STATUS 2026-11-15 queue | 227 built (LL²); 193 unknown | - | - |

## Terrain
| idea | source | status | evidence | depends on |
|---|---|---|---|---|
| Terrain repairs ranked by cost: terraform the ramp tiles vs change the shape (ramp straight + S-bend), both generated from a named height failure | D196/D199 (removed 2026-11-18 as old-pipeline mechanisms; idea kept here) | not built on planar | - | terrain on planar |
| User weight between organic (shape change) and artificial (terraform) | D199 (removed 2026-11-18; idea kept here) | not built | - | terrain repairs |
| Seed on flat ground, then terrain facts as lazy constraints with the seed's binaries as start | DESIGN-planar §3 | partly built | planar_plan.cpp:299-335 | - |
| Hover: seed binaries fixed, positions solved at the cursor | DESIGN-planar §3 | partly built (hover param); timing flakes | planar_plan.cpp:299 `hover`; STATUS | - |
| Terrain in the grid layer (deferred like clashes) | §11 | not built | - | grid layer |
| Reuse of old terrain facts, level solve, signals, pricing, build | DESIGN-planar §4 | built | planar_plan.cpp:16 | - |

## Rig and tests
| idea | source | status | evidence | depends on |
|---|---|---|---|---|
| `pv check` baselines per tag | STATUS | built | tools/junction/pv; wip/pv-baselines/ | - |
| LL² tag list rebuilt from the full built set | §12 | not built | see search theme | - |
| LL² mapped test area 807 stale (needs PLANAR_ONLY) | STATUS ladders note | not built (open defect) | STATUS | - |
| Hidden-test oracle: signals and checker on mapped dump | STATUS 2026-11-16 | built | PLANAR_MAPPED_DUMP | - |
| Rig in-game side-by-side old vs new (T2/T3/LL²) | §12 rig verdict | built (done once); repeats per landing step by engineer's eye | STATUS RESUME 2026-11-18 | - |
| Lane-path text topologies in wip/topologies/ | wip/topologies | built (files) | ls wip | - |
| ASan / CPU instability proof | memory cpu-instability | open | STATUS 4b | - |

## Legacy-era ideas (old pipeline, G*/N3/O4/blocks/modules): skimmed by headings only
Not carried over unless one of the planar rows above names it. Mentioned for completeness; none assessed against planar code.
- DESIGN-G1 (lane programs, series bridges D209, §11 placement), G2 general graph (G2.4-G2.6 doubled out edges, S-bend roles), G3 vectors (joint solve, clash rows), G4 static (static rows, item rows O1-O3, 2b2a/2b2b steps; stopped by D276), N3, O4: superseded by the planar solver (D305-D309); old pipeline "stays untouched and keeps passing its tests" (DESIGN-planar).
- DESIGN-blocks (B2 join block 227, B3 exit block, H1 composition), DESIGN-modules (blocks by template, balancing patterns D279, throughput D279, order of work D289): superseded by components inside bundles (D310, D330, "no composite blocks").
- DESIGN-width (width-2 T, R9-R13, D292-D295): superseded by pieces/channels (D326-D327).

## Items I could not classify
- D331/D332 doubling by throughput, D333 width reservation, D344 pin syntax, the 135-degree no-U-turn row, "over/under decided once" for LL²: no obvious code mechanism by grep and no later note; each needs a targeted check.
