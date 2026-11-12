# RECON IG5: why half the sloped straights still end flat (launch HEAD 0b1d7ba12c, worktree .claude/nodes, branch wt-ig5)

Experiment patch (junction_levels.cpp instrumentation + hidden test `[.ig5]` in junction_real_terrain.cpp): /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/ig5.patch. Raw outputs and logs: scratchpad/ig5/ (run-*.txt, log-cls-*.txt, plans-*.txt, cmp.py). Run: `IG5_STEEP=4 IG5_MODE=0 [IG5_CLASSIFY=1 IG5_OUT=f] IG5_PLANS=f build-rel/openttd_test "[.ig5]"`.

## Method
- Release; the 48 fixtures x the 153 near offsets of LEVELHASH (7344 offsets; PlanJunctionHover, no search), test spec tl 4. Planned 2861 (2862 at steepness 0). HOVERNEAR from PlanJunctionHoverNear at the centred offsets.
- ramp_levels: steepness 0 gives 4 and 3-5 give 5, 4: all >= tl (4), where SolveTileLevels does not apply the ramp rule at all (`ramp_levels < train_length`, junction_levels.cpp). Steepness 4 gives 3 (< tl), the only strong-train case where the rule binds. Steepness 0 and 4 give the same classes within 0.2 %, so IG1's 53-57 % was measured with the rule OFF or loose.
- Candidate = lone-axis straight tile, not a bridge head, ground not flat: 88370 over the sweep (IG1 definition incl. heads/nodes: 63.6 % of 93836 sloped straight tiles end flat; 38.7 % of the 88370 candidates end as ramps).
- Each flat-ending candidate whose ground is an incline along its axis is re-solved with its ramp FORCED (levels of tile and next differ by the ground's step; the two equality groups merged with that offset) and the full descent re-run: infeasible = classes (a)/(b) from the reasons of JunctionPositions::Explain; feasible = (c) when the exact cost is not lower, (d) when it is lower. Local exhaustive search: the 7 route tiles around the candidate, each group at current +-2 (<= 5^7), exact cost, difference constraints checked.

## 1. Classes (steepness 4, 54171 candidates end flat; steepness 0 is the same to 0.2 %)
| class | tiles | share |
| ground is not an incline along the axis (cross incline 15922, one corner raised 12802, three corners 9562, steep 2442): ramp impossible without terraform; ends on a foundation 11150, by terraform 29578 | 40728 | 75 % |
| (a0) the tile is not rampable: a node, port, route end (fixed) | 5445 | 10 % |
| (a) a tie forces it: ramp forced, no feasible levels, reasons only equalities | 6302 | 12 % |
| (c) ramp feasible, dearer under the exact objective (equal: 20) | 1539 | 3 % |
| (d) ramp feasible and cheaper than the plan: the descent missed it | 122 | 0.2 % |
| ramp feasible by levels, the clamp cannot join the corners (terraform conflict) | 15 | 0.03 % |
| (b) ramp rule window | 0 | 0 % |
- (a) in detail: reasons are kind 6 "the corner shared by track tiles is one height" in every case (alone 4330, with 2 "flat tile continues" 1650, with 4/5 251). Tiles around the shared corner (letters F fixed, H head, C curve/diagonal/junction piece, R rampable straight): CRRR 3392, CCHR 543, FRRR 517, HRRR 328, CCR 321, CCFR 224, RRRR 194. So the ramp is refused where its entry or exit corner is shared with a tile that must be flat (curve, node, head): all its corners are one height. That is the game's tile geometry (a flat tile has 4 equal corners), not an arbitrary solver rule, except where the neighbour could be a foundation tile (see 3).
- Along-incline candidates: 34199 ramped vs 7998 flat = 81 % ramp. The terrain is followed where the ground allows it; the flat half is mostly ground that cannot hold a straight ramp.

## 2. Is the cost comparison right (c)?
- READ: Evaluate prices a ramp on its exact incline at clear_cost only and a foundation at clear_cost + foundation_cost; CheckRailSlope (rail_cmd.cpp:407) charges BuildFoundation exactly when the foundation differs, none on an exact incline: right. The terraform is the game's (HC1 test); each dirtied tile adds its clear_cost.
- 3 examples, terms (moves x187 | clear of dirtied tiles | clear/foundation of the other tiles | total), base flat vs forced ramp: (113,40) 28 | 1230 | 5707 | 12173 vs 31 | 1350 | 5490 | 12637; (114,39) 66 | 1860 | 5145 | 19347 vs 77 | 2160 | 5070 | 21629; (115,41) 77 | 2115 | 4995 | 21509 vs 79 | 2130 | 4980 | 21883. The ramp costs 2-11 more corner moves and dirties 1-20 more tiles; the exhaustive search of its window finds nothing below the base (12173, 19347, 21509): these are true (c). Per tile, a ramp is free where the ground fits; the extra cost comes from the neighbours it drags (shared corners).
- Not modelled: the game builds a lone X/Y piece on a one-corner-raised slope or steep ground on an INCLINED foundation (GetRailFoundation: InclinedX/Y, ApplyFoundationToSlope: dz 0, the incline up to the raised corner) at foundation price, a ramp with no terraform. Evaluate offers only an exact incline or terraform there (12802 + 2442 flat-ending tiles). Experiment (mode 8, option added, exact check of a moved foundation tile): ramped tiles 34199 -> 34112, cost +0.19 % on common plans, planned +2: no gain with this descent. INFERRED not proven worthless: the hard ties (kind 6/7) and the single-group descent may stop it using the option. Dropping the kind 6/7 ties crashes (invalid slope, slope_func.h NOT_REACHED): they are load-bearing for exact_cost.

## 3. (d) and the search
- The descent is a coordinate descent over equality groups, each group tried at EVERY level in its feasible range (the brief's "d = 1" is the corner slope limit SLOPE_RANGE, not a move range; d = 2 changes nothing: transitive). It stalls where neighbouring tiles must move together: examples (D): (135,48) base 39595, forced ramp 33138, window exhaustive 32547 (-18 %); (128,55) 23577 vs 22522 (-4.5 %); (132,57) 23712 vs 22081 (-6.9 %). Seen in example 1: a flat plateau at level 2 over ground 1 for 8 tiles (778 each) that no single group move lowers.
- Changes measured (steepness 4; plans-*.txt, cmp.py: common plans):
| change | planned | mean cost on common | better / worse | ms total (7344 offsets) | ms max plan | HOVERNEAR ms mean / max |
| baseline | 2861 | 35796 | | 20.7 s | 20.6 | 22.4 / 80.6 |
| ground-following start (flat start replaced where the entry corners are level) | 2861 | +0.07 % | 19 / 32 | 20.9 s | 21.3 | |
| + interval moves: shift any run of <= 5 consecutive non-fixed tiles by +-1 when cost falls (mode 18) | 2888 (+27, 0 lost) | -3.02 % | 575 / 0 | 78.3 s | 103 | 38.3 / 187 |
| + interval moves: any run, +-1/+-2 (mode 2) | 2899 (+38) | -3.46 % | 615 / 0 | 140 s | 207 | |
(machine loaded by other builds; ratios not absolutes.) The interval move is the only change that helps; both costs exceed the < 100 ms hover target, so a cheaper form is needed (cache the interval costs, or only intervals touching a tile whose neighbour group is tied) before it can ship. Forced re-descent with mode 18: (d) 122 -> 79, so some misses remain.
- The classes do not move: ramped share 38.7 % -> 38.6 %. The interval move lowers cost and wins plans; it does not make the junction less flat.

## 4. Height groups (tiles joined along a route while the level is unchanged), per plan, 2861 plans, 191 track tiles each
| | groups | steps (level changes along routes) | ramp tiles | distinct levels | level range | ground range | largest group |
| baseline | 45.1 | 26.0 | 18.1 | 3.82 | 2.82 | 3.73 | 39 % of tiles |
| interval moves (mode 18) | 45.2 | 26.1 | 18.2 | 3.86 | 2.86 | 3.76 | 39 % |
0 of 2861 plans sit on one plane; the level range follows 76 % of the ground range. The junction steps with the terrain (D218 fan up/down), roughly one ramp tile in ten; no search change moves these numbers.

## Recommendation
1. The "flatness" the engineer sees is mostly the game's slope rules, not the level solve: 75 % of flat-ending sloped straights lie on ground that is not an incline along their axis (X and Y tracks both occur: half the tiles cross the slope), 10 % are nodes/ports; only 15 % could ramp, and 81 % of the along-incline tiles do. No search change alters this.
2. Real levers, in order: (i) offer the inclined foundation (InclinedX/Y) as a ramp on one-corner and steep ground, together with relaxing the kind 6 tie for foundation neighbours: needs its own experiment (mine shows no gain with the current descent); (ii) prefer axis orientations along the incline in the placement (placement, not levels); (iii) the interval move for cost: -3 % cost and +1 % plans for 4x the time.
3. Do not touch the ramp rule window (0 refusals) nor the cost model (checked).
