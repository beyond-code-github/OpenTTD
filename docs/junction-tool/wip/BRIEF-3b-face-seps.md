# Brief 3b-1: region separations up front (experiment knob, measured)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, head `{HEAD}`, build dir `build-rel` (Release). Build only `openttd_test`, never `openttd` (a rig and the engineer's client run that binary). Never use git stash, never start a game client, never touch ports 4077/4079.
- Plan: `/home/pete/OpenTTD/docs/junction-tool/DESIGN-tracks.md` §3-§4. Step 3a added `PlanarLineSides` (`src/junction/planar/planar_regions.{h,cpp}`): faces of the topology's planar map; `Side(p, q)` = the side of q (relative to q's travel) where their common face lies. The lazy loop uses it to separate every face-decided clash of a round at once.
- This step: an experiment knob `PLANAR_FACESEPS` that adds face-neighbour separations BEFORE the first solve, so the lazy loop should find few or no clashes. Measured, not yet the default.

## 2. Read first (once, offset/limit)
- `CODINGSTYLE.md` (C++ part); `src/junction/planar/CLAUDE.md` (line sides, env knobs, `PLANAR_IIS`).
- `planar_regions.h/.cpp` (whole): how faces are walked and stored; what a face's boundary entry holds (line, stretch, side).
- `planar_components.cpp`: the up-front `sep` lambda (~1486-1500, the fan and side rules), `SepOptions` (its row shape: every end of sq at least 2 beyond every end of sp along f), `SolveLazy1`, and how `MappedComposer` exposes `sides`.

## 3. Changes
1. `PlanarLineSides` gains one query, no other change:
```cpp
/** Every ordered pair of distinct lines (p, q) bounding a common face with Side(p, q) != 0, once each. */
std::vector<std::pair<uint32_t, uint32_t>> FacePairs() const;
```
2. In `MappedComposer::Build`, after the up-front clash rules, when `PLANAR_FACESEPS` is set (tag `face sep <p#i> <q#j>` via `TagScope`): for each `(p, q)` from `FacePairs()` with p's index < q's (each unordered pair once), for every segment `sp` of strand p and every segment `sq` of strand q (the lines' own strands only, no copy strands), add the separation rows with direction `f` = `Right(sq.d)` scaled by `-Side(p, q)` (so p stays on its side of q). Use the same row shape as `SepOptions` (each end of sq at least 2 beyond each end of sp along f).
   - Mode `PLANAR_FACESEPS=all`: every segment pair as above.
   - Mode `PLANAR_FACESEPS=adjacent`: only pairs where sp and sq are the segments nearest the face's shared node or crossing at either end: for each face, its boundary walk gives the vertex where p's stretch and q's stretch meet (a split, merge, crossing or port edge); take the first two segments of each line leaving that vertex. If you cannot get the meeting vertex from the face data without new structures, implement only `all` and report.
3. No change to the lazy loop: it still separates whatever clashes remain.

## 4. Measure (output under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/3b/`)
- Baselines first at the launch head: `/home/pete/OpenTTD/tools/junction/pv save build-rel T2` and `... T3`; LL² singly with `PLANAR_TIMES=1`, `timeout 60`: o49t0f1s0, o1t3f0s0, o1t3f0s1, o0t0f0s0 (area, size, ms, round lines, track).
- After, for each mode (`all`, `adjacent`): `PLANAR_FACESEPS=<mode> pv check build-rel T2` and `T3` (diff summary, best area, solved count, wall time); the same LL² variants with `PLANAR_TIMES=1` (area, ms, rounds, clashes in round 0, decided vs trial).
- For any variant that solved at baseline and is infeasible under a mode: run it with `PLANAR_IIS=1` and include the core (first 30 lines) — it tells us which face pairs over-constrain.

## 5. Do not
- Do not make either mode the default, remove the up-front rules or the trial path, or change `Side`. No other knobs.

## 6. Stop conditions
- None for infeasibility: that is the measurement. Stop only if the face data cannot give the pairs at all, or the build breaks something outside the knob (all `pv check` results without the knob must be unchanged: run `pv check build-rel T2` with the knob unset after your change).

## 7. Gates
- `tools/junction/jt fast build-rel '[planarregions]'`, `'[planargaps]'`, `'[planarmodel]'`, `'[planar]'`.
- `src/junction/planar/CLAUDE.md`: one line for `PLANAR_FACESEPS` (experiment) with the measured outcome in one sentence.
- One commit: `Add: Junction planar PLANAR_FACESEPS experiment (region separations up front)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 8. Report (short)
References read; commit; the before/after tables per mode; IIS cores for newly infeasible variants (grouped by pattern if many); gates; `git status --porcelain`, `git log -1 --oneline`. When you have reported, stop.
