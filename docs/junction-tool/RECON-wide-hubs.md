# RECON: wider hubs stop at two refusals

Read-only recon at a629c61dd5, pairing any, `MakeTestJunctionSpec()`, hubs built as in the hidden test `[.g3scale]` (`src/tests/junction/placement/junction_joint.cpp`). Dumps came from a temporary hidden test and a temporary `fail()` message carrying edge ids, both reverted (nothing of source changed); raw dumps are in the scratchpad `rwide/` (dump.txt, dump2.txt). Edge/node ids below are from those dumps.

Hub state today (unchanged source): L_R x L_R, LL_RR x L_R, LL_RR x LL_RR, L_R x {LL_RR, LLL_RRR, LLLL_RRRR} pass structure and programs. Stopped: LLL_RRR x L_R and LLLL_RRRR x L_R at programs (refusal 1); LLL_RRR x LL_RR, LLL_RRR x LLL_RRR, LLLL_RRRR x LLLL_RRRR (also LLL_RRR x LLLL_RRRR, LLLL_RRRR x LL_RR) at structure (refusal 2).

Vocabulary: row r has level 2r+1; band b has level 2b (band b is north of row b). In LLL_RRR rows 0-2 run east (W in, E out), rows 3-5 west; the centre band is level 6. A "joiner" is an edge whose paths come from a branch slot.

## 1. "a diverging joiner that crosses is not ruled yet" (programs stage)

### (a) Where
`src/junction/topology/junction_topology.cpp:1171` in `ProgramJunctionTrackGraph`, band-edge step "a. Start":

```
if (is_band_node(edge.from, true) && diverges(e, edge.from.index, side_hand)) {   // :1169
    if (is_row_node(edge.to)) {
        if (!crossings.empty()) return fail("a diverging joiner that crosses ...");  // :1171
```
So: the edge starts at a split node that is in a band (even level), it is the non-straight child of a two-child split (`diverges`: straight child = the one whose far end has the smaller level, tie west; D-less G1 §10.2 rule), it ends at a row node (it is a joiner arriving on a main lane), and `order.edge_crossings[e]` is non-empty.

### (b) Triggers
- LLL_RRR x L_R: edge 21 = N5 -> N20, kind Leg, path 10 (branch in slot 6 to E out slot 12, row 1). N5 is a split of slot 6 at level 6 (centre band); N20 is the merge of slot 12 on row 1 (level 3). Its sibling edge 20 (N5 -> N21, row 0, level 1) is the straight child (farther end, smaller level). Edge 21 crosses edge 16 = the row-2 through lane (N2 -> N19, both level 5, a row edge). Only that one crossing.
- LLLL_RRRR x L_R: edge 24 = N9 -> N16, path 9 (branch in slot 8 to W out slot 5, row 5). N9 is a split of slot 8 at level 16 (band 8, south of the last row); N16 is the merge of slot 5 at level 11. It crosses edges 34 and 33, the through lanes of rows 7 and 6 (level 15 and 13, both row edges).
- Common shape: a branch leg that diverges from the branch trunk at the band node nearest the S port, and must cross one or more far-side through lanes before reaching its own row. This is a "joiner crossing through lanes", legal in the narrow cases only because there the diverging joiner lies in the band adjacent to its row (no row between), so `crossings` is empty.

### (c) The nearest ruled case and the rule
Nearest ruled cases: (i) diverging joiner, no crossings (T, LL_RR x L_R): the diverge IS the arrival: the edge starts with the row's Corner (hand must equal the side hand, else the separate refusal at :1174), `corner_placed`, then step "c" adds only the Converge S-bend; (ii) a non-diverging joiner with crossings: step "b" emits Over/Under/SBend+Under per crossing and step "c" emits Corner then Converge S-bend; (iii) a diverging non-joiner (edge to a non-row end) starts with `SBend(Diverge)` (:1177).

