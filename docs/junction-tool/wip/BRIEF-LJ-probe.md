# Brief LJ (probe, throwaway): a straight channel's climb after its crossings, from the shape

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, on `wt-tracks`. Run `git switch -c probe-lj probe-lo` (probe-lo is at 321977a8fc: `PLANAR_LATE` letters r, c, g, f, a-free, o, s; `PLANAR_SIDETRIAL`, `PLANAR_SIDEANY`); commit only there. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 90 minutes. Single-variant runs `timeout -s KILL 30`; end to end `timeout -s KILL 60`. One run at a time. No `PLANAR_IIS`, no pv.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.

## Why
Rulings D340/D350: bends are never optional pieces; headings come from the shape layer (one `PlanarChannelPath` per channel: headings per piece, the piece each crossing sits on), and the length solve scores shapes. On T3 o1t3f1s0 the E lines climb early (fray rise, the prio fray's doubled steps) and leave empty space; the engineer's hand edit (`wip/ANALYSIS-PD.md`) climbs E after its bridges instead. Today the E channel's shape is one heading (east) so it is built as a core on one heading and cannot climb. The probe hands the solver a shape where the channel climbs after its crossings and lets the length solve place it.

## Read first
- `src/junction/planar/planar_components.cpp` (probe-lo): `SolveMappedVariant` (~2670-2720, the `PLANAR_SHAPE` modes and how a shape reaches the composer); where cores are chosen (`path.headings.size() == 1`) and `Bundle` (a multi-heading channel is a bundle).
- `src/junction/planar/planar_paths.h`: `PlanarChannelPath`, `CheckPlanarShape`.
- `src/junction/planar/CLAUDE.md`, "Channel paths" and "Shape program".

## Change
In `SolveMappedVariant`, when `shape == nullptr` and `PLANAR_JOG` is set (format `<line>:<h0>,<h1>,...:<piece>`, e.g. `E1:2,1,2:0`): build the default paths (`BuildPlanarPaths(topology, channels, variant.over)`), find the channel whose `lines` contain the topology line named `<line>`, set its `headings` to the list and every entry of its `crossing_piece` to `<piece>`, run `CheckPlanarShape` on the result if the code has a call for a given shape (report its `why` and stop the run if it refuses), and solve the variant with that shape only (no program shape). Under `PLANAR_TIMES` print `jog channel <index> lines <names> crossings <count> headings <list> piece <p>`.
If the composer refuses the shape (a failure text), record the text; do not change the composer to accept it.

## Measure (T3 test `"planar components: T3 from its topology text (mapped) at the hand composition's area"`, `PLANAR_ONLY=o1t3f1s0 PLANAR_TIMES=1 PLANAR_SIDETRIAL=1`)
1. Without `PLANAR_JOG`: `PLANAR_LATE` unset and `o` (baselines 1796.25 and 1715.25).
2. `PLANAR_JOG=E1:2,1,2:0` (east, north-east, east; crossings before the climb) and `E1:2,1,2:2` (after it), each with `PLANAR_LATE` unset, `o`, `ro`, `rgo`: area, size, track, ms or failure text. Also the `jog` print line once.
3. If `E1` is not in a channel that runs east on one heading in the default shape, stop and report its channel's lines and headings.
4. End to end for the best of 2 by track length (any area): `PLANAR_SIDETRIAL=1 PLANAR_JOG=<it> PLANAR_LATE=<it> PLANAR_ONLY=o1t3f1s0 PLANAR_LINEDUMP=<S>/lj-lines.txt JUNCTION_COMPONENTS_TOPOLOGY=T3 JUNCTION_COMPONENTS_OUT=<S>/lj-T3.json timeout -s KILL 60 build-rel/openttd_test "planar: dump a mapped component layout" -s > <S>/lj-e2e-T3.txt 2>&1`: plan result, `MAPPED`, `VIOLATION` lines grouped by rule; E1-3 heading runs from the dump compressed with `<S>/runs.sh`.

## Do not
- Change the composer, slot layer, paths builder, shape program, signal pass or checker. Touch ports 4077/4079 or start a game client. Use git stash. Run two heavy jobs at once.

## Commit and report
- Commit on probe-lj: `Probe: Junction shape with a late climb on a straight channel (throwaway)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Reply with the measurements, any refusal text, the commit hash, `git status --porcelain`. When you have reported, stop.
