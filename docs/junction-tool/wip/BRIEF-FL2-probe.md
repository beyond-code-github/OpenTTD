# Brief FL2 (probe, throwaway): the prio fray's straight legs may climb after their bridges

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`. It is on `probe-sy` (built). Run `git switch -c probe-fl2 probe-sy` (probe-sy is at {HEAD}); commit only there. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 60 minutes. Single-variant runs `timeout -s KILL 30`; pv runs `timeout -s KILL 600`; end to end `timeout -s KILL 60`. One run at a time. Do not run `PLANAR_IIS`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.

## Why
On T3 o1t3f1s0 the E lanes' bridges over F are the prio fray's own crossings, built inside `Fray` (`planar_components.cpp` ~1018-1035: `S[0]` one straight segment; each `S[j]` doubled over the far `V` legs, then a straight segment). Holding the split low (`PLANAR_SPLITLOW=2`) is infeasible, and the conflict set is only E1's four segments (all on heading h, so E1's end has the split's y) plus the split-low row. The `c` letter of `PLANAR_LATE` added a late climb to cores, but E1 is a fray leg, not a core. The engineer's hand edit splits low, bridges E over F there, then climbs E to its port rows after the bridges.

## Change (planar_components.cpp, `Fray`, prio case only)
New letter `f` of `PLANAR_LATE` (`LateFlag('f')`): after the straight legs are built (after `M.AddSeg(S[0], h, 2)` for lane 0, and after the `M.AddSeg(S[j], h, 2)` that follows each `S[j]`'s doubling), add per lane `S[k]` a segment `AddSeg(S[k], Turn(h, -1), 0)` (the same turn as the in-lane rise `Lt`) then `AddSeg(S[k], h, 0)`, all lanes' climb diagonals equal in length (rows under `TagScope("fray late climb")`). Lane 0's climb is added after the loop over j so every lane's climb comes after its bridges. If the model refuses a zero-length diagonal here, stop and report the text.

## Measure (T3 test `"planar components: T3 from its topology text (mapped) at the hand composition's area"`)
`PLANAR_ONLY=o1t3f1s0 PLANAR_TIMES=1 <env> timeout -s KILL 30 build-rel/openttd_test "<T3 test>" -s > <S>/fl2-<name>.txt 2>&1`: area, size, track, ms or failure; the `split dy` print.
1. `PLANAR_LATE=f`, `PLANAR_LATE=rf`, `PLANAR_LATE=rfg`, `PLANAR_LATE=rcfg`.
2. For the best of 1 by area: with `PLANAR_SPLITLOW` = 4, 8, 12 (half units): each result (does the low split solve now, at what area and track).
3. The best of 1 and 2 by track length end to end with the line dump: `<env> PLANAR_ONLY=o1t3f1s0 PLANAR_LINEDUMP=<S>/fl2-lines.txt JUNCTION_COMPONENTS_TOPOLOGY=T3 JUNCTION_COMPONENTS_OUT=<S>/fl2-T3.json timeout -s KILL 60 build-rel/openttd_test "planar: dump a mapped component layout" -s > <S>/fl2-e2e-T3.txt 2>&1`: plan result, `MAPPED`, `VIOLATION` lines grouped by rule. Name the env used.
4. `pv run build-rel T3` and `pv run build-rel T2` with the best `PLANAR_LATE` of 1 (no `PLANAR_SPLITLOW`), `--out <S>/fl2-pv-<hub>.txt`: built/failed with reasons grouped, best variant and area, max ms; per variant side by side with `<S>/fl-pv-T3-base.txt` / `<S>/fl-pv-T2-base.txt` if present.

## Do not
- Change anything else. Touch ports 4077/4079 or start a game client. Use git stash. Run two heavy jobs at once.

## Commit and report
- Commit on probe-fl2: `Probe: Junction prio fray straight legs climb after their bridges (throwaway)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Reply with the measurements, the commit hash, `git status --porcelain`. When you have reported, stop.