Reason for the refusal: the start Corner of case (i) turns onto the row heading at the approach item. With crossings between the band node and the row, a Corner at the start would turn before the crossings, which is geometrically wrong (crossing row 2 is travelled before turning onto row 1).

Rule candidate: a diverging joiner with crossings starts as any diverging edge does, by `SBend(Diverge)` on side_hand; its crossings are emitted by step "b" exactly as for a non-diverging joiner; its arrival (Corner on the approach item, Converge S-bend) is step "c" unchanged. The Corner-at-start form is kept only for the case with no crossings, where it is the same corner.

Experiment (temporary, reverted): replacing the refusal by exactly that (SBend Diverge, corner_placed stays false) lets both hubs pass the programs stage and reach seed placement, where they behave as the already-ruled LL_RR x L_R baseline: seed fails, joint solve status 1 (infeasible, relaxed also infeasible), conflict chain 15 (LLL_RRR x L_R) and 48 (LLLL_RRRR x L_R) against 8 for LL_RR x L_R. So the programs rule is not what separates wide hubs from placement; the downstream failure is shared with LL_RR x L_R and is a placement/joint matter, not part of this refusal. Not checked: whether the placed geometry of the diverging joiner is right, nor whether step "b"'s `Under`/`SBend`+`Under` pair is right for a crossing that now follows an S-bend (it was written for a straight run); the checker must prove that once placement works.

### (d) Prior decisions
- DESIGN-G1.md:123-125 states the refusal as a deliberate scope limit ("a diverging joiner that crosses"), with "any other diverging child starts with an SBend" at :123; D75/D87 (prio opposite the approach side), D203(1) (S-bends to open rows for joiner approaches), D204 (branch-in split by direction, then lane, next to the main line it feeds), D223/D227 (over/under at crossings), D229 (merge-then-bridge on a crossed lane), D230 (S-port fans). No decision rules the crossing diverging joiner itself.
- DESIGN-G2-general-graph.md:46 defines Diverge as "the split at the edge's start", which supports reading the S-bend start as the general form.

### (e) Coop practice
RECON-junctionary-crossings.md (Junctionary/coop images, on OneDrive, already pulled, not re-fetched): all joiner crossings in the clear Branch-Merge images are tunnel portals under the through lines or bridges over opposing mains; no detail of how a branch leg leaves its trunk before crossing. Practice supports the shape (branch legs cross the far-side lanes), says nothing about the start of the leg.

### Rule candidate
A diverging joiner starts with `SBend(Diverge)`, crosses per step "b" as a non-diverging joiner would, and arrives per step "c"; the start-Corner form is the special case with zero crossings.

### Decision needed from Pete: no
It is a program-token ordering inside already ruled crossing semantics (D223/D227, D203(1)); no new layout preference is created. Reversible and checkable; D227-style default, to be recorded when built. Escalate only if the placed geometry of the crossing-after-S-bend proves to need a layout choice.

## 2. "two joiners that meet the main line at one level are not ruled yet" (structure stage)

### (a) Where
`junction_topology.cpp:944` in `StructureJunctionTrackGraph`, in the loop over `graph.crossings` for two non-row edges, neither carrying a through path, both `from_branch`:

```
if (bi) {                                                    // both joiners
    if (near_end(i) == near_end(j)) return fail("two joiners that meet the main line at one level ...");   // :944
    i_over = near_end(i) > near_end(j);                      // D227 C3 (a)
```
`near_end(e) = min(end_level(from), end_level(to))`. The tie is a refusal; C3 (D227) gives no tie-break.

