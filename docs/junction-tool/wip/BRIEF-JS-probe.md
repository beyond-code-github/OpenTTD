# Brief JS (probe): a prio joiner may run along the main line between its crossings

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, on `wt-tracks`. Run `git switch -c probe-js probe-lj` (probe-lj is at ea0f45d018: `PLANAR_LATE` letters incl. r, g, o; `PLANAR_SIDETRIAL`; `PLANAR_JOG`; `PLANAR_JOINLOW`. A previous attempt created and deleted `probe-js`; create it fresh); commit only there. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 90 minutes. Single-variant runs `timeout -s KILL 30`; pv `timeout -s KILL 600`; end to end `timeout -s KILL 60`. One run at a time. No `PLANAR_IIS`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.

## Why
D356 (DECISIONS.md, corrected): on this probe base the prio join (`Join`, prio case, ~1100-1190 on probe-lj) lets each near lane `S[j]` (j >= 1) step out toward the joiners (`AddSeg(S[j], h, 2)`, `outs[j] = AddSeg(S[j], Turn(h, sigma), 2)`, `AddSeg(S[j], h, 2)`) under the far joiners' straight decks (`jdeck[k]`), and each far joiner turns in onto its lane (`js[k]`, at least 2, no greatest). The far lane `S[0]` alone is built straight (`M.AddSeg(S[0], h, 2)`). On T3 o1t3f1s0 the merges sit at rows 1/8/11 with ports 10/11/12, so D1 climbs up to E1 and ED1 falls 9 tiles after the merge (the engineer's "hump": "Blue should just come down to meet yellow"). The probe removes the far lane's special case.

## Read first (probe-lj, `src/junction/planar/planar_components.cpp`)
- `Join` (prio case, ~1080-1200): `first_seg`/`row_first`, `M.AddSeg(S[0], h, 2)`, the `jdeck`/`js` loop, the `outs` loop, `prio coverage`, the rows (`prio row offset`, built piece by piece from `row_first`), the `jdeck` crossings with `outs[j]` and rows.
- `src/junction/planar/CLAUDE.md`, "Component solver".

## Change (behind a new letter `j` of `PLANAR_LATE`, prio joins only)
Replace the far lane's `M.AddSeg(S[0], h, 2)` by the same three segments the near lanes get: `AddSeg(S[0], h, 2)`, `AddSeg(S[0], Turn(h, sigma), 2)`, `AddSeg(S[0], h, 2)`. Nothing else changes (no joiner crosses the far lane's step; `outs[0]` stays as it is). Without `j` the code is exactly today's. If the rows or coverage loops then fail (a failure text), record it and stop.

## Measure (T3 test `"planar components: T3 from its topology text (mapped) at the hand composition's area"`, T2 test `"planar components: T2 from its topology text (mapped) at the probe's area"`, all with `PLANAR_SIDETRIAL=1 PLANAR_TIMES=1`)
1. `PLANAR_ONLY=o1t3f1s0` on T3 with: `PLANAR_LATE=rgo` (baseline 1715.25), `rgoj`; and both again with `PLANAR_JOG=E1:2,1,2:0` (baseline 1612.25): area, size, track, ms or failure text.
2. Same on T2 (`PLANAR_ONLY=o1t3f1s0`, no jog): `rgo`, `rgoj`.
3. For the best T3 run of 1 with `j`, end to end: `<env> PLANAR_ONLY=o1t3f1s0 PLANAR_LINEDUMP=<S>/js-lines.txt JUNCTION_COMPONENTS_TOPOLOGY=T3 JUNCTION_COMPONENTS_OUT=<S>/js-T3.json timeout -s KILL 60 build-rel/openttd_test "planar: dump a mapped component layout" -s > <S>/js-e2e-T3.txt 2>&1`: plan result, `MAPPED`, `VIOLATION` lines grouped by rule; heading runs of E1-3 and D1-3 (`bash <S>/runs.sh <S>/js-lines.txt <route>`); and the first and last piece (x y) of ED1, ED2, ED3 from the dump (`awk '$5=="ED1"'`, head -1 / tail -1).
4. `pv run build-rel T3` and `pv run build-rel T2` with `PLANAR_SIDETRIAL=1 PLANAR_LATE=rgoj` (no jog), `--out <S>/js-pv-<hub>.txt`: built/failed with reasons grouped, best variant and area, max ms.

## Do not
- Change the slot layer, paths, signal pass, checker, `Fray`, `Double`, `PairDecks`, or any composer rule other than the above. Touch ports 4077/4079 or start a game client. Use git stash. Run two heavy jobs at once.

## Commit and report
- Commit on probe-js: `Probe: Junction prio joiner runs along the main line between crossings`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Reply with the measurements, any stop, the commit hash, `git status --porcelain`. When you have reported, stop.
