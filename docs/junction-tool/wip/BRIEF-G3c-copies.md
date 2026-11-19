# Brief G3c (landing): the track composer builds the map's copies as parallelograms

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (on `wt-tracks`). Run `git switch wt-g3b` (at 25ce5fd099: G3b and G3b2 as two WIP commits on d65fd57869). Commit only there; the orchestrator merges it. Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- G3b (`wip/BRIEF-G3b-composer.md`) and G3b2 (`wip/BRIEF-G3b2-steps-decks.md`) are read-first: `TrackComposer` builds T2 o1t3f1s0 but the solve stops at "a clash is not separable": B1's straight prio row lies on B2's side, the B lanes being one lane apart from their port to the join (`<S>/g3b2-iis.txt`). The copies of this step are what spread lanes in `MappedComposer` (D351: each doubled crossing moves the line one track toward its copy side; B's far lane copies outward, the other away from it). The outer test (`[.]` now) is this step's gate too.
- Time box: 120 minutes. One heavy job at a time. Tests via `tools/junction/jt`; sweeps via `tools/junction/pv`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.

## Why
DESIGN-tracks §16.7 ("a copy as a `CopyRec` over its range on the map's copy track with the D351/D312 parallelogram and paired decks") and §16.8 G3. Rulings to read in `docs/junction-tool/DECISIONS.md`: D351 (parallelogram only: the line ends on its copy's track), D312 (equal bridges, one-tile stagger, tight near heads), D118, D145, D322 (doubling in series allowed). G3b built every crossing single; full-share lines must double over the crossings where they are over (the map already holds these copies: `PlanarTrack::Kind::Copy`).

## Read first
- `CODINGSTYLE.md` (C++ part).
- `src/junction/planar/planar_components.cpp`: `TrackComposer` (all of it: its working copies `heads` and `cpiece` hold each line's headings with port steps and its crossing pieces; the per-run deck walk); `MappedComposer::StartCopy`, `MappedComposer::Double`, `MappedComposer::PairDecks` (the parallelogram and its paired decks: the pattern this step reuses); `CopyRec` use in the export (`M.copies`).
- `src/junction/planar/planar_regions.h`: `PlanarTrack`, `PlanarTrackMap` (`tracks`, `expanded`, `events`, `crossings`, `Side`).
- `src/junction/planar/planar_paths.h`: `PlanarLineHeadings::beside`.
- The G3b tests in `src/tests/junction/planar/planar_components.cpp`.

## Definitions
For every map track i of kind `Copy` (parent line `p = tracks[i].line`, `hd.beside[i] = {p, k_first, k_last}`):
- **Piece:** `k = k_first`, moved up by 2 when the parent's working copy has a fan or candidate port step in front (the `beside` pieces count G2's list); `k_first != k_last` fails `"a copy over several pieces: " + name` (a later step).
- **Side:** `sg = map.Side(i, 0, q, 0)`, q the track in `expanded.nodes[expanded.lines[i].from.index].out` other than i. `sg == 0` fails `"a copy with no side: " + name`.
- **Its crossings:** the topology crossings the copy mirrors: for each map crossing in `map.events[i]`, the topology crossing of p with the same partner line (as G2's rule 6 maps them). Each must be over on p (`hd.over[c] == p`) and sit on piece k of p, else fail `"a copy crossing not over on its piece: " + name`; they must be consecutive in p's events, else fail `"a copy's crossings not consecutive on " + name of p`.
- **Doubled piece (one or more copies, D322 doubling in series):** a piece k of p (heading h, which must be axis, else fail `"a copy on a diagonal piece: " + name`) holding copies C_1 .. C_m (sorted by their first crossing in p's events) is cut into `a_0, d0_1, step_1, a_1, d0_2, step_2, a_2, ..., d0_m, step_m, a_m`: each `a_j` an axis piece h (least 2; `a_0` raised to `STUB` when k is an in-port line's first piece; `a_m` raised to `STUB` when k is an out-port line's last piece and to `2 * P + 2` when k is the prio coverage piece: the coverage runs from the last rejoin, D108), each `d0_j` h (least 2), each `step_j` `Turn(h, sg_j)` exactly 2. Copy C_j's strand (named its track's name) starts at `a_{j-1}`'s end (`M.NewStrand`, `M.Node({p's name, copy})`), takes `Turn(h, sg_j)` exactly 2 then `d1_j` (h, least 2), and its end is tied to `step_j`'s end; `M.copies.push_back({copy, p's name, d0_j.i, d0_j.i + 2})` (as `Double`). The line stays on its copy's track after each (D351; no step back).
- **Other events on a doubled piece:** p's events on piece k that are not in any copy's crossings (pieces read from the working copy `cpiece`) sit on the `a_j` between the copies they fall between in p's event order (`a_0` before C_1's first crossing, `a_m` after C_m's last). This holds for p as over (its deck walk then runs on that `a_j`) and as under (the crossing's under segment is that `a_j`).
- **Decks:** the copy's crossings get the paired decks of `MappedComposer::PairDecks` on `d0` and `d1`, crossing each crosser's under segment once per deck (heads 4); the per-run deck walk of G3b2 then runs per segment (`a_j`) over the events on it, with each copy's paired decks as one unit in the walk's order (its `d0` deck's n and f for the neighbour rows); make `PairDecks` a file-local free function `static void PlanarPairDecks(Model &M, const Seg &d0, const Seg &d1, const std::vector<Seg> &crossers, int h)` (body unchanged, `M` for `this->M`, doc comment moved) used by both composers, as G3a did for `PortIn`.

## Changes
1. `PlanarPairDecks` as above; `MappedComposer::PairDecks` removed and its callers call the free function.
2. `TrackComposer::Build`: the line loop builds a doubled piece as defined (all of a line's copies are known before its pieces are added: compute them from the map before the loop); the crossing step routes each crossing to its segment (`segs` per piece becomes the segment holding each event: keep the G3b names, add only what this needs) and builds the copy's crossings with `PlanarPairDecks` instead of a single deck.
3. The prio row (`M.prio_rows`) is unchanged; its merge point is the coverage line's end as before.

## Tests (in `planar_components.cpp`; red before the code, record the failure)
1. Inner, extend `"planar track composer: T2 o1t3f1s0 strands are the line headings"`: for every copy track of the variant's map (compute the map in the test as `planar_tracks.cpp` does), a strand of its name exists with headings `{Turn(h, sg), h}`; its parent's strand headings equal the parent's headings with port steps (as the test already computes them) with each doubled piece k replaced by `h` followed by `h, Turn(h, sg_j), h` per copy on it; every other line's strand is unchanged. Expected red: no strand named after the copy.
2. Outer: `"planar track composer: T2 o1t3f1s0 builds and solves"`: remove `[.]`; it must pass (ok, `layout.tracks`, under 15 s) and print its line.
3. New outer: `"planar track composer: T2 o1t3f1s0 passes the rule checker on doubled bridges"`: with `PLANAR_COMPOSER=tracks`, solve, render the layout the way the existing T2 mapped checker test does (find it: the test that calls `CheckJunctionRules` on a mapped T2 layout), and REQUIRE no violation whose text names a bridge not being doubled; print every other violation's text grouped by count (no assertion on them: rows and sides are later steps). Expected red: the doubled-bridge violation.

## Measure (report only)
`PLANAR_COMPOSER=tracks tools/junction/pv run build-rel T2 --out <S>/g3c-pv-T2.txt`, then T3. Per hub: ok / failed (by reason text), best by area (area, size, track, wasted, empty, ms), largest ms; beside `<S>/g3a-after-T2.txt` and `-T3.txt` (MappedComposer).

## Do not
- Add any row or bound beyond the definitions. Change `MappedComposer`'s behaviour (the `PairDecks` move is identity: run `pv` T2 without the switch and diff against `<S>/g3a-after-T2.txt`: no column but ms may change), `FinishPlanarModel`, the track map, G2 or the solve. Touch ports 4077/4079 or start a game client. Use git stash.

## Stop conditions
- The outer variant does not solve: rerun with `PLANAR_IIS=1` into `<S>/g3c-iis.txt`, commit as `WIP: Junction track composer copies (G3c) not solving` with the failing outer tests tagged `[.]`, report the IIS rows grouped (which strands, which rows, `extra` rows first). Do not loosen rows, and do not touch the prio row (`M.prio_rows`): a row that follows its lane is a later step.
- A copy track of T2 o1t3f1s0 fails one of the definition's checks: report which and stop.
- Start from the first attempt's patch: `git apply <S>/g3c-wip.patch` on wt-g3b (25ce5fd099), read it, then change its doubled-piece build from one copy per piece to the definition above (its guard "two copies on one piece" goes). Its fan shift of k stays (now in the definition).

## Gates
- Tests red then green; `tools/junction/jt fast build-rel '[planar]'` and `'[planarcomponents]'` (counts, times); the MappedComposer identity diff above.
- `src/junction/planar/CLAUDE.md`: G3b's "Track map" fact (BuildPlanarLineHeadings also gives each crossing's over line and each node's straight leg and side) and, in "Component solver", the composer fact of `wip/BRIEF-G3b2-steps-decks.md` with "no copies" replaced by: "each map copy is a parallelogram on its parent's one over piece (pre, deck, step to the copy's side, post; the copy steps then decks; paired decks as `PlanarPairDecks`), other events on that piece before or after it by event order".
- One commit for G3b, G3b2 and G3c: `git reset --soft d65fd57869`, then commit `Add: Junction track composer builds a variant from its line headings (PLANAR_COMPOSER=tracks)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit hash; red evidence; functions added and removed; gate results (counts, times); the identity diff; the outer tests' printed lines and the checker's other violations by count; the measure tables; any stop; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
