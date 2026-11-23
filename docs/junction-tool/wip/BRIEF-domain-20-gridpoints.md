# BRIEF domain-20: F4, every point on an edge midpoint; then Y1 (the copy)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes wall. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- **F4 (domain-19):** the program's `grid` option (planar_draw.cpp, the Grid block ~612) seeds only the across-parity of axis runs. It never forces a point to be an edge midpoint (x + y odd). Y1's base drawing (flex-stripped, gridded, Optimal) has node points with x + y even, at the ends of diagonal runs of even length, and the domain rejects every track. The game needs every track point on an edge midpoint.
- Y1's texts are in the domain-19 rulings (copied below).

## 2. Changes
1. **F4, production (planar_draw.cpp, `grid`):** per track point (every point variable pair, after merging shared points), one row `x + y - 2 h = 1` with `h` a new integer variable (group "other"), tag `grid point <track> <k>`, written when `grid` is on (and not elastic). If the existing parity pass (2 h + p rewriting) needs these as seeds rather than rows, add them as the pass expects: say which you did. CLAUDE.md grid bullet: "every point lies on an edge midpoint (x + y odd, `grid point`)".
2. **Y1 fixture** (harness, through `DrawnFixture` as L1): topology `topology Y1` / `port W in E: AC1` / `port S in N: BD1` / `port N out N: C1, B1` / `port E out E: AD1` / `node AC1 -> C1, A1` / `node BD1 -> B1, D1` / `node A1, D1 -> AD1` / `cross A1: B1<` / `cross B1: A1>`; structure `tunnel T1 A1: B1` / `copy A1 over T1` / `straight AC1: A1` / `straight BD1: B1` / `straight AD1: A1`; Shape by `ShapePlanarStructureVariant`; base from the flex-stripped variant, gridded, domain-checked.
3. **Y1 configurations:** Y1a (the copy's opening: the copy and A1's pieces from the split to the tunnel's first end piece, inserts {0,1,2}, axis even, the run before the tunnel absorbing to keep the crossing) and Y1b (the closing, mirrored). Report corridor and lead row counts (corridor must be above 0).

## 3. Tests
- Fast: "planar draw: grid points lie on edge midpoints": X1's base shifted by (1, 1) (x + y parity unchanged? no: shifting both by one keeps x + y parity; use a shift of (1, 0)) breaks a `grid point` row in the program check with grid on. Red before the change.
- Fast: "planar domain: Y1 loads and its base is legal".

## 4. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5`; `tools/junction/jt gate build-rel` (production change; stop if a previously passing test fails).
- Release once each: X1, X2, M1, K1, L1 (they must stay exact; report any change) and Y1, `span+lead+endnode+crosspiece+grid`.
- One commit for F4 (`Fix: Junction: grid puts every point on an edge midpoint (D382, D428)`), one for Y1 (`Add: Junction: domain configuration Y1, a copy (D428)`), Co-Authored-By Claude Sonnet 5.5.

## 5. Report (short)
Commits; red evidence; per configuration the classes (with group keys, counts, first examples for any non-zero too strict or too loose); corridor and lead row counts for Y1; gate; state. When you have reported, stop.
