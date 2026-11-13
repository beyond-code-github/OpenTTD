# SP2: split junction_topology.{h,cpp} into topology, placement, signals and the pipeline driver (identity step)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed. Never read junction_topology.cpp whole; use the line map below with offset/limit.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes. Run `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-sp2 <head>` with the head given in your launch prompt. Builds: `build` (Debug) and `build-rel` (Release). Ignore docs/junction-tool/wip/. Never use git stash.
- D226 (DECISIONS.md, last row) is the ruling. SP1 has already moved the route types to junction_route.h, and the level solve no longer includes topology.h.
- The map is docs/junction-tool/RECON-topology-placement-seam.md §1. Its line numbers are from before GS1/SP1 and have shifted by a few lines, so find each item by its NAME.

## Read first
src/junction/CLAUDE.md, topology/CLAUDE.md, placement/CLAUDE.md, src/tests/junction/CLAUDE.md, RS1 recon §1-§5, and topology/junction_topology.h (all of it, about 440 lines).

## Target files (code moves VERBATIM: same text, comments and order within each moved block)
- `topology/junction_topology.{h,cpp}` keeps:
  - port slots, lanes, chords and slot rows;
  - the row-item and fan helpers, and the `JunctionIs*Edge` predicates;
  - the tree builder and `BuildJunctionTrackGraph`;
  - `OrderJunctionTrackGraph`, `StructureJunctionTrackGraph` and `ProgramJunctionTrackGraph`;
  - `JunctionTTopology`, `JunctionLLRRSidelineTopology` and `MakeJunctionHubTopology`;
  - the types those functions produce: topology, graph, order, structure, programs, bridges.
- `placement/junction_placement.{h,cpp}` (new):
  - the placement helpers block (`FloorKind`, `JunctionPrioStepBits`, `JunctionRunFloor`, and the others from that block that only placement uses);
  - `PlaceTrackGraph`;
  - the two `PlaceJunctionTrackGraph` overloads that take a seed or fixed choices;
  - `MakeJunctionPlacementShared` and `JunctionPlacementNearReject`;
  - the types `JunctionTrackPlacement`, `JunctionPlacementChoices`, `JunctionPlacementShared` and `JunctionPrioSpan`, plus any other type that only placement writes.
- `placement/junction_render.{h,cpp}` (new): `RenderJunctionTrackPlacement`, `JunctionTrackTouches` and `AddJunctionTracks`.
- `signals/junction_signals.{h,cpp}` (new): `JunctionRouteEvents` and `SignalJunctionTrackRoutes`, plus the types only they write, such as `JunctionSignalReason`.
- `signals/` also receives, by `git mv`: `topology/junction_signal_pass.{h,cpp}` and its test `src/tests/junction/topology/junction_signal_pass.cpp`, which goes to `src/tests/junction/signals/`.
- `src/junction/junction_pipeline.{h,cpp}` (new, root):
  - `AppendBridgeCompositions`, `AppendBridgeCounts` and `CutJunctionTrackBridges`;
  - `MakeJunctionGraphSeed` and its `JunctionGraphSeed` type;
  - `IsRunPiece` and `ApplyJunctionDetours`;
  - the third `PlaceJunctionTrackGraph` overload, the one taking facts and terraform that calls render, signals and `SolveJunctionLevels`. It keeps its name.
- `src/junction/junction_track_geometry.{h,cpp}` (new, root): `JUNCTION_AXIS_TRACKDIR`, `JunctionBendMoves` and `JunctionCornerTrack`, plus every other file-static helper or constant that ends up used by two or more of the new files. Each one moves verbatim, minus `static` or the anonymous namespace, and is declared in the .h with its existing doc comment. List every helper you put here.

## Rules for anything the map does not settle
- A helper defined in an earlier stage and used by a later one stays in the earlier stage's file. If it is file-static or in an anonymous namespace, it becomes declared in that stage's header (e.g. `FanTokenOf`, the `JunctionIs*Edge` predicates). Dependencies run in one direction: topology → placement → render → signals → pipeline.
- If any code would need to call a LATER stage (e.g. topology calling placement, or placement calling signals), stop and report the call.
- A type goes with the stage that writes it. If a type is written by one stage and only read later, it goes with the writer.
- Never duplicate a definition.

## Also
- CMake: add the new files to the folder lists, and add `add_subdirectory(signals)` plus a `src/junction/signals/CMakeLists.txt`. Do the same in src/tests/junction/. Add junction_pipeline and junction_track_geometry to the root list.
- Includes: every user of a moved symbol includes the header that now declares it. topology.h must not include any placement, signals or pipeline header. Test files stay where they are, except signal_pass, and only gain includes.
- CLAUDE.md, moving facts without rewording them:
  - Move topology/CLAUDE.md's "Signals and prio" section and the signal_pass and PlaceJunctionSignals lines into a new `src/junction/signals/CLAUDE.md`, with the title `# Junction signals (src/junction/signals/)`.
  - Delete the line "Horizontal placement ... currently lives in junction_topology.cpp ..." from topology/CLAUDE.md.
  - In the root CLAUDE.md folder map: add signals/ (signal resolution, drag signals, prio spans) and junction_pipeline (the driver: bridge cut, seed, detours, terraform entry). Update the topology/ and placement/ lines so placement/ reads "horizontal placement, render, positions solver, level solve".
  - Update docs/junction-tool/PATHS.md with one line per moved symbol group.

## Do not
- Change any logic, statement order inside a function, error text, reason literal, or JunctionPositions call order.
- Rename anything.
- Split PlaceTrackGraph into functions.
- Touch legacy/ beyond include lines.

## Stop conditions
- A call to a later stage (see the rules above).
- A helper whose placement the rules do not settle.
- Any identity hash or test count change.
- A new warning in a file you created.

## Gates
- Before the change, run `tools/junction/jt gate build-rel` and `tools/junction/jt identity build-rel` from the worktree root to get a baseline. Expect 489 cases and 3041662 assertions for the gate, and 4 cases and 255 assertions for identity.
- After the change:
  - Debug and Release both build `openttd` and `openttd_test`, with no new warnings (the known set is 16, in legacy/junction_nodes.cpp and topology).
  - gate and identity show the same counts.
  - `tools/junction/jt fast build '[topology]'` passes within its budget.
  - `grep -nE 'junction_(placement|render|signals|pipeline)\.h' src/junction/topology/*.h src/junction/topology/*.cpp` returns nothing.
- One commit: `Codechange: Junction topology file splits into topology, placement, signals and the pipeline driver`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
- References read.
- Commit hash and subject.
- Baseline and after counts.
- Line counts of every resulting file.
- The helpers in junction_track_geometry.
- The symbols that became exported, with the reason for each.
- The remaining cross-folder includes, listed per file.
- Stops.
- `git status --porcelain` and `git log -1`.

When you have reported, stop.
