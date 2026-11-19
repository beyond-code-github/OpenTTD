# Brief G3a (refactor, identity): the composer-independent rows and the variant solve become shared

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (on `wt-tracks`). Run `git switch -c wt-g3a feature/junction-tool` (at 665852e0b5). Commit only there; the orchestrator merges it. Build dir `build-rel`, target `openttd_test` only. Finish with `git switch wt-tracks` and rebuild `openttd_test` in build-rel.
- Time box: 90 minutes. One heavy job at a time. Tests via `tools/junction/jt`; sweeps via `tools/junction/pv`.
- S = `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad`.

## Why
DESIGN-tracks §16.7-16.8 (G3): a second composer (the track composer, next step) builds the same model rows for ports, box, routes, bay length and curves, and is solved by the same variant solve. This step makes those shareable with no behaviour change. It is a refactor: every output must be identical.

## Read first
- `CODINGSTYLE.md` (C++ part).
- `src/junction/planar/planar_components.cpp`: `MappedComposer` (members `M`, `sides`, `ladder_faces`, `ladder_off`, `routes`, `outline`, `P`; `PortIn` ~964); `MappedComposer::Build` from the comment `/* Out ports on the boundary. */` (~1816) to the `Composer::RouteCurves(M, this->routes, M.params, std::sqrt(2.0));` call (~1965), and the in-ports loop (`/* In ports. */`, ~1310); `SolveMappedVariant` (~2724) to its end.

## Changes (planar_components.cpp only)
1. A file-local free function `static void PlanarPortIn(Model &M, const std::string &name, PlanarSide side)` with `MappedComposer::PortIn`'s body (M for `this->M`); `MappedComposer::PortIn` is removed and its callers call `PlanarPortIn(this->M, ...)`.
2. A file-local free function `static void FinishPlanarModel(Model &M, const PlanarTopology &t, int P, RouteList &routes, RouteList &outline)` holding, verbatim, the code of `MappedComposer::Build` from `/* Out ports on the boundary. */` through the `Composer::RouteCurves(...)` call, with `this->P`, `this->routes`, `this->outline` replaced by the parameters and every other name kept (move every comment with the code). `Build` calls `FinishPlanarModel(M, t, this->P, this->routes, this->outline);` there. If that range uses any other member of `MappedComposer` (for example `ladder_faces`), stop and report which and where.
3. `SolveMappedVariant`'s part from `MappedComposer composer(topology, channels, params, variant, shape);` to its end moves into `template <class C> static VariantOutcome SolveComposedVariant(...)` taking what that part uses (the same parameters plus `VariantOutcome out`), with `MappedComposer` replaced by `C` (including `std::make_unique<MappedComposer>` and `std::unique_ptr<MappedComposer>`). `SolveMappedVariant` keeps its shape handling and returns `SolveComposedVariant<MappedComposer>(...)`. The `PLANAR_DRY` block stays in `SolveMappedVariant` before the call if it cannot move unchanged; say which you did.
4. Nothing else changes; no new comments except a doc comment on each new function.

## Gates (identity)
1. BEFORE the change, on wt-g3a: `pv run build-rel T2 --out <S>/g3a-before-T2.txt`, then T3, then LL2 with `--tags <S>/ll2-12.txt`.
2. After: the same into `...-after-...` and `pv diff` each pair: no variant's area, size, track, wasted, empty or reason may change (ms may). Any change is a stop: report the variants.
3. `tools/junction/jt fast build-rel '[planar]'` and `'[planarcomponents]'` (counts and times; `[planarcomponents]` is known to take ~14 s against its 5 s budget: report the time, it is not your stop).
- One commit: `Codechange: Junction composer-independent model rows and variant solve shared`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Do not
- Change any row, bound, order of construction, or the solve. Touch other files. Touch ports 4077/4079 or start a game client. Use git stash.

## Report
References read; commit hash; functions added and removed; the identity results (pv diff output per hub); test counts and times; `git status --porcelain`; `git log -1`. Finish with `git switch wt-tracks` and the rebuild. When you have reported, stop.
