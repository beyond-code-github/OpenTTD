# V's PSG 197 with portal-only end runs: domain breaks, and the judge's refusals triaged (domain-41; D393, D436)

Head 4284cb5bf3 plus the test edits of this step. Reproduce: `PLANAR_DOMAIN_JUDGE_197=<path> PLANAR_DOMAIN_DUMP_197=<path> build-rel/openttd_test '[planardomain197]'` (the report goes to `answerkey-197.txt`; the judge file holds every refusing row with its terms and values, once as the judge is and once with end run 1 in the judge as a counterfactual). Points are half tiles in the shifted frame; game tile = point / 2 + (78, 137). Row sums are in the program's grid units (a `half` variable is one tile of the row's axis; a row's bound is rounded inward on the grid).

## 1. The answer key with ends = 1

The 197 test's domain input now gives every structure `end_run` 1 (a copy of the variant, the judge keeps the planner's end run 2; reason: wip/CARD-tunnel-slopes.md, triage-197.md cause A). The domain breaks go from 55 to 20:

| rule | count | class |
|---|---|---|
| node | 4 | ARTEFACT: box edge L94/L96 (2), L69/L70 node (2) |
| connection | 2 | ARTEFACT: box edge L94/L96 (1), L69/L70 node (1) |
| head | 7 | NEW (below) |
| missing crossing | 7 | NEW (below) |

The 6 artefacts are exactly the triage's artefacts 1 and 2 (same tiles, same ours/V differences: L94/L96 at game (128..130,173..174); L69/L70 at (104,179)). The triage's artefact 3 (diamond L62xL60 at (98,180)) is gone; every span, diamond and the earlier connection/head/missing-crossing breaks are gone with it.

