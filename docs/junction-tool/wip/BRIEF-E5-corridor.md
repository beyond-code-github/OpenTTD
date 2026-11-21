# BRIEF E5-corridor: beside tracks as offset curves in the drawing program (Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head: given in the launch message. Release build `build-rel`, `-j 4` only.
- Source: DECISIONS D421 (read it in full: the design), D409 (3), SPEC-slots-lengths §11. The hidden recon test "planar draw: beside tracks and their parent chains (corridor recon)" (end of `src/tests/junction/planar/planar_draw.cpp`) shows T1/T2's beside tracks: one-piece prio rows, same heading as their one-track parent chain, between two row nodes, no flex.

## 2. Read first (once each, offset/limit)
- `src/junction/planar/planar_draw.cpp`: lines 280-360 (points, lengths, piece rows), 425-436 (node rows), the `eq` and `unsupported` helpers (about lines 200-260).
- `src/junction/planar/planar_shape_layer.h` line 80 (`ParentChain`) and `planar_shape_layer.cpp` lines 130-152.
- `src/junction/planar/planar_regions.h` lines 100-108 (`part_of`, `Side`).
- `src/junction/planar/planar_model.h` (`PlanarModel::STEP`).
- `src/tests/junction/planar/planar_draw.cpp` lines 40-120 (`ShapeFirst`, `PlanarTText`).

## 3. Tests (outermost first; red before green), tag `[junction][planar][planardraw]`, after the curves tests
1. **"planar draw: a prio row runs one lane beside its parent (corridor)"** (outer). For lanes 1 and 2: `ShapeFirst(PlanarTText(lanes), 1)`, solve `{.clearance = false, .corridor = true}` (5 s), REQUIRE feasible. For each beside track b (`part_of[b] == UINT32_MAX`) with chain (`ParentChain`) and s = `map.Side(b, 0, chain[0], 0)` (REQUIRE s != 0): CHECK `points[b].front() == node point + PlanarBesideOpening(h0, s)` where the node point is the chain's first point (`points[chain[0]].front()`) and h0 = b's first piece heading mod 8; CHECK `points[chain.back()].back() == points[b].back() + PlanarBesideClosing(hn, s)`; for every interior vertex k (concatenated chain vertices, a shared node once), CHECK Dot(P_b,k - P_chain,k, N(h)) == 2 s for both headings meeting at k (N(h) = STEP[(h + 2) mod 8]). Red: with the option field and the two functions in place but no rows, the front CHECK fails (the row starts at the node).
2. **"planar draw: the opening and closing steps are one lane"** (inner, the two functions). Do not restate the code's formula in the test. Check two literal cases, `PlanarBesideOpening(2, 1) == {2, 2}` and `PlanarBesideOpening(5, 1) == {0, 2}`, and the properties for h in 0..7 and s in {1, -1}: Dot(opening, N(h)) == 2 s, Dot(closing, N(h)) == -2 s, and both are 2 STEP of a heading one eighth off h (|Rot(h, heading)| == 1). Red: the functions declared returning {0, 0} first.
3. **"planar draw: the T draws with keep-apart and corridors"** (outer, behaviour): lanes 1 and 2, `{.pairwise = true, .corridor = true}`, 15 s each: REQUIRE status "Optimal". Record (do not assert) the same with `.corridor = false` in the report. If either corridor run is not Optimal in 15 s, STOP and report status and conflict tags.

## 4. Changes
a. `planar_draw.h`: `bool corridor = false; ///< The corridor family (D421): beside tracks (copies, rows) as offset curves of their parent chain one lane to their side, with an exact one-lane opening and closing step at their nodes.` directly after `curves`. Two free functions, declared after `PlanarDrawOptions`, with Doxygen:
   - `std::array<int, 2> PlanarBesideOpening(int heading, int side);` returns `2 * PlanarModel::STEP[(heading + side + 8) % 8]` (heading mod 8 first);
   - `std::array<int, 2> PlanarBesideClosing(int heading, int side);` returns `2 * PlanarModel::STEP[(heading - side + 8) % 8]`.
b. `CheckPlanarDrawable`, when `options.corridor`:
   - before the node rows, per beside track b: `ParentChain` (false: `return unsupported("corridor: no parent chain for <name>")`); if b's pieces differ from the concatenated chain's pieces (mod 8) or b or any chain track has flex or steps: `return unsupported(...)` naming it; s = `map.Side(b, 0, chain[0], 0)`, 0: `unsupported`.
   - node rows: at b's from-node, b's front enters the node rows as front minus the opening (eq rows with constant: front[axis] - node[axis] = opening[axis]); at b's to-node, back plus the closing (node[axis] - back[axis] = closing[axis]). The node point is the end of a non-beside line at that node; if every line there is beside, `unsupported`. Worked call site for the from-node (axis loop): `eq({{point[b].front()[axis], 1}, {node_end[axis], -1}}, opening[axis], fmt::format("corridor {} open", map.tracks[b].name));` and skip b's front in that node's plain `node` rows.
   - interior vertices: for k = 1 .. pieces - 1, the chain vertex k (concatenate the chain tracks' points, dropping each later track's first point), for each distinct heading h of pieces k-1 and k: `eq(Dot(point[b][k] - chain_point[k], N(h)) terms, 2 * s, "corridor <name> <k>")`.
c. File comment: drop "beside tracks as offset curves" from "Not in it yet", add "corridors (option)".
d. Remove nothing; keep the recon test.

## 5. Do not
- No default change (`corridor` stays false); no change to Shape, Structure, the curve or bay rows, or the keep-apart code.
- No flex following (SPEC §11's synced inserts): beside tracks with flex are unsupported in this step.
- No copy-deck special cases: a copy is handled like a row by these rows; if T1/T2's first variants contain a copy that the rows cannot hold, STOP.

## 6. Stop conditions
- Test 3 not Optimal in 15 s with corridors.
- A beside track on T1/T2's first variants is unsupported by 4 b.
- Anything outside `planar_draw.h`, `planar_draw.cpp`, `src/tests/junction/planar/planar_draw.cpp` seems needed.

## 7. Gates
- `tools/junction/jt fast build-rel '[planardraw]' 5`; `tools/junction/jt gate build-rel` (30 s). Known flake: "planar components: T2 mapped lands in its box at the anchor"; rerun alone and report.
- One commit: `Add: Junction: beside tracks as offset curves in the drawing program (option)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 8. Report (short, fixed format)
- References read
- Commits: hash and subject
- Red evidence: each new test and its failure before the rows existed
- Types and members added or removed: names only
- Suite results (and test 3's corridor = false statuses, recorded)
- Stops and anything left undone, with the reason
- State: `git status --porcelain` and `git log -1 --oneline`

When you have reported, stop.
