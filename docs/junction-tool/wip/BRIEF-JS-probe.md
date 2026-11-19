# Brief JS (probe): a prio joiner may run along the main line between its crossings

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, on `wt-tracks`. Run `git switch -c probe-js probe-lj` (probe-lj is at ea0f45d018: `PLANAR_LATE` letters incl. r, g, o; `PLANAR_SIDETRIAL`; `PLANAR_JOG`; `PLANAR_JOINLOW`); commit only there. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 90 minutes. Single-variant runs `timeout -s KILL 30`; pv `timeout -s KILL 600`; end to end `timeout -s KILL 60`. One run at a time. No `PLANAR_IIS`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.

## Why
D356 (DECISIONS.md): in `Join` (prio case) each joiner `J[k]` is one diagonal segment `js[k]` that crosses every nearer lane `S[j]` (j > k) on that lane's doubled stretch before landing on `S[k]`. Those crossings sit about a prio length apart along the main line, so a single 45-degree diagonal forces the main lanes that far apart sideways (T3 o1t3f1s0: E1/E2/E3 at rows 1/8/11 at the join, ports 10/11/12, ED1 falls 9 tiles: the engineer's "hump"). This is construction, not a rule. The probe lets the joiner run along the main line between crossings.

## Read first (probe-lj, `src/junction/planar/planar_components.cpp`)
- `Join` (~1060-1100): the `js` loop (`M.AddSeg(J[k], h, 2)`, then `M.AddSeg(J[k], Turn(h, -sigma), lo, n == 2 ? lo : nullopt)`), the prio loops that call `PairDecks(d0, d1, {js[k]}, h)` and `PairDecks(d0, d1, {js[j - 1]}, h)`, the `Tie`s.
- `Model::Cross`, `Model::Deck`, `PairDecks` (how a crossing names the under segment).
- `src/junction/planar/CLAUDE.md`, "Component solver" (joins) and `PieceCurves` in `planar_model.cpp` (every heading change is a curve with spacing rows).

## Change (behind a new letter `j` of `PLANAR_LATE`, prio joins only)
For each joiner `J[k]` that crosses at least one nearer lane (k + 1 < n), build instead of the one diagonal:
- `AddSeg(J[k], h, 2)` (as today);
- for each crossed lane in the order the joiner meets them (nearest to the joiner's side first: j = n-1 down to k+1): a crossing diagonal `x[k][j] = AddSeg(J[k], Turn(h, -sigma), 2)` then an axis run `AddSeg(J[k], h, 0)`;
- the landing diagonal `js[k] = AddSeg(J[k], Turn(h, -sigma), lo, n == 2 ? lo : nullopt)` (lo as today).
Each crossing of lane `S[j]` with joiner `J[k]` (the `PairDecks` calls) uses `x[k][j]` instead of `js[k]`. Everything else in `Join` is unchanged (the `Tie`s, prio rows, the exit). Without `j` the code is exactly today's. If the crossing loop does not let you identify which lane j a `PairDecks` call is for, stop and report.

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
