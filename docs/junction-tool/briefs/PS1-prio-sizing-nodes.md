# PS1: size the prio coverage on the node model, not the old generator (D119, D137, D185a, D193; RECON-prio-rule.md)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
SizeJunctionPrio (src/junction_generator.cpp:1940) builds each candidate with the old generator (GenerateJunctionUnchecked), which returns NoLayout at 140/160 km/h in game, and takes ~7 s for six speeds. The rule stays the same (D119: the ML is never delayed, judged by the train simulation PrioTimingsOf); only the layout it is judged on changes: the node model's T (DESIGN-N3 §5-§6), which solves at every speed in ~1.5 ms. Measured (orchestrator, scratch): JunctionPrioLength alone gives 7, 8, 9, 9, 11, 12 at 100..196 km/h against the old sweep's 9, 8, 10, 11, 12, 14, so the simulation on a real layout stays the judge.

## State
Worktree `/home/pete/OpenTTD/.claude/ps1`, branch `wt-ps1`, head given at launch. Create `build` (Debug) and `build-rel` (Release) (clang-18, ccache); build `openttd_test` only. Tests only through `tools/junction/jt`; tags below; `jt gate build-rel` once before the commit. Output in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/ps1/`. Another agent edits src/junction_nodes.* and src/tests/junction_nodes.cpp in .claude/oc1: do not change those files.

## Read first (once each, offset/limit)
`CODINGSTYLE.md`; `docs/junction-tool/RECON-prio-rule.md`; `src/junction_generator.cpp` 1900-1970 (SizeJunctionPrio, PrioTimingsOf) and ConstructJunctionTMap's declaration in `src/junction_generator.h`; `src/junction_nodes.h` (BuildJunctionNodeTemplate, AddJunctionNodeConstraints, JunctionNodeSnap, RenderJunctionNodeTracks); `src/junction_signal_pass.h` (PlaceJunctionSignals); `src/tests/junction_generator.cpp` 60-110 (the SizeJunctionPrio tests); `src/tests/junction_nodes.cpp` 800-860 (the N0c2 speed test and its spec helper).

## Change (only `src/junction_generator.cpp`)
SizeJunctionPrio keeps its declaration, doc meaning and loop (start max(1, spec.prio_tiles); step by required_distance / JunctionProgressPerStraightTile() + 1 or +1; MAX_PRIO_TILES bound -> PrioTooLong). Only the candidate layout changes:
- Topology source: the test spec's flat T, `ConstructJunctionTMap(MakeTestJunctionSpec(), nullptr, std::nullopt, true)`, built once (function-local static); its layout must exist (else return {0, its error}).
- Per candidate: tpl = BuildJunctionNodeTemplate(flat layout, copy); error -> return {0, JunctionGenerateError::NoLayout}. Positions over tpl.flat.size() variables, range [-1024, 1024]; AddJunctionNodeConstraints false -> NoLayout; values = JunctionNodeSnap; empty -> NoLayout; layout = RenderJunctionNodeTracks(tpl, values); PlaceJunctionSignals(layout, copy); NormaliseJunctionLayout(layout) (as the node tests' RenderWithSignals). Then the existing PrioTimingsOf check on that layout.
- Comment the function: the node model's T at the spec, judged by the simulation (D119), replacing the old generator's sweep (D185a).

## Tests (`src/tests/junction_generator.cpp`), red first. Red: the new test with the old body; record.
1. (outer) "SizeJunctionPrio sizes 100 to 196 km/h on the node model", tags `[junction][nodes]`: for speed in {100, 120, 140, 160, 180, 196}: spec = MakeTestJunctionSpec() with train.max_speed = reference_speed = speed and prio_tiles = 0 (as the N0c2 test's helper, minus its constants); sizing = SizeJunctionPrio(spec); CHECK sizing.error == None and prio_tiles > 0 (INFO speed). Minimality: build the node layout at prio_tiles - 1 (when >= 1) the same way inside the test and CHECK at least one PrioTimingsOf-equivalent timing delays the ML — PrioTimingsOf is file-local: if the test cannot reach an equivalent through a public function (JunctionPrioTimings in junction_throughput.h), drop the minimality CHECK and report. WARN speed, prio_tiles, JunctionPrioLength(spec.train, speed, tl, sd). Time the whole test with std::chrono and WARN it. Red: the old body fails at 140/160 (NoLayout) or is over budget; record which.
2. Existing SizeJunctionPrio tests: "sizes the test spec" ([slow]) and "a slower train needs more coverage" ([slow]): run them once with `jt fast build-rel '[generator]' 120` is NOT valid for [slow]; run them directly via the test binary by name ONCE (state the command and output file); if a pinned value changes, replace the pinned equality by: sizing succeeds and the value is minimal (as test 1), and report the old and new values. "reports the generation error": keep; if its premise (an old-generator failure) no longer reaches SizeJunctionPrio, STOP and report.

## Do not
No change to junction_nodes.*, the throughput simulation, JunctionPrioLength, the checker, the old generator itself, the GUI or console. No `std::to_string`/`std::printf`; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
- The node layout has no prio merge/joiner PrioTimingsOf recognises (timings empty): report.
- Sizing fails or exceeds MAX_PRIO_TILES at some speed: report per speed with the timings.
- Test 1 over 1 s Release / 5 s Debug: report per-speed times (a measurement, then stop).
- Anything not covered.

## Gates
0 warnings; `jt fast build '[nodes]' 5`, `jt fast build-rel '[nodes]' 1`, `jt fast build-rel '[generator]' 30`, `jt gate build-rel`. One commit on `wt-ps1`: `Change: Junction prio sizing judged on the node model's T (D119, D185a)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence; test 1 per speed (prio_tiles, JunctionPrioLength, minimality) and its time; the slow tests' old/new values; `[nodes]`/`[generator]` times; gate; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
