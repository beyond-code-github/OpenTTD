# BRIEF domain-6: structure end runs in the domain (portal and ramp)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. No rerun loops. Never run git stash.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- The X1 suite now decides every polyline (domain-5c). Its 79 too-strict polylines (`[end]`, `[bound | end]`, `[cross | end]`) come from the program's `end` rows. They are right and the domain is too loose: a tunnel on flat ground needs its portal and one ramp tile straight on its axis at each end (D155, D365; Shape's `end_run`: bridge 1 tile, tunnel 2), and the domain gives every structure a single head piece.

## 2. Read first
- `src/junction/planar/planar_domain.h/.cpp` (structures: span, head, decks, under), `src/junction/planar/planar_domain_map.cpp` (how `first`/`last` are derived), `src/junction/planar/planar_shape_layer.h` (`PlanarShapeStructure::end_run`, index 0 bridge, 1 tunnel).
- Tests: `src/tests/junction/planar/planar_domain.cpp` (hand cases 7, 15-19), `planar_domain_map.cpp` (X1 structure first 2, last 4).

## 3. Changes
- `PlanarDomainStructure` gains `int ends = 1; ///< Pieces at each end that belong to the structure: a bridge's head (1), a tunnel's portal and ramp on flat ground (2) (D155, D365).` `first` and `last` stay the outermost pieces; the `ends` pieces at each end are its end pieces, the pieces between are middles.
- `CheckPlanarDomain`: the span rule needs `last - first + 1 >= 2 * ends + 1`; the head rule ("head") covers every end piece (exclusive tile); straightness and the limit (middles = `last - first + 1 - 2 * ends`) as now. Doc comments say so.
- `PlanarDomainInputOf`: `ends = v.structures[s].end_run[tunnel ? 1 : 0]` (for a copy deck the same); `first` = least crossed index minus `ends`, `last` = greatest plus `ends`; "no room for a head" becomes "no room for its end runs".
- Update the CLAUDE.md **Domain** and `PlanarDomainInputOf` bullets in `src/junction/planar/CLAUDE.md` with "end runs (a bridge's head, a tunnel's portal and ramp)".

## 4. Tests
- Domain hand cases: existing ones keep `ends = 1` (bridges). Add: a tunnel (`tunnel = true`, `ends = 2`) over a listed crosser with two straight pieces at each end: legal; the same with an unlisted Y track through the ramp piece (second end piece): exactly `{"head"}`; the same with the outer end piece on a diagonal: exactly `{"span"}`. Red: the old rule passes the second and third.
- X1 adapter test: the structure now has `ends` 2, `first` 1, `last` 5 (was 2, 4); its shifted case keeps its note (now "no room for its end runs").

## 5. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5`; `tools/junction/jt gate build-rel` (one rerun if only the LL² joint case fails).
- Release `plain+lead` and `span+lead` (overwrite), timeout 300.
- One commit: `Change: Junction: the domain knows structure end runs, a tunnel's portal and ramp (D428)`, Co-Authored-By Claude Sonnet 5.5.

## 6. Stop conditions
- `end_run` for X1's tunnel is not 2; anything not covered.

## 7. Report (short)
Commit; red evidence; per set the totals line (both accept, both reject, too strict, too loose, undecided) and group keys with counts and first examples; suites; state. When you have reported, stop.
