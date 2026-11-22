# BRIEF domain-2b (v2): keep-apart on fixed points, and the encoding report on X1

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 90 minutes wall in total. Solves only in test 2 (two solves, 5 s cap each). The enumeration solves nothing and must run in well under 10 s Release. Run the measurement at most twice. No rerun loops.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>` (given at launch). Commit there; do not merge.
- Plan: `/home/pete/OpenTTD/docs/junction-tool/wip/CARD-domain-rules.md`, design point 2. Ruling D428. In place: `CheckPlanarDomain` (planar_domain.h), `PlanarDomainInputOf` (planar_domain_map.h).
- v1 of this step judged each geometry by solving the whole drawing program with the points fixed. That is a hub test in disguise: it stopped on port rows, last-piece rows and an integer infeasibility. The unit under test is the keep-apart rules. With the points fixed, each pair's rows are plain inequalities, so this step records the pairs as the program writes them and judges them on given points with no solve.
- v1's diagnosis on the X1 base geometry (record it in the report): with the section 5 options the program was integer-infeasible on `sepdir` rows (42 of 46 in face 1); with `strict_span`, `strict_zero` and `crossing_flanks` off it was Optimal. The domain accepts that geometry.

## 2. Read first (once each, with offset/limit)
- `CODINGSTYLE.md`, the C++ part.
- `src/junction/planar/planar_draw.h` lines 20-55 (`PlanarDrawResult`) and 72-135 (`PlanarDrawOptions`).
- `src/junction/planar/planar_draw.cpp` lines 214-260 (the result, `dumping`, the dump members) and 1183-1640 (the pairwise block: the `skip` lambda at 1208, `dump_kept` at 1350, `pi`/`pj` at 1353, `excluded` at 1355, the binary branch at 1606-1632), and 1740-1750 (the solve).
- `docs/junction-tool/wip/probes/domain/encoding-mip-harness.cpp.txt` (main checkout): v1's harness. Reuse its X1 loading, heading check, base and enumeration code, and report writer; drop its solve.
- `src/tests/junction/planar/planar_domain_map.cpp` (X1 texts).

## 3. Production changes (planar_draw.h / .cpp only)

In planar_draw.h, before `PlanarDrawResult`:
```cpp
/** One keep-apart pair of the pairwise block as written (D428 read-out): its skip reason, or its candidates with the rows each writes. */
struct PlanarKeepApartPair {
	/** One row of a candidate: s . (q - p) >= spacing, p and q given as (track, point index). */
	struct Row {
		std::array<uint32_t, 2> p;
		std::array<uint32_t, 2> q;
	};
	/** One side of one axis; the pair holds when every row of one candidate holds. */
	struct Candidate {
		std::array<int, 2> s;
		int spacing;
		std::vector<Row> rows;
	};
	std::string tag; ///< "sep <face> <i> <j>".
	std::array<uint32_t, 2> tracks; ///< Wall i's track, wall j's track.
	std::array<uint32_t, 2> pieces; ///< Wall i's piece, wall j's piece.
	std::string skipped; ///< Empty when rows were written, else the skip reason (the dump's wording).
	std::string kept; ///< When a strict rule kept a pair the plain rules skip: "strict span", or the strict_zero text the dump writes to `kept`; else empty.
	std::vector<Candidate> candidates;
};

/** Whether a pair holds on given points (as PlanarDrawResult::points): it is skipped, or every row of one of its candidates holds. */
bool PlanarKeepApartHolds(const PlanarKeepApartPair &pair, const std::vector<std::vector<std::array<int, 2>>> &points);
```
- `PlanarDrawResult`: add `std::vector<PlanarKeepApartPair> keep_apart; ///< With keep_apart_out: every pair the pairwise block considers, in order.`
- `PlanarDrawOptions`: add `bool keep_apart_out = false; ///< A read-out (D428): fill PlanarDrawResult::keep_apart; binary mode only (no sides_from, sides_by_walk or facing_sides; else unsupported).` and `bool build_only = false; ///< A measurement (D428): build the program and return before the solve, status "built".`
- In the pairwise block, when `keep_apart_out`:
  - At the top of the pair loop body push one `PlanarKeepApartPair` (tag, tracks and pieces from `w[i]`, `w[j]`; for a wall whose track is `>= nt` store the track as given).
  - The `skip` lambda sets its `skipped` to the reason (also when not dumping).
  - Where `strict_span` lets a span pair through (after the `sanctioned` check fails), set `kept = "strict span"`. Where `dump_kept` gets its text (line 1350), set `kept` to that same text (also when not dumping).
  - In the binary branch, per (axis, side) candidate actually written: one `Candidate{axis.s, sp(axis), rows}`, with one `Row` per row written (after `excluded`). `p` is (wall track, piece + e) for the endpoint `from[e]`, `q` the same for `to[e']`, where `from`/`to` are wall i's or wall j's two ends as the code picks them.
  - If `keep_apart_out` is set together with `sides_from`, `sides_by_walk` or `facing_sides`: `return unsupported("keep_apart_out needs the binary mode")` before the block.
- `build_only`: immediately before `milp.Solve()`, set `result.status = "built"` and return the result (rows and columns as already filled).
- `PlanarKeepApartHolds`: true when `skipped` is non-empty; else true when some candidate has every row with `s[0] * (q.x - p.x) + s[1] * (q.y - p.y) >= spacing` on `points`; else false.
- With both options off, nothing about the program, its rows or its result changes.

## 4. Tests (file `src/tests/junction/planar/planar_domain_encoding.cpp`)