NEW, in full (all nine tiles identical to V's, so these are V's own tiles, not the rebuild's): one cause, 14 breaks in 7 pairs. A tunnel's head tile lies directly above the middle of a perpendicular tunnel. The domain's `head` rule ("an end piece shares its tile with another piece") fires on the head tile, and the `missing crossing` rule fires because the crosser's piece on that tile is a Head, not Ground ("never passes under a middle"). The save has all of them, so the game allows it.
- L11 tunnel head (92,171) over L47's middle; L12 head (92,172) over L47's middle: head L11/L47 (shifted (14,34)), head L12/L47 ((14,35)); missing crossing L11/L47 and L12/L47 at (92,163).
- L11 head (95,169) and L12 head (95,170) over L57's middle: head L11/L57 ((17,32)), head L12/L57 ((17,33)); missing crossing L11/L57, L12/L57 at (95,159).
- L45 head (93,166) over L48's middle: head L45/L48 ((15,29)); missing crossing L45/L48 at (93,163).
- L45 head (96,166) over L59's middle, L46 head (96,167) over L59's middle: head L45/L59 ((18,29)), head L46/L59 ((18,30)); missing crossing L45/L59 and L46/L59 at (96,160).
This is a domain gap (too strict against the game, V's save the evidence), not an artefact: it was masked while the 2-piece end runs dropped these structures. Not fixed here (the brief forbids it).

## 2. The judge's refusals (35 rows) after naming every family

`RULE_NAMES` now names deck, node, port order and span; no refusal shows as `other`. Refusals by family: end run 17, keep apart 10, deck 2, node 2, grid 2, node gap 1, port order 1.

Counts by class: PLANNER RULE 30, ARTEFACT 5, TOO STRICT 0.

| family | PLANNER RULE | ARTEFACT | TOO STRICT |
|---|---|---|---|
| end run | 17 | 0 | 0 |
| keep apart | 10 | 0 | 0 |
| deck | 2 | 0 | 0 |
| port order | 1 | 0 | 0 |
| node | 0 | 2 | 0 |
| grid | 0 | 2 | 0 |
| node gap | 0 | 1 | 0 |

### End run: PLANNER RULE (17 rows). Rule: a tunnel's end run is portal plus ramp = 2 (D155, D365), the card's option A.
Test: the same judge with end run 1 for every structure (counterfactual in the judge file) refuses none of these 17; its refusals are exactly the other 18. V's crossing sits 1 tile (or 2 with a diagonal crosser) from the head, the row asks for end run 2 plus the diagonal half tile (and, for `node` rows, one tile off the node's switch tile, D428 probe). Row: V's sum vs bound (grid units), separator of structure s.
- end 9 (L48): 2 vs 3. end 15 (L80): 2 vs 3. end 15 node (L80): 3 vs 4. end 16 node (L71): 3 vs 4 (two rows). end 17 (L45): 2 vs 3. end 17 node (L45): 3 vs 4. end 20 (L46): 2 vs 3. end 22 (L11): 1 vs 2. end 25 (L12): 1 vs 2. end 28 (L11): 2 vs 3 (two rows). end 30 (L12): 2 vs 3. end 32 (L57): 1 vs 2. end 33 (L59): 1 vs 2. end 34 (L60): 1 vs 2. end 40 (L48): 1 vs 2. end 45 (L49): 1 vs 2. end 48 (L70): 1 vs 2.

### Keep apart, deck, port order: PLANNER RULE (13 rows), one root: a crosser runs along its separator's line
V's surface line crosses a tunnel by an S-bend whose straight piece lies directly over the tunnel: L40 over L57 (tiles (95,166..168): Right, Y, Left, the Y piece on L57's column, over the tunnel's middle) and L89 over L74 (tiles (112..114,175), X pieces on L74's row). In the contracted drawing the crossing piece is parallel to, and on the line of, its separator (L40#1 (35,62)-(35,60) on L57's x = 35; L89#1 (74,77)-(68,77) on L74's y = 77). The domain accepts it (the tunnel is underground there; no break on those tiles). Our rule: a crosser crosses its separator at 90 or 45 degrees under one angle per span (D376, H11), crossings are points on a transversal piece, and walls of a face keep spacing unless adjacent (D381, D428). The rows refuse because the walls overlap; no row is wrong against its own statement, which does not cover a crosser lying on its separator.
- sep 57 2 4, sep 95 1 3, sep 119 1 6, sep 119 2 6, sep 120 0 3, sep 120 1 3 (L40, L57): each fails every candidate side because L40#1/#2's points have x = 35 = L57's x (spacing needed 1 or 2 half tiles, value 0).
- sep 141 3 6, sep 141 4 6, sep 155 4 6, sep 155 4 7 (L89, L74): same, L89#1/#2 on y = 77 = L74's y (value 0, or -2 for the diagonal piece).
- order L57 0 and deck 1 (one inequality): crossings L40xL57 (parameter 13) and L46xL57 (12) must keep the event order with gap >= 0; V has L46 half a tile before L40's crossing point (sum -1), because L40's crossing point on its overlay piece is arbitrary within 3 tiles.
- deck 20: L46's crossers L40xL46 and L46xL57 both at parameter 3 (gap needed 1): L40 and L57 are the same line there.
Candidate for a ruling (not a finding of a too-strict row): the game allows a surface line to run along a tunnel's line over it; D376 does not. If Pete wants it, the rule is "a crosser may lie on a tunnel separator's line over its middle tiles" and these 13 rows would need the overlay excluded.

### ARTEFACT (5 rows)
- node 47, grid `integer half` and `bound half`, node gap `leg zero L101` (4 rows): the L69/L70 split at game (104,179) (node 47 = L69 and L70 from, L101 to). V's step/split sits one piece earlier: V's end points are (53,86) and (54,85), a half tile apart, so the node's grid point is 26.5/42.5 (not an integer) and L101's leg is -0.5 (bound -1). Triage artefact 2 (and the domain's node and connection breaks of section 1).
- node 61 (1 row, two lines): the L94/L96 split (node 61 = L94 and L96 from, L99 to) at the box edge, game (128..130,173..174): V's lines continue past the box edge, ours stop at the port, so the ends differ by one tile (51 vs 50). Triage artefact 1 ("end pins not met: 2").

### TOO STRICT: none by the brief's test
No row refuses a geometry that its rule's own statement allows and the domain accepts. The two things worth Pete's attention are not too-strict rows: the 7 NEW domain breaks (a domain rule too strict against V's save: tunnel head over another tunnel's middle) and the ruling candidate above (crosser along a tunnel's line).

## 3. Every refusing row

(tag: class, family, reason)
- end 9, 15, 15 node, 16 node, 17, 17 node, 20, 22, 25, 28, 30, 32, 33, 34, 40, 45, 48: PLANNER RULE, end run, end run 2 (D155/D365, card).
- sep 57 2 4, sep 95 1 3, sep 119 1 6, sep 119 2 6, sep 120 0 3, sep 120 1 3: PLANNER RULE, keep apart, L40 over L57 (D376, D381, D428).
- sep 141 3 6, sep 141 4 6, sep 155 4 6, sep 155 4 7: PLANNER RULE, keep apart, L89 over L74 (same).
- deck 1: PLANNER RULE, deck, L40/L46 order over L57 (same). deck 20: PLANNER RULE, deck, L40 on L57's line at L46 (same).
- order L57 0: PLANNER RULE, port order, same inequality as deck 1.
- node 47, node 61: ARTEFACT, node (triage artefacts 2 and 1).
- integer half, bound half: ARTEFACT, grid (the L69/L70 node, artefact 2).
- leg zero L101: ARTEFACT, node gap (the L69/L70 node, artefact 2).
