# Brief G3b (landing, the risky one): the track composer builds and solves T2 o1t3f1s0

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (on `wt-tracks`). Run `git switch -c wt-g3b feature/junction-tool` (at the hash given in your launch prompt). Commit only there; the orchestrator merges it. Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 150 minutes. One heavy job at a time. Tests via `tools/junction/jt`; sweeps via `tools/junction/pv`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.

## Why
DESIGN-tracks §16.7 and §16.8 step G3 (read both): a second composer builds every line as one segment per heading from `BuildPlanarLineHeadings` (G2), with only the ruled rows, beside `MappedComposer`, selected by `PLANAR_COMPOSER=tracks`. This step is the core only: no copies (G3c), no rows following the lane (G3d: the prio row stays today's straight `PrioRow`), no sides per stretch (G4), no ladders (G6).

## Read first
- `CODINGSTYLE.md` (C++ part).
- `src/junction/planar/planar_model.h`: `Model::NewStrand`, `End`, `AddPiece`, `Node`, `Tie`, `Cross`, `Deck`, `CrossRec`, `DeckRec`, `PrioRow`, `Fail`.
- `src/junction/planar/planar_components.cpp`: `PlanarPortIn`, `FinishPlanarModel`, `MappedComposer` (constructor, public members `M`, `sides`, `ladder_faces`, `ladder_off`; `Build` from its start to the end of `/* Depth order ... */`, and the `/* An under track is at ground ... */` block), `VariantOutcome`, `SolveComposedVariant`, `SolveMappedVariant`, `PickBest` (where `layout.program_shape` is set), `SolvePlanarMappedVariant`, `ParseMappedTag`, `MakeParams`, `STUB`.
- `src/junction/planar/planar_paths.{h,cpp}`: `PlanarLineHeadings`, `BuildPlanarLineHeadings` (rule 0 `legs_of`, rule 4's `over`).
- `src/junction/planar/planar_regions.h`: `BuildPlanarTrackMap`, `PlanarLineSides::Events()`.
- `src/tests/junction/planar/planar_tracks.cpp` (the line headings tests) and `src/tests/junction/planar/planar_components.cpp` (how T2 tests build topology and spec, ~130-150).

## Changes
### 1. `planar_paths.{h,cpp}`: two outputs G2 already computes
- `PlanarLineHeadings` gains `std::vector<uint32_t> over; ///< Per topology crossing, its over line.` and `std::vector<std::pair<uint32_t, int>> legs; ///< Per topology node, rule 0: its straight leg (UINT32_MAX for none) and the other leg's side.`
- Fill `over` for every topology crossing: a site crossing with the line rule 4 picks; a channel crossing with its line in the channel `paths.over` names for the channel crossing whose `line_crossings` hold it. Fill `legs[n]` with `legs_of(n)` for every node. No other change.
- Test (extend the existing outer line-headings test in `planar_tracks.cpp`, per variant with empty `why`): `over.size()` is the crossing count and each `over[c]` is one of crossing c's two lines and is the line the test's existing over check accepts; `legs.size()` is the node count.

### 2. `planar_components.cpp`: `TrackComposer` (file-local class, after `MappedComposer`, before `SolveComposedVariant`)
Same constructor signature and body shape as `MappedComposer` (`M.params = params; M.deck_parity = true; Build();`). Public members with the same names and types as `MappedComposer`'s that `SolveComposedVariant` reads: `M`, `sides` (constructed `sides(topology, channels)`), `ladder_faces` and `ladder_off` (left empty). Private: what `Build` needs. `Build()`, in this order (fail = `M.Fail(text); return;`):
1. `sides.Why()` non-empty: fail with it. `paths = BuildPlanarPaths(t, ch, variant.over, shape)`: fail with `paths.why`. `M.X0 = M.NewVar() * 2; M.Y0 = M.NewVar() * 2;` (as `MappedComposer::Build`).
2. `res = ResolvePlanarSites(t, ch, variant.ties, why)`: fail with `why`. `map = BuildPlanarTrackMap(t, ch, res, paths.over, false)` (form A): fail with `map.why`. `hd = BuildPlanarLineHeadings(t, ch, paths, res, sides.Events(), map)`: fail with `hd.why`.
3. In ports: `PlanarPortIn(M, name, port.side)` for every line of every in port (as `MappedComposer::Build`).
4. Lines in depth order (the same `depth_of` as `MappedComposer::Build`; equal depths by line index). For line l (name `nm`, headings `hd.lines[l].headings`):
   - From a node n (`!from.port`): a split (one in line `i`): `M.NewStrand(nm, M.End(name of i))`; a merge (two in lines `i0`, `i1`): `M.NewStrand(nm, M.End(name of i0))` and `M.Tie(M.End(name of i1), M.End(name of i0))`. Once per node: `M.Node({in lines..., out lines...})` with every in and out line of n.
   - One `M.AddPiece(nm, h, lo)` per heading h, in order, keeping each `Seg` in `segs[l][k]`. `lo` = 2 for an axis piece, 1 for a diagonal, raised to `STUB` for the first piece of an in-port line and the last piece of an out-port line, and to `2 * P + 2` (D108) for the prio coverage piece below.
   - Prio coverage: line l ends at a merge node m with `hd.legs[m].first == l`, a prio route runs on l and none on m's other in line: l's last piece is the coverage piece (fail `"a prio coverage on a diagonal piece: " + nm` when it is not axis); after its pieces, `M.prio_rows.push_back({nm, M.End(nm), last heading, -hd.legs[m].second})`.
5. Crossings: for every topology crossing c with `o = hd.over[c]`, `u` the other line: `po` = `hd.lines[o].crossing_piece` at c's index in `sides.Events()[o]`, `pu` likewise on u. Group the crossings by `(o, po)`; per group, `xs` = `M.Cross(segs[o][po], segs[u][pu], 4)` for each crossing in it, then `M.Deck(segs[o][po], xs)`; remember per group its deck index and the crossings it holds, and per crossing its `M.crossings` index.
6. Under-deck (a piece both under and over): for a crossing c whose under piece `(u, pu)` holds a deck D (a group keyed `(u, pu)`): with `e` = c's index in `sides.Events()[u]` and `E` = the indices there of D's crossings: `e < min(E)`: `M.Add(Le(crossing.b, deck.n - 3))`; `e > max(E)`: `M.Add(Ge(crossing.b, deck.f + 3))`; otherwise fail `"an under crossing inside a deck on " + name of u`. (The same rows as `MappedComposer`'s under-deck block, ordered by the line's events.)
7. `FinishPlanarModel(M, t, P, routes, outline);`

### 3. Selection and the outcome flag
- `VariantOutcome` gains `bool tracks = false; ///< Whether the track composer built it.`; `PlanarComponentLayout` (header) gains `bool tracks = false; ///< Whether the track composer built the winning variant (PLANAR_COMPOSER=tracks).` after `program_shape`.
- `SolveComposedVariant` sets `out.tracks = std::is_same_v<C, TrackComposer>;` at its start. `PickBest` sets `layout.tracks = best->tracks;` beside `layout.program_shape`.
- `SolveMappedVariant`: where it returns `SolveComposedVariant<MappedComposer>(...)`, it returns `SolveComposedVariant<TrackComposer>(...)` instead when `std::getenv("PLANAR_COMPOSER")` is `"tracks"`.
- Test hook, declared in `planar_components.h` beside `SolvePlanarMappedVariant` with a doc comment: `std::string PlanarTrackComposerStrands(const PlanarTopology &topology, const JunctionSpec &spec, const std::string &tag, std::map<std::string, std::vector<int>> &strands);`: parses the tag and flips channels exactly as `SolvePlanarMappedVariant` does, builds a `TrackComposer` (shape null), fills `strands` with every strand name of `M.order` mapped to its segments' headings, and returns `M.error`.

## Tests (in `src/tests/junction/planar/planar_components.cpp`, tags as the T2 mapped tests; each red before its code, record the failure)
1. Outer: `"planar track composer: T2 o1t3f1s0 builds and solves"`: `setenv("PLANAR_COMPOSER", "tracks", 1)` (unset at the end, also on failure: a small RAII guard local to the test), `SolvePlanarMappedVariant(topology, spec, "o1t3f1s0", nullptr, layout, &why)`: REQUIRE ok, `layout.tracks`, area > 0, the solve under 15 s; print `tracks T2 o1t3f1s0 area <a> size <WxH> track <t> wasted <w> empty <e> ms <ms>`. Expected red: `layout.tracks` false (then the member, the flag and the selection, then green).
2. Inner: `"planar track composer: T2 o1t3f1s0 strands are the line headings"`: `PlanarTrackComposerStrands(...)` returns empty; every topology line has a strand whose headings equal its `BuildPlanarLineHeadings` list (compute it in the test as `planar_tracks.cpp` does for this variant); every other strand's name starts with `prio:`, and there is at least one. Expected red: write the hook first returning `"not built"` and record the assertion.

## Measure (after the gates; no assertion, report only)
`PLANAR_COMPOSER=tracks tools/junction/pv run build-rel T2 --out <S>/g3b-pv-T2.txt`, then the same for T3. Report per hub: variants ok / failed (failures grouped by reason text), the best by area with its area, size, track, wasted, empty, ms, and the largest ms. Compare with `<S>/g3a-after-T2.txt` and `-T3.txt` (MappedComposer). Do not run LL².

## Do not
- Add any row, bound or piece beyond the list in 2 (no stubs beyond `STUB` on port pieces, no fray gap, fan, spread, entry gap, bundle offset, side or fan rule, no optional segment). Change `MappedComposer`, `FinishPlanarModel`, the solve, the track map or G2's rules. Touch ports 4077/4079 or start a game client. Use git stash.

## Stop conditions
- The outer test's variant fails to build or solve: rerun it with `PLANAR_IIS=1` (output to `<S>/g3b-iis.txt`), commit the work as `WIP: Junction track composer (G3b) not solving` on wt-g3b with the outer test tagged `[.]`, and report the failure text and the IIS rows. Do not add or loosen rows to make it solve.
- It solves but over 15 s: the same commit and report, with `PLANAR_TIMES=1` output to `<S>/g3b-times.txt`.

## Gates
- Both tests red then green; `tools/junction/jt fast build-rel '[planar]'` and `'[planarcomponents]'` (counts and times; `[planarcomponents]` is known to run over its 5 s budget: report the time).
- `src/junction/planar/CLAUDE.md`, "Component solver": one fact: "`PLANAR_COMPOSER=tracks` builds each variant with `TrackComposer`: one strand per topology line (split legs start at their in line's end, merge in lines tied), one required piece per heading of `BuildPlanarLineHeadings` at its least length (port pieces `STUB`, a prio coverage piece 2P + 2), one deck per over piece holding all its crossings (heads 4), an under crossing on a piece holding a deck outside it by the line's event order, then the shared finish; no copies, no sides per stretch, no ladders yet." In "Track map", the `BuildPlanarLineHeadings` fact gains: it also gives each crossing's over line and each node's straight leg and side.
- One commit: `Add: Junction track composer builds a variant from its line headings (PLANAR_COMPOSER=tracks)`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
References read; commit hash; red evidence; types, members and functions added; gate results (counts, times); the outer test's printed line; the measure tables; any stop; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
