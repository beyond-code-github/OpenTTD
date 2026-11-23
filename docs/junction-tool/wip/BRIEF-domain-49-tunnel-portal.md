# BRIEF domain-49: a tunnel's end run is its portal (D437)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes wall. A production change (Shape's derived end run); the wide gate once. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- D437 (engineer, 2026-11-23): tunnels are built V's way: the end run is the portal alone (1 piece), not portal plus a dug ramp (2); Heights owns the lower ground outside each portal. Test-first: behaviour, then implementation.
- Read first: DECISIONS D437, D155, D365, D436; `planar_shape_layer.cpp` ~1348 (`PlanarShapeStructure shape{0, {-1, -1}, {bridge ? 1 : -1, tunnel ? 2 : -1}}`) and ~654 (H5's check of the same); `planar_shape_layer.h` ~37 (`end_run` doc); `planar_domain_map.cpp` ~150-157 (the domain's `ends` from `end_run`); planar_draw.cpp ~1185-1200 (`least_run` from `shape.end_run`); in the test file: the BR1 end-run pinned test and the D435/end_node tests that set `end_run[1] = 2` (~2992, ~3719), the 197 answer key's clamp to 1 (~3920, ~4154).

## 2. Test first (red before the change)
"planar domain: a curve may come right up to a tunnel's portal (D437)" (behaviour, through the judge, `[junction][planar][planardomain]`): on X1 (or TT1/BR1-shaped X1 with a tunnel), a geometry where the separator's piece outside the portal is a diagonal starting exactly one piece from the first crossing's middle (the end run is 1 piece): the domain accepts (with the structure's `ends` taken from the variant as the suite does) and the judge accepts. Before the change: the judge refuses with `end run` (and the domain refuses with `span`): record both. Counterpart: the diagonal starting at the portal piece itself (end run 0): refused by both.

## 3. Change
- Shape: the tunnel end run is 1 in both places (~1348 and H5 ~654); `end_run` doc: "the straight end-run pieces per kind (bridge: the head; tunnel: the portal, the ground outside one level lower (D437); -1 ...)".
- The test file: the 197 answer key's clamps (~3920, ~4154) are now no-ops: remove them with their comments, and say the answer key reads Shape's end runs directly. Tests that SET `end_run[1] = 2` to model the old flat-ground tunnel stay (they are toggle evidence), with their comments updated to say "the flat-ground ramp of D155 (before D437)".
- `src/junction/planar/CLAUDE.md`: every place that says a tunnel's end run is portal plus ramp (2) says portal (1), D437 (D225). Grep the planar CLAUDE.md and `docs/junction-tool/SPEC-*.md` for "ramp" and list the SPEC lines you did not change (docs are mine).

## 4. Measure
- All ten exactness tests: 0 too strict / 0 too loose expected (domain and rows both read the new end run). A too strict or too loose is a finding: report in full.
- The 197 answer key (hidden, Release once): the judge's 17 `end run` refusals should be gone; report the refusals left by rule.

## 5. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5`, `tools/junction/jt fast build '[planardraw]' 30`, the Shape layer's fast tag (find it: `[planarshape]` or as TESTS.md says), `tools/junction/jt gate build-rel` once. A one-off SIGSEGV/SIGILL that passes on rerun: report it, it is the known CPU instability.
- One commit: `Change: Junction: a tunnel's end run is its portal (D437)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 6. Report (short)
References read; commit; red evidence; exactness after; 197 refusals after; SPEC lines left for me; gates; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