### Test 1: "planar draw: keep-apart pairs hold on given points" (`[junction][planar][planardomain]`)
Two hand `PlanarKeepApartPair`s (no variant): a skipped one holds on any points; one with candidates {s (1,0), spacing 2, row p (0,0) q (1,0)} and {s (-1,0), spacing 2, row p (0,0) q (1,0)} holds on points where track 1's point 0 is 2 right of track 0's point 0, and fails at 1. Red: the stub returning false fails on the skipped pair.

### Test 2: "planar draw: keep-apart on fixed points agrees with the program on X1" (`[junction][planar][planardomain]`)
X1 with flex, base geometry (section 5). Options `o` = section 5's.
1. `CheckPlanarDrawable(v, 5, o + keep_apart_out + build_only)`: `REQUIRE` status "built" and `keep_apart` non-empty. Every pair holds on the base? `REQUIRE` at least one does not (with the strict rules on).
2. The same with `strict_span = strict_zero = crossing_flanks = false`: `REQUIRE` every pair holds.
3. The cross-check against the program: `CheckPlanarDrawable(v, 5, o + fix_points = base, fix_window 0, tidy false)`: `REQUIRE` Infeasible with the strict rules, Optimal without (v1's diagnosis). 
Red: before the read-out exists, part 1 fails on the empty `keep_apart`.

### Test 3: "planar domain: our keep-apart against the domain on X1" (`[.][planardomainenc]`)
The v1 harness, with the solve replaced: per configuration build the read-out once (`keep_apart_out`, `build_only`, section 5 options), then per geometry: grid filter (as v1), **rows accept** = every pair holds (`PlanarKeepApartHolds`), **domain accept** = `CheckPlanarDomain(PlanarDomainInputOf(...)).legal`. Classes: both accept, both reject, too strict, too loose, off grid.
Report (to the path in env `PLANAR_DOMAIN_REPORT`):
- per configuration the class counts;
- **too strict** groups keyed by the sorted set of `kept` values of the pairs that fail (empty `kept` written as `plain`), each with count and 3 examples: varied lengths, and per failing pair its tag, track names, pieces and each candidate's worst row slack (`s.(q-p) - spacing`, the least over its rows);
- **too loose** groups keyed by the sorted set of domain break rules, each with count and 3 examples: varied lengths, the breaks (rule, track names, tile, why), and every keep-apart pair between the tracks of each break with its `skipped` reason or "held";
- the base geometry's failing pairs (strict rules on) listed the same way, under a heading "base".

## 5. Configuration (as v1, with the fixes found there)
X1 texts and `PlacePlanarShapeFlex`; headings checked as in v1. Starts: AC1 (0,5), A1 and C1 (4,5), BD1 (11,12), B1 and D1 (11,10). STEP as v1.
Base lengths (unlisted pieces 0): AC1 piece 0 = 4; A1 piece 4 = 12, piece 8 = 4; B1 piece 4 = 6, piece 8 = 4; C1 piece 0 = 3, piece 9 = 2; BD1 piece 0 = 2; D1 piece 0 = 1, piece 9 = 8.
Enumerated, on top of the base:
- S1: B1 piece 2 in {0,2,4}, piece 3 in {0..4}, piece 4 in {2,4,...,14}, piece 5 in {0..4}, piece 6 = 4.
- S2: A1 piece 2 in {0,2,4}, piece 3 in {0..4}, piece 4 in {2,4,...,16}, piece 5 in {0..4}, piece 6 = 4.
- N1: C1 piece 0 in {0..3}, piece 1 in {0,2,4}, piece 2 in {0..3}; A1 piece 0 in {0,2,4}, piece 1 in {0..3}.
Options: `clearance = clear_rows = pairwise = pairwise_all_faces = true`, `strict_span = strict_zero = crossing_flanks = true`, `node_gaps = true`, `bays = curves = false`, `skip_rows = {"port "}`.

## 6. Do not
- Change nothing in the program when both new options are off; no new rows, no change to the dump.
- Do not record pairs from the `sides_from` or walk branches.
- No other production change; do not touch planar_domain*.
- Do not interpret or fix a disagreement.

## 7. Stop conditions
- Test 2 part 1 finds every pair holding with the strict rules on, or part 2 finds one failing without them: the read-out disagrees with the program; stop and report.
- Headings or names differ from v1's check.
- Anything not covered.

## 8. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5` and `tools/junction/jt fast build '[planar]' 10` (Debug; `[planar]` is already about 6 s from two existing tests, so report its time, and stop only if it rises by more than 1 s).
- `tools/junction/jt gate build-rel`.
- `PLANAR_DOMAIN_REPORT=/home/pete/OpenTTD/.claude/ng1/docs/junction-tool/wip/probes/domain/encoding-x1.txt timeout 300 build-rel/openttd_test "[planardomainenc]" > <scratch log> 2>&1`.
- `src/junction/planar/CLAUDE.md`, drawing program section, after the dumps bullet: `- Keep-apart read-out (D428, \`keep_apart_out\`, \`build_only\`): the pairwise block records every pair as written (skip reason, or candidates with their rows, and the strict rule that kept it); \`PlanarKeepApartHolds\` judges a pair on fixed points with no solve, and on X1 it agrees with the program.`
- One commit: `Add: Junction: keep-apart pairs judged on fixed points, and the encoding report on X1 (D428)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 9. Report (short)
References read; commit; red evidence per test; types and members added; per-configuration class counts and the enumeration's wall time; the top 3 groups of each disagreement class (key and count); suite results with times; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
