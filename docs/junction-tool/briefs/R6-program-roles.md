# R6: programs carry roles: no in/leg/out shape check; S-bends tagged lane split, lane merge or shift (DESIGN-G1 §16 R6; RECON-row-chain §1, §4 step 6, risk 10)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-r6 <HEAD>` (given at launch). Never use git stash. Another agent may work in .claude/nodes2: never touch it.
- R5 merged: placement reads every edge as an Elem chain. What still assumes the fixed row shape: ProgramJunctionTrackGraph's check "a lane other than in, one leg, out is not ruled yet" (~:742) and its refusal of a crossed in/out edge (~:763); JunctionRouteEvents finds a doubled leg's lane split/merge as "the first SBend token" (~:3305), which an inserted Shift S-bend would break; placement's `doubled_row`, `in_edge/leg_edge/out_edge` per row as the way chains are built (read RECON-row-chain §1 for the list); R5's copy-1 S-bend takes its hand from `program.back().hand` (check it is the role's hand).
- Next step R7 inserts a Shift (an S-bend pair) into an edge's program; this step makes programs and their readers ready for it without changing any output.

## Read first (once each, offset/limit)
- CODINGSTYLE.md (C++ part); TESTS.md; DESIGN-G1.md §16; RECON-row-chain.md §1, §4-§5.
- src/junction_topology.h JunctionTrackStep, JunctionTrackPrograms; src/junction_topology.cpp ProgramJunctionTrackGraph (the row-shape checks), JunctionRouteEvents and its three callers in the signal pass, the placement's row identification (in_edge/leg_edge/out_edge, doubled_row, doubled_edge), RenderJunctionTrackPlacement's bend matching (~:3021 per the recon).

## Tests (record each red)
1. **Outer**: every existing test and sweep with identical numbers; circuit byte-identical.
2. **`[topology]` "topology: an inserted S-bend pair does not move the lane events"**: on LL_RR's programs, insert an SBend pair (hand +1 then -1, role Shift, index = the leg's own item) into one doubled leg's copy-0 program before its lane-merge SBend; JunctionRouteEvents for the routes through it returns the same lane split/merge events (by role) as without the insert (routes rendered from a placement that may fail: if placement fails, build the routes' tags by hand for the test; say which). Red: "first SBend" picks the inserted one.
3. **`[topology]` "topology: a row edge may hold any token sequence"**: ProgramJunctionTrackGraph-produced programs plus an inserted SBend pair on an undoubled leg pass the program checks (no "not ruled yet"); placement of it is NOT required in this step (R7).

## Changes
1. JunctionTrackStep gains `enum class Role : uint8_t { None, LaneSplit, LaneMerge, Shift }; Role role = Role::None; ///< SBend: what it serves (D215 feature; R7 inserts Shift pairs).` (and in operator==). ProgramJunctionTrackGraph sets LaneSplit/LaneMerge on the doubled legs' SBends.
2. JunctionRouteEvents and every reader that finds a lane split/merge by position use the role.
3. The program-level row-shape checks become per-edge role checks: a row edge's program may hold Fan (first on in edges, last on out edges), SBend of any role, Over/Under; a lane must still have exactly one leg between its exit split and joiner merge (that is graph structure, keep it). Keep "a crossed lane in or out edge" refusal unless placement now handles it (say).
4. Placement's identification of row edges stays; any place that finds the lane pair by position reads the role. R5's copy-1 hand: use the role's own step hand; numbers must stay identical.
5. TESTS.md rows.

## Do not
- No Shift placement (R7), no edit/search, no render change beyond reading roles if it matches bends by position (say if it does).

## Stop conditions
- Any number changes; render matching that cannot use roles; anything not covered.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `'[topoproj]' 5`; `build-rel/openttd_test` with `"[topologysweep]"`, `"[topologystretch]"`, `"[placeoracle]"`, `"[realterrainsweep]"`, `"[topoprojsweep]"` (diff against launch HEAD: identical except timings); circuit byte-identical; `tools/junction/jt gate build-rel` (report the count).
- One commit: `Codechange: Junction program steps carry roles; lane events read roles (R6)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report. When you have reported, stop.
