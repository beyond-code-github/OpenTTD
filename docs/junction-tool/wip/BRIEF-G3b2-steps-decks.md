# Brief G3b2 (landing, continues G3b): port steps and decks per run, so the track composer solves T2 o1t3f1s0

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (on `wt-tracks`). Run `git switch wt-g3b` (at 87a7e135fa: `WIP: Junction track composer (G3b) not solving`, on d65fd57869). Commit only there; the orchestrator merges it. Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 120 minutes. One heavy job at a time. Tests via `tools/junction/jt`; sweeps via `tools/junction/pv`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.

## Why
G3b (`docs/junction-tool/wip/BRIEF-G3b-composer.md`, read it: this brief changes only what is below and finishes its gates) built T2 o1t3f1s0 but the solve is base infeasible (`<S>/g3b-iis.txt`). Two causes:
1. E and B run straight between port pins 2 tiles apart, and F leaves E and crosses B, which needs 3. DESIGN-tracks §16.3 ("Sideways steps") and D359 (DECISIONS.md, read it): a step at a port pin is an S-bend on the line's port stretch, its direction from the landed gap-step and candidate-step rules.
2. G3b's "one deck per over piece" put B1's fray, channel and join crossings under one bridge longer than the longest middle. A deck holds one run of consecutive crossings of one kind.

## Read first
- `src/junction/planar/planar_gaps.h`: `PlanarGapStep`, `BuildPlanarGapSteps`, `PlanarCandidateStep`, `PlanarCandidateSteps`.
- `src/junction/planar/planar_components.cpp`: `TrackComposer` (yours, on wt-g3b); `MappedComposer::Build` where it reads `variant.steps` bits against `cand_steps` (the bit k ↔ candidate k mapping); `Model::Deck` in `planar_model.h` (n at least 2 from the segment start, the far end at least 2 before the segment end).

## Changes (`TrackComposer::Build` only, plus tests and docs)
### A. Port steps (D359)
After `hd` is built and before the line loop, the composer takes a working copy of each line's heading list and crossing pieces, and adds port steps to it:
- Steps: every `BuildPlanarGapSteps(t, ch, res, why)` entry (fail with `why`), and every `PlanarCandidateSteps(t, ch)` entry k whose bit `variant.steps >> k & 1` is set (as `MappedComposer` maps them), taken as an in-side step with its `toward`.
- A close-up (`fan` false): each line of the step's channel that ends at an out port: with its last heading h (not axis: fail `"a port step on a diagonal piece: " + name`), the list gets `Turn(h, toward)` and then `h` appended. The former last piece keeps its events; the new last piece is the port piece (`STUB`).
- A fan (`fan` true) or a candidate step: each line of the step's channel that starts at an in port: with its first heading h (axis, as above), `h, Turn(h, toward)` are put in front, and every crossing piece of the line moves up by 2. The new first piece is the port piece (`STUB`); the former first piece takes the axis minimum.
- Two steps on the same end of one line: fail `"two port steps on " + name`.
- The diagonal is a required piece at its least length (1); nothing else about it (no equal lengths across lanes, no bound).
Everything after (pieces, prio coverage, crossings) reads the working copy.

### B. Decks per run
Replace step 5's grouping by `(o, po)`: for each line o and piece k, walk o's events on piece k in event order (`sides.Events()[o]` with the working copy's crossing pieces). An over crossing (o is `hd.over[c]`) joins the current deck when the event before it on this piece is an over crossing of the same kind: both channel crossings (in no `group_crossings` list), or both in the same `group_crossings` group; otherwise it starts a new deck. An under crossing is its own unit. Each deck: `M.Cross(..., 4)` per crossing and one `M.Deck` (as G3b). Then, for consecutive units on one piece, in order:
- deck D then deck E: `M.Add(Ge(E.n - D.f, 4))` (each deck's own 2-unit margins, end to end);
- deck D then an under crossing x: `M.Add(Ge(x.b, D.f + 3))`; an under crossing x then deck D: `M.Add(Le(x.b, D.n - 3))` (G3b's under-deck rows, now per neighbour);
- two under crossings: nothing.
G3b's under-deck step 6 and its "an under crossing inside a deck" failure go (the walk replaces them).

## Tests
1. Un-hide the outer test `"planar track composer: T2 o1t3f1s0 builds and solves"` (remove `[.]`): it must pass (ok, `layout.tracks`, under 15 s) and print its line.
2. Extend the inner test: each topology line's strand headings equal its `BuildPlanarLineHeadings` list with the port steps inserted as in A (compute the steps in the test with `BuildPlanarGapSteps` and `PlanarCandidateSteps` on the same variant; the tag's s bit is 0 so only gap steps apply here). Red first: the current code has no steps (record the assertion).

## Stop conditions
- The outer test still does not solve: rerun with `PLANAR_IIS=1` into `<S>/g3b2-iis.txt`, keep it `[.]`, commit as `WIP: Junction track composer (G3b2) not solving`, report the IIS rows grouped as before. Do not add or loosen rows beyond A and B.
- A step lands on a diagonal port piece on T2 o1t3f1s0: report which and stop.

## Gates (G3b's, now in full)
- `tools/junction/jt fast build-rel '[planar]'` and `'[planarcomponents]'` (counts, times).
- The measure of G3b: `PLANAR_COMPOSER=tracks tools/junction/pv run build-rel T2 --out <S>/g3b-pv-T2.txt`, then T3 (report ok / failed by reason, the best by area with area, size, track, wasted, empty, ms, and the largest ms; beside `<S>/g3a-after-T2.txt` and `-T3.txt`).
- `src/junction/planar/CLAUDE.md`: G3b's two facts, with the composer fact reading: "`PLANAR_COMPOSER=tracks` builds each variant with `TrackComposer`: one strand per topology line (split legs start at their in line's end, merge in lines tied), one required piece per heading of `BuildPlanarLineHeadings` at its least length (port pieces `STUB`, a prio coverage piece 2P + 2), plus a port step S-bend (D359) on each line of a gap step's channel (a close-up after the out line's last piece, a fan before the in line's first) and of a candidate step its bit turns on; on each piece, one deck per run of consecutive over crossings of one kind (channel crossings, or one site group's), heads 4, decks at least 4 apart, an under crossing beside a deck 3 outside it; then the shared finish; no copies, no sides per stretch, no ladders yet."
- One commit for the whole of G3b and G3b2: `git reset --soft d65fd57869`, then commit `Add: Junction track composer builds a variant from its line headings (PLANAR_COMPOSER=tracks)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Do not
- Add any row, bound or piece beyond A and B and G3b's list. Change `MappedComposer`, `FinishPlanarModel`, the track map, G2 or the solve. Touch ports 4077/4079 or start a game client. Use git stash.

## Report
References read; commit hash; red evidence; gate results (counts, times); the outer test's printed line; the measure tables; any stop; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
