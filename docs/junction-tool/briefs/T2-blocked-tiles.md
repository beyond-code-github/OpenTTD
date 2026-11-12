# T2: blocked tiles as run bounds and group domains in the X phase (DESIGN-G1 §14.4, §14.7 T2; D216)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-t2 <HEAD>` (given at launch). Never use git stash.
- Merged: T1 (placement on JunctionPositions with reasons; the fixed-choices overload solves once), T1b-1 (C1, C3; `x_floors` seam; `JunctionTrackPlacement::columns`).
- D216: projection is the graph's placement solved on the target terrain's facts. §14.4: facts enter the X phase only; rows (Y) stay as the structure gives them. A straight run on row y between points a and b must be buildable on every tile it covers; a rigid group (bend, switch tile, bridge head) has a domain of legal anchor columns. JunctionPositions already has min/max-closed `AddAvoid(a, b, c)` (not (x_a <= c <= x_b)) and `RestrictDomain`.
- This step: houses (blocked tiles) only. Heights are T3; edits are T4. A house on a row run that no point can move past fails with a reason chain: that is correct (an edit is needed).

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); TESTS.md (`[positions]`, `[topology]`, `[topoprojsweep]` rows); DESIGN-G1.md §14.4.
- src/junction_facts.h (Straight, Switch, Half, NextBad, MinX/MaxX); src/junction_positions.{h,cpp} (AddAvoid, RestrictDomain, causes, Explain).
- src/junction_topology.cpp the placement helper: Elem kinds, `edge_tiles` (SymTile: anchor point, dx, row), the fixed branch, `solve`, the bound; how the render turns elements and runs into pieces (RenderJunctionTrackPlacement) and how copy-1 (doubled leg) tiles are produced.
- src/junction_nodes.cpp: how the node solve treats bridge middle tiles against facts (JunctionNodeHeightsOk and the template's bridge links).
- src/tests/junction_topology.cpp: SolveLLRRBlocked, "topoproj: every failure on LL_RR names the graph elements it hit" (the 20-tile sweep), FlatTerrainAround, GraphSolveSetupOf.

## Tests (record each red)
1. **Outer, `[topoprojsweep]` "topoproj: placement on facts around each of 20 houses"**: LL_RR at the test spec, the same terrain and the same 20 tiles as the P4a sweep test, at the frame offset that maps the seed placement onto that terrain (derive it from the seed layout's frame; state it). Per tile: place with the seed's choices on the facts. If it succeeds: render, signal tokens, prio element, CheckJunctionRules empty, and no route piece on the house (bridge middles as the node path treats them). If it fails: `why` non-empty, its first entry is a fact reason ("blocked tile") naming the house's column and row, and every entry names a graph element or a fact. Count solved vs the node path's solved count for the same tiles (WARN both). Red: the facts overload stub ignores facts, so a house under a run gets track on it.
2. **`[topology]` "topology: placement on flat facts is the seed"**: T and LL_RR on flat terrain big enough for the box: equals the fixed-choices placement without facts (SamePlacement).
3. **`[topology]` "topology: run and group tiles match the render"**: at the test spec on LL_RR: the tiles the facts are applied to (every SymTile of every element, every horizontal run's columns between its two points, copy-1 tiles) equal, as a set, the tiles of the rendered routes minus bridge middles. Red: stub returns no tiles. If a rendered tile has no symbolic source, STOP and report it.
4. **`[positions]` "positions: a domain or avoid explains itself"**: as T1's test 4 but the failing constraint is RestrictDomain with a reason, then AddAvoid with a reason: Explain() lists them.
- Every existing test stays green unedited.

## Changes
1. **junction_positions:** `RestrictDomain(v, allowed, uint32_t reason = NO_REASON)` and `AddAvoid(a, b, c, uint32_t reason = NO_REASON)`; their lo/up changes record causes {reason, SIZE_MAX} (domain) and {reason, the other variable} (avoid). Existing callers unchanged.
2. **junction_topology.h:** a new overload `JunctionTrackPlacement PlaceJunctionTrackGraph(<same inputs>, const JunctionPlacementChoices &choices, const JunctionFacts &facts, TileIndexDiffC offset, std::string *error);` doc: the fixed-choices solve with the target terrain's facts in the X phase (DESIGN-G1 §14.4); `offset` = the working tile of placement (0, 0). Include junction_facts.h or forward-declare JunctionFacts.
3. **junction_topology.cpp, X phase with facts only:** after the relations, before solving:
   - **Group domains:** for every SymTile (pt, dx, y) of every element: RestrictDomain(pt, the columns c in [0, bound] where the fact holds at (c + dx + offset.x, y + offset.y)); fact kind by tile: Half for bend tiles, Switch for node (split/merge) tiles, Straight along the travel axis for heads and vertical-run tiles. Reason "blocked tile" with the element's edge (and node), column and row in the error text.
   - **Row runs:** for every horizontal run between consecutive points a, b of a chain on row y (including copy-1 runs): for each column c in the facts' range where Straight(X) fails on row y + offset.y: AddAvoid(lower point, higher point, c - offset.x), reason "blocked tile". If a run's end columns are not themselves piece tiles of that route (test 3 shows), STOP.
   - **Bridge middles:** as the node path does (if it checks nothing, add nothing; say which).
   - **Bound:** at least facts.MaxX() - offset.x + 1, so facts can push points across the whole snapshot; columns outside the snapshot are not allowed.
   - `JunctionPlacementReason` needs no new member: put the fact's (column, row) in the error text; `why` keeps the rule "blocked tile" with edge/node/row.
4. TESTS.md rows (`[positions]`, `[topology]` T2 and counts, `[topoprojsweep]` T2 and its time).

## Do not
- No heights (T3), no edits or search change (T4), no Y facts, no change to the rounds or the relations, no change to junction_nodes or the node path.

## Stop conditions
- A rendered tile without a symbolic source (test 3); a run whose ends are not piece tiles; test 1 shows a solved layout with a violation (report each: tile, rule, edges) — a finding, do not add relations; an existing test fails; anything not covered.

## Gates
- `tools/junction/jt fast build '[positions]' 5`; `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt fast build '[topoproj]' 5`; `build-rel/openttd_test "[topoprojsweep]"` (to a file); `tools/junction/jt gate build-rel` (418 before; report the count).
- One commit: `Add: Junction placement on terrain facts: blocked tiles as run bounds and group domains`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus: per tile of test 1, solved or the first two reasons; solved counts (placement vs node path); Release time of one facts solve on LL_RR (mean of 20); the offset used. When you have reported, stop.
