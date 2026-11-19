# Brief UD (probe, throwaway): under a bridge on the diagonal

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`. Run `git switch -c probe-ud probe-sl` (probe-sl is at {HEAD}); commit only there. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 90 minutes. Single-variant runs `timeout -s KILL 30`; pv runs `timeout -s KILL 600`; end to end `timeout -s KILL 60`. One run at a time.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad` (`mkdir -p` if missing).

## Why
The engineer, on T2 (D lanes, S->E, passing under the B main line's bridges and then turning east to join E): "it's that they don't turn as early as they could do - they could be diagonal under the bridges and then only have to do a further 45 degree turn - which would leave them 5 or 6 tiles closer in". Diagonal track under a bridge is allowed (DECISIONS D61). Today `BuildPlanarPaths` (`src/junction/planar/planar_paths.cpp` ~330-345) puts a crossing under a prio channel on a piece at right angles to it when one exists ("Under a prio (doubled) channel, cross at right angles ..."), else on the first piece that suits; so D crosses B on its N piece and turns NE and E only after.

## Read first
- `src/junction/planar/planar_paths.cpp` ~190-380 (crossing pieces, `suits`, the right-angle block, detours, settling).
- `src/junction/planar/CLAUDE.md` "Channel paths" and "Shape program" sections (default shape vs `SolvePlanarShape`, `PLANAR_SHAPE`).

## Change (planar_paths.cpp only)
When `PLANAR_UNDERDIAG` is set in the environment, for a crossing where the channel is under (`!over`): skip the right-angle block, and before the existing first-suiting loop look for the first piece index i >= `at` whose heading is diagonal (odd) and suits; if found, `at = i`, placed. Otherwise fall through to the existing loop unchanged. Over crossings and everything else are unchanged.

## Measure
Variant run: `PLANAR_ONLY=o1t3f1s0 PLANAR_TIMES=1 <env> timeout -s KILL 30 build-rel/openttd_test "planar components: T2 from its topology text (mapped) at the probe's area" -s > <S>/ud-<name>.txt 2>&1`: area, size, track, ms or failure (with its reason); the last `slack` line and the `face` lines.
1. No env: 932.25 75x31 (unchanged).
2. `PLANAR_UNDERDIAG=1`; then `PLANAR_UNDERDIAG=1 PLANAR_SHAPE=default`; then `PLANAR_SHAPE=default` alone (baseline for the default shape).
3. `PLANAR_UNDERDIAG=1 PLANAR_DRY=1` on T2 (no `PLANAR_ONLY`): per variant whether the shape builds, and the crossing piece headings of D's channel if the dry line shows them.
4. `pv run build-rel T2` and `pv run build-rel T3` (tool `/home/pete/OpenTTD/tools/junction/pv`, `--out <S>/ud-pv-<hub>[-base].txt`), each with and without `PLANAR_UNDERDIAG=1`: built/failed counts with failure reasons grouped, best variant and area, per variant areas side by side.
5. End to end on T2 and T3 with `PLANAR_UNDERDIAG=1`: `JUNCTION_COMPONENTS_TOPOLOGY=<hub> JUNCTION_COMPONENTS_OUT=<S>/ud-<hub>.json timeout -s KILL 60 build-rel/openttd_test "planar: dump a mapped component layout" -s > <S>/ud-e2e-<hub>.txt 2>&1`: plan success or message, the `MAPPED` line, `VIOLATION` lines grouped by rule with counts. (T3 end to end may segfault in `planar_plan.cpp:238` on this branch with or without the knob; if so run it once without the knob to confirm and report both.)

## Do not
- Change anything outside planar_paths.cpp; change over-crossing placement, detours, settling, `CheckPlanarShape`, `SolvePlanarShape`, the composer, slot layer, signal pass or checker.
- Touch ports 4077/4079 or start a game client. Use git stash. Run two heavy jobs at once.

## Stop conditions
- Measurement 2 fails with a composer refusal (e.g. "a slot step on a diagonal piece is not composed"): record the exact message, run measurements 3-4 anyway, report.

## Commit and report
- Commit on probe-ud: `Probe: Junction under crossings on a diagonal piece (throwaway)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Write `/home/pete/OpenTTD/docs/junction-tool/wip/PROBE-UD.md` (do not commit): change and measurements 1-5.
- Reply with the measurements, the commit hash, `git status --porcelain`.

When you have reported, stop.