### (b) Triggers
All three are two edges lying wholly inside the one centre band (every end node at that band's level), crossing each other there:
- LLL_RRR x LL_RR: edges 13 and 39, near_end 6. Edge 13 = N12 -> N13, kind In, paths 18,19, in the split tree of branch in slot 7 (both nodes level 6). Edge 39 = N7 -> N39, kind Leg, path 17, from slot 6's tree node N7 (level 6) to the merge N39 of E out slot 13 (row 2, level 6).
- LLL_RRR x LLL_RRR: edges 16 and 56, near_end 6. Edge 16 = N15 -> N16, kind In, paths 24,25 (slot 7's tree, levels 6, 6); edge 56 = N10 -> N58, kind Leg, path 23 (slot 6 tree node N10, level 6, to the merge N58 of slot 15, level 6).
- LLLL_RRRR x LLLL_RRRR: edges 18 and 26, near_end 8. Both kind In (edge 18 = N17 -> N18 paths 38,39; edge 26 = N24 -> N26 paths 40,41), levels 8, 8; edge 18 also crosses 33 and 40.
(Also LLL_RRR x LLLL_RRRR 19/73 near_end 6 and LLLL_RRRR x LL_RR 10/18 near_end 8: same shape.)
Shape: a branch lane's own turning legs cross the neighbouring branch lane's legs inside the band where the two directions split (D204); adjacent lanes of a multi-lane branch cross there because lane k's far-direction leg crosses lane k+1's near-direction leg. It appears only from three lanes (LLL x LL) up: LL_RR x LL_RR has the same crossing pairs but each end lies at different levels (its band nodes sit at levels 4 and 8), so C3 decides them.

### (c) Nearest ruled case and the rule
Ruled: two joiners whose near ends differ (LL_RR x LL_RR): the one meeting the main line farther from it goes over, `bridges.push_back({i_over ? i : j, {...}, false})`. CARDS-crossings.md C3 lists option (c), "the order of the two edges at the crossing within the band", as the third option beside the adopted (a); (c) is exactly what a tie needs.

Rule candidate: the near end decides (D227 C3 a); on a tie, break it by the order of the two edges within the band (the sweep order, `order.bands`), here equivalently by the lane order of their branch in slots: the edge from the lower (west) in slot goes over. Any fixed total order works for correctness; it must be the same order everywhere so the bridge pair is consistent.

Experiment (temporary, reverted): a hard-coded tie-break in either direction (i over, or j over) lets all three hubs pass structure, and the two orientations behaved identically in the scale probe. Next stops then are: LLL_RRR x LL_RR passes programs and stops in placement with "a bridge over several band crossings is not ruled yet" (`src/junction/placement/junction_placement.cpp:275`, DESIGN-G2 F3: a band edge carries one bridge over one crossing); LLL_RRR x LLL_RRR and LLLL_RRRR x LLLL_RRRR stop at programs with "a crossed lane in edge is not ruled yet" (`junction_topology.cpp:1056`, DESIGN-G2 F2). So this refusal is the first of three; clearing it alone does not place anything.

### (d) Prior decisions
D223 (joiner x joiner: the one farther from the main line goes over), D227 C3 (a) (the joiner whose main-line end is farther goes over; "kept in mind as a toggle"), CARDS-crossings.md C3 (options a/b/c, reversible), D224 (one joiner per main lane; legs merge first), D204, D205, D229. DESIGN-G2-general-graph.md F2/F3 name the other two limits above.

### (e) Coop practice
RECON-junctionary-crossings.md C3: NO EVIDENCE from the Junctionary/coop images for joiner x joiner order (tunnels or three stacked decks, per-joiner order not resolvable; Psg197_bbh01 shows parallel pairs, one on a deck and one on the ground). So practice neither supports nor contradicts a tie-break; D220 (tunnels, "especially towards LLL_RRR") is the coop-consistent way to resolve crossings of this density later by cost.

### Rule candidate
Joiner x joiner tie on near_end: lower branch in slot (west lane) goes over; recorded as a D227-style default, toggle candidate.

### Decision needed from Pete: no
A tie-break between two equal-level crossings is a reversible default of the C3 kind already delegated in D227 ("your judgement on those defaults ... keep them in mind for toggles"); the experiment shows the orientation does not change which hubs pass. Pete's call is only needed on the downstream F2/F3 limits (a bridge over several band crossings, a crossed lane in edge), which are placement/program work with their own cards, not this refusal.
