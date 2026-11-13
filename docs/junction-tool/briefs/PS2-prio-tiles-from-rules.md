# PS2: the prio length M comes from the game rules, not the legacy sizing

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
D233: the slider is authoritative (D139); M, its top and start, is `JunctionPrioLength` for the picked train (game acceleration model, flat). No layout simulation. The live callers stop calling the legacy `SizeJunctionPrio`.

## State
Worktree `/home/pete/OpenTTD/.claude/nodes2`. First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt-ps2 <HEAD given at launch>`. Builds `build` (Debug) and `build-rel` (Release) exist; build `openttd_test` (and `openttd` once, for the GUI and console files). Output files in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/ps2/`. Another agent works read-only in `.claude/nodes`: never touch it.

## Read first (once each, offset/limit)
`CODINGSTYLE.md`; `docs/junction-tool/DECISIONS.md` rows D139, D227, D233; `src/junction/junction_train_model.h` (JunctionPrioLength) and its .cpp 110-140; `src/junction/junction_spec.h`; `src/junction/legacy/junction_generator.cpp` 1855-1895 (SizeJunctionPrio, the old bound MAX_PRIO_TILES = 32 at 1718); `src/junction/tool/junction_gui.cpp` 675-690, 835-860, 875-895; `src/console_cmds.cpp` 420-445; `src/junction/game/junction_game.h` 60-70; `src/junction/legacy/CLAUDE.md`; root `src/junction/CLAUDE.md`.

## Tests, red first
1. (outer) In `src/tests/junction/junction_train_model.cpp`, tag `[junction]`: "prio tiles are the game-rules prio length". For speed in {100, 120, 140, 160, 180, 196}: spec = MakeTestJunctionSpec() with train.max_speed = reference_speed = speed; CHECK `JunctionPrioTiles(spec) == JunctionPrioLength(spec.train, spec.reference_speed, spec.tl, spec.sd)` and `> 0` (INFO speed). Second section: a spec whose `JunctionPrioLength` exceeds `JUNCTION_MAX_PRIO_TILES` gives 0. Find the input by lowering `spec.train` tractive effort/power (assert inside the test that the length really exceeds the bound); if no such input exists within the model's tick limit, drop this section and report.
   Red: declare and define `JunctionPrioTiles` returning 0 first, run, record the failure text; then the body.

## Changes
1. `src/junction/junction_spec.h/.cpp` (the spec holds prio sizing):
   - `static constexpr uint8_t JUNCTION_MAX_PRIO_TILES = 32; ///< The longest prio coverage the tool offers, in tiles.`
   - `uint8_t JunctionPrioTiles(const JunctionSpec &spec);` doc: "The prio length M, the slider's top and start (D139): the game-rules prio length of the spec's train at its reference speed on flat track (JunctionPrioLength); 0 when it exceeds JUNCTION_MAX_PRIO_TILES." Body: `const uint32_t length = JunctionPrioLength(spec.train, spec.reference_speed, spec.tl, spec.sd); return length > JUNCTION_MAX_PRIO_TILES ? 0 : static_cast<uint8_t>(length);` (include junction_train_model.h in the .cpp).
2. `src/junction/tool/junction_gui.cpp` worker (~843): replace the SizeJunctionPrio call by `job->spec.prio_tiles = JunctionPrioTiles(job->spec); job->sizing_error = job->spec.prio_tiles == 0 ? JunctionGenerateError::PrioTooLong : JunctionGenerateError::None;`. Keep the thread, the job and the status texts as they are. If the file no longer needs anything else from junction_generator.h, do NOT remove the include (other uses may remain); just report.
3. `src/console_cmds.cpp` (~433): same replacement: `spec.prio_tiles = JunctionPrioTiles(spec); sizing_error = JunctionGenerateError::PrioTooLong;` stays for the 0 case as today.
4. `src/junction/game/junction_game.h/.cpp` comments only: "The strict coverage from SizeJunctionPrio" -> "The prio length M from JunctionPrioTiles".
5. Tests that size a spec for the graph pipeline: every `SizeJunctionPrio(spec).prio_tiles` in `src/tests/junction/topology/junction_topology.cpp` and `src/tests/junction/game/junction_game.cpp` becomes `JunctionPrioTiles(spec)`. Legacy tests (`src/tests/junction/legacy/*`) keep calling SizeJunctionPrio: do not change them.
6. `MakeTestJunctionSpec`'s `prio_tiles = 14` (junction_spec.cpp ~108) stays 14: it is a fixture value, as a slider choice; change only its comment to "A fixed fixture coverage (the old sizing's value at this spec), as a slider choice." Do not change the number.
7. CLAUDE.md, no D-ids, design, brief or recon references:
   - root `src/junction/CLAUDE.md`, under Pipeline, add: "- Prio length: the slider in the window decides the coverage built; its top and start M is `JunctionPrioTiles`: a joiner starting from rest at the merge and a main-line train of the same picked train following at the reference speed, stepped by the game's acceleration model; M is the main train's largest lead until the joiner reaches that speed, in tiles rounded up, plus TL and the signal spacing, on flat track; at most `JUNCTION_MAX_PRIO_TILES`."
   - `src/junction/legacy/CLAUDE.md`: remove "the window's library job calls `SizeJunctionPrio` live on every spec change, in `EnsureLibrary`; it builds no library" and the line "The live prio-length sizing (`SizeJunctionPrio`, called by tool/) still runs on the legacy node model."; state instead "- `SizeJunctionPrio` has no live caller; only legacy tests call it." Keep the rest of the include list true (if a file still includes junction_generator.h, it stays listed).

## Identity
Changing the sizing in tests moves seeds only where prio_tiles changes. Before the change, record in Release for each test you edit in step 5 which spec values it sizes (speed, tl) and the old `SizeJunctionPrio` value next to the new `JunctionPrioTiles` value (a temporary WARN, removed before the commit). Run `jt identity build-rel` and the gate. A moved hash or failing pinned value is allowed only in a test whose prio_tiles changed; list each one with old and new prio_tiles. Anything that moves without a prio_tiles change: STOP and report.

## Do not
No change to JunctionPrioLength, the train model, the legacy generator or its tests, the window's thread/job structure, the status strings, the slider range code, or the placement. No `std::to_string`/`std::printf`; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- JunctionPrioTiles gives 0 for the test spec at any of the six speeds.
- A pinned identity number or test moves where prio_tiles did not change.
- A test edited in step 5 then fails for another reason than a moved pinned value (report it).
- Anything not covered.

## Gates
0 new warnings; `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt gate build-rel`; `tools/junction/jt identity build-rel`; `openttd` target builds. One commit on wt-ps2: `Change: Junction prio length M comes from the game rules, not the legacy sizing`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; per speed JunctionPrioTiles vs the old SizeJunctionPrio value (test spec, 100..196); every test whose pinned values moved, with old/new prio_tiles; gate and identity counts and times; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
