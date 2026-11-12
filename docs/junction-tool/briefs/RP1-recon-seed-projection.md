# RP1 (recon): the G1 seed through the node template and solve (projection)

Read-only recon. Do not commit code. You may add a temporary hidden diagnostic TEST_CASE (`[.][diag]`) in src/tests/junction_topology.cpp to measure; remove it before you finish (leave the worktree clean: `git status --porcelain` shows only your recon file). Keep your context small: send command output to files and report only what failed. Never use git stash. Mark anything you did not read in code as INFERRED.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-rp1 <HEAD>` (given at launch).
- Goal (engineer): projection of the new G1 seeds (the T and LL_RR, built by the topology pipeline, see SignalledOf in src/tests/junction_topology.cpp) onto real terrain with houses and obstacles. On flat ground the seed is the layout. Today's projection (GenerateJunctionNear, junction_game.cpp) uses the traced flat T: BuildJunctionNodeTemplate(flat layout, spec) -> SolveJunctionNodes(tpl, spec, facts, offset) (junction_nodes.h). DESIGN-G1 §10 planned: seed -> template -> Lo -> fixed point (G1c(iii)); FABLE-G1c.md §3 (a)-(f) lists the template fixes it needs. That review predates the exact placement (DESIGN-G1 §11.8) and G1d (topology-aware checker).
- D212 (DECISIONS.md): where a lane changes rows near a port is decided in projection: a lane keeps its port row as long as it can.

## Read first (once each, offset/limit)
- DESIGN-G1.md §10, §11.8; FABLE-G1c.md §3; DESIGN-nodes.md §2-§3; DESIGN-N3.md §1-§3, §7, §10 (skim headings, read what you need).
- src/junction_nodes.h (whole), junction_nodes.cpp (as needed); junction_game.cpp GenerateJunctionNear (how the template is built and solved, terrain facts, offsets, timing).

## Questions (answer each, with file:line)
1. **Does the seed go through today?** For the T seed and the LL_RR seed (SignalledOf pipeline): BuildJunctionNodeTemplate(layout, spec): succeeds? point/link/class/bridge counts; for the T, DiffJunctionNodeTemplates against the traced flat T's template. Then SolveJunctionNodes on flat facts (all tiles flat, no houses; find how the existing tests make flat facts) at offset 0: feasible? If infeasible, the JunctionNodeSolve failure info. If feasible: RenderJunctionNodeTracks of the result: does it equal the seed's track bits (tile diff count)? Release timings of build and solve.
2. **Role-specific code** still in junction_nodes.cpp (and anything it calls in the solve/render path): every place that assumes the T's ports/movements (FABLE-G1c §3(d) names IsMainLinePath :111, rule 5 EIn/WIn->SOut :528, rule 6 :538, the EIn->SOut anchor :688; line numbers will have moved). For each: what it does, and the topology replacement (G1d gave JunctionPath from_slot/to_slot/movement, IsThroughPath, IsExitPath, SameLanePath, FindJunctionPorts(layout, topology)).
3. **FABLE-G1c §3 (a)-(f) today:** for each item, does it still apply now that the seed is the exact least placement (not a generous seed)? Evidence from code or the diag.
4. **Signals:** does the node path keep, drop or re-place the seed's signals (SignalJunctionTrackRoutes tokens, prio element)? What does GenerateJunctionNear do for signals after the solve?
5. **The port fan (D212):** how does the template represent a lane's port -> fan run and the fan S-bend (link classes, rigid or free)? Which value does the solve pick for a free run (least? snap to flat?) and so where would the fan land on terrain? Is there an existing objective/preference mechanism in the solve where "fan as late as it can" could be expressed? Name it, do not design it.
6. **Terrain and obstacles:** list what the solve handles today (heights HF1, houses, tunnels G5, bridges, terraform, negotiation/detour InsertJunctionDetour) and whether each depends on T roles.
7. **Wiring:** what GenerateJunctionNear / the GUI need to take a topology (which topology is selected today, where the flat T is traced/cached).

## Output
docs/junction-tool/RECON-seed-projection.md, at most ~120 lines, one section per question, facts with file:line, then a short "Gaps for projection of the G1 seed" list ordered by dependency. Commit nothing; leave it untracked.

## Report
References read; the answers to Q1 in five lines; the gaps list; `git status --porcelain` and `git log -1`. When you have reported, stop.
