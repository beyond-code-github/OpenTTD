# BRIEF domain-12: configuration X2, a 45-degree crosser bending at the crossing (D414)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes wall. No rerun loops. Never run git stash.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge. (Branch `wip-defaults` holds parked work: do not touch it.)
- Engineer: "we want certainty to come from the unit test suite". D414 (a structure's crosser may cross at its piece's end, a bend at the crossing, as V's L40 does) is checked today only by a T1 test that pins a point copied from one drawing. With `end_node` and `cross_piece` on, that test fails, and it cannot say whether the rules or the test are wrong. X1's crosser crosses at 90 degrees on an axis piece, so its crossings always fall inside a tile, never at a piece end: the suite never sees D414. X2 adds that case.

## 2. Read first
- The harness `src/tests/junction/planar/planar_domain_encoding.cpp` (X1 fixture, configurations, rule-set suffixes `+lead`, `+endnode`, `+crosspiece`, the report).
- `src/junction/planar/planar_domain_map.cpp`.

## 3. Changes
1. **Guard** in `PlanarDomainInputOf`: when `points.size()` differs from the map's track count, return an empty input with the note "points do not match the variant" (today it reads out of range).
2. **X2 fixture** in the harness, a second fixture beside `X1Fixture` (same pattern, its own name): X1's topology and structure texts unchanged; shape text as X1 but `shape B1: 0 1 0` and `at B1: A1@1` (B1 crosses A1's tunnel on its NE piece). If `LoadPlanarShapeVariant` refuses it, or `CheckPlanarShapePromise` reports a crossing-side or crossing-angle breach (H-rules naming the crossing), use `shape B1: 0 -1 0` instead; if both are refused, stop and report both messages. Then `PlacePlanarShapeFlex`; record B1's headings, flex middles and event piece after flex in the report header.
   - Base geometry: B1 from (11,10): N 2 to (11,8), NE 4 to (15,4) (crossing A1's row y = 5 at (14,5)), N 4 to (15,0); or the mirror with NW for `0 -1 0` (N 2, NW 4 to (7,4), N 4). Put each run on the first piece of its heading at or after the previous run's piece, with the crossing run on the event piece. A1, AC1, C1, BD1, D1 as X1's base. `REQUIRE` the base is on the grid, domain-legal and conformant.
3. **X2 configurations** (same classes and report as X1, a section `== X2 ... ==` per configuration and in the totals of a separate X2 report):
   - **C1, the crossing piece and its neighbours:** B1's event piece length in {1..6} (diagonal, so every half tile), each B1 piece within two of it on either side in {0..4} (axis pieces even only), B1's last piece = what is left to reach y <= 0 rounded up to the grid (or a fixed 4 if simpler: say which).
   - **C2, near the tunnel's end:** A1's span piece (its event piece) start moved by A1's pieces before it in {0, 2, 4} and its length in {8, 10, 12, 14}, B1 at base.
   - Report file: env `PLANAR_DOMAIN_REPORT_X2` (a second path); the X2 runs use the same `PLANAR_DOMAIN_RULES`.
4. The fast test: "planar domain: X2 loads and its base is legal" (`[junction][planar][planardomain]`): the fixture loads, its base is domain-legal, conformant, and accepted by the rows with the rule set `span+lead+endnode+crosspiece`. If the rows reject the base under that rule set, the test records the failing rows in its message and FAILS: that is the D414 finding, stop and report it (do not change a rule).

## 4. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5`; `tools/junction/jt gate build-rel`.
- Release, X2 with `span+lead`, `span+lead+endnode`, `span+lead+crosspiece`, `span+lead+endnode+crosspiece` (report files `encoding-x2-<set, + as _>.txt` in docs/junction-tool/wip/probes/domain/), timeout 300 each.
- One commit: `Add: Junction: domain configuration X2, a 45-degree crosser bending at the crossing (D414, D428)`, Co-Authored-By Claude Sonnet 5.5.

## 5. Report (short)
Commit; which B1 shape loaded; B1's headings/flex/event piece after flex; red evidence for the guard (a call with a wrong-sized points list, before: crash or out-of-range; after: the note); per rule set the X2 totals (other shape, both accept, both reject, too strict, too loose, undecided) with group keys, counts and first examples; suites; state. When you have reported, stop.
