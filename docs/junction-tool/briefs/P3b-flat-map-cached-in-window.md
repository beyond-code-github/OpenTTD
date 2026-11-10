# P3b: the junction window keeps the flat map between hovers (D173, P3)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
After P3, each hover search (`GenerateJunctionNear`, run on a worker by `BuildJunctionWindow::StartJob`) computes D173's flat map once, about 25 ms of a 38 ms flat-ground hover in Release. The flat map depends on the spec only, and the spec only changes when the settings or the slider change. The window keeps the last flat map and its key and hands it to every hover search with the same spec.

## State
Worktree `/home/pete/OpenTTD/.claude/p3b`, branch `wt-p3b`, head given at launch (feature/junction-tool with P3 merged: `ConstructJunctionTMap(..., const JunctionTMapResult *flat)`). Create `build` (Debug) and `build-rel` (Release) like the main ones (clang-18 with ccache); build `openttd_test` and, at the end, `openttd` (to prove the GUI compiles). Tests only through `tools/junction/jt` with bracketed tags.

## Read first (once each)
`CODINGSTYLE.md`; `GenerateJunctionNear` in `src/junction_game.{h,cpp}`; in `src/junction_gui.cpp`: `JunctionGenerateKey` (and how `library_key` neutralises anchor/rotation/mirror), `JunctionGenerateJob`, `StartJob`, `PollJob`, `DetachJob`, the window's members.

## Tests, red first
1. `src/tests/junction_game.cpp`, `[tmapnear]`: "GenerateJunctionNear with a supplied flat map gives the same result": for the ridge terrain of the existing ridge test, `GenerateJunctionNear(spec, terrain, 0, false, cancel, &flat)` equals the call without `flat` (layout, offset, hub, score). Red: the parameter does not exist (add it first, ignored, so the red is an assertion on a time or a flag: assert the supplied call does not run the flat map by checking `micros` of a direct `ConstructJunctionTMap` with `flat` given is below the flat map's own `micros`; if that is not observable through `GenerateJunctionNear`, say so and keep only the equality, with a red from the ignored parameter in a direct `ConstructJunctionTMap` check).
2. The GUI cannot be unit-tested: name it as not tested, and build `openttd` with 0 warnings.

## Changes
1. `src/junction_game.{h,cpp}`: `GenerateJunctionNear` gains a last parameter `const JunctionTMapResult *flat = nullptr` (doc: D173's flat map for this spec, kept by the caller; when null it is computed here, P3). When given, use it and skip computing it.
2. `src/junction_gui.cpp`:
   - `JunctionGenerateJob` gains `std::shared_ptr<const JunctionTMapResult> flat; ///< P3b: the window's flat map for this spec, or null; the worker computes and sets it when null.` The worker: if `flat` is null, compute `ConstructJunctionTMap(job->spec, nullptr, std::nullopt, false)` into a new shared_ptr and set `job->flat` before calling `GenerateJunctionNear(..., job->flat.get())`.
   - The window gains `std::optional<JunctionGenerateKey> flat_key; ///< P3b: the key (anchor INVALID_TILE, rotation 0, not mirrored) of flat_map.` and `std::shared_ptr<const JunctionTMapResult> flat_map; ///< P3b: the flat map for flat_key's spec.`
   - `StartJob`: compute the neutral key from the job's key the way `library_key` is formed, but keeping `prio_tiles` (the slider's coverage sets the flat map); if it equals `flat_key`, set `job->flat = this->flat_map`.
   - `PollJob`: when a finished job has a `flat`, store it with its neutral key in `flat_key`/`flat_map`.
   - Nothing else in the GUI.

## Do not
No change to the generator, the router, the library tier, decisions, savegame, network or command code. No settings. No `std::to_string`/`std::printf` in tests; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
Any existing test changes outcome; the key cannot be formed without changing `JunctionGenerateKey`; OVER BUDGET; anything not covered.

## Gates
0 warnings in both builds and in `openttd`; `jt fast build-rel '[tmapnear]' 5` and the gate pass. One commit on `wt-p3b`: `Change: Junction window keeps the flat map between hovers`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
Commit; red evidence; members added; jt lines; what was not tested (the GUI path); `git status --porcelain`, `git log -1`.

When you have reported, stop.
