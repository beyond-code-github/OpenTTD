# BRIEF domain-3: no S-bend before a node leg's separation point (rule A, probe)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall in total. No solves except where an existing test already solves. The enumeration runs in well under 1 s Release. Run the measurement once per rule set (four runs). No rerun loops.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>` (given at launch). Commit there; do not merge.
- Ruling: the engineer chose option A (2026-11-23, "Yep lets try A then"): a node's legs get no S-bend before their separation point. D400 proves legs separate one game piece past the node tile. Today a flex pair's leading flank may be 0, so a leg's S-bend can start on the node and run along its sibling leg. The X1 encoding report (`docs/junction-tool/wip/probes/domain/encoding-x1-span.txt`, main checkout) shows it: S1 B1 against D1, N1 C1 against A1, every pair skipped by zero contraction.
- Corpus: all 36 of V's node-side flanks (PSG 197) are at least one game piece (half tiles: axis >= 2, diagonal >= 1). The rule keeps V.
- This step adds the rule behind an option, as data that both the program and the suite read, and measures the suite with it.

## 2. Read first (once each, with offset/limit)
- `src/junction/planar/planar_draw.h`: `PlanarKeepApartPair`, `PlanarKeepApartHolds`, `PlanarDrawOptions` (72-140).
- `src/junction/planar/planar_draw.cpp`: around line 386 (D412, the port line that may have total length 0) and where the `piece` rows are written (search `"piece "`), to see how a point's variables are named (`point[t][k]`) and how rows are added (`row(...)`).
- `src/tests/junction/planar/planar_domain_encoding.cpp` (whole): the harness and `PLANAR_DOMAIN_RULES`.

## 3. Production changes (planar_draw.h / .cpp only)

In planar_draw.h, after `PlanarKeepApartHolds`:
```cpp
/** A row on fixed points: s . (point q - point p) >= least, p and q as (track, point index). */
struct PlanarPointRow {
	std::string tag;
	std::array<uint32_t, 2> p;
	std::array<uint32_t, 2> q;
	std::array<int, 2> s;
	int least;
};

/** Whether a point row holds on given points (as PlanarDrawResult::points). */
bool PlanarPointRowHolds(const PlanarPointRow &row, const std::vector<std::vector<std::array<int, 2>>> &points);

/**
 * The node lead rows (rule A, D400): no S-bend before a node leg's separation point. Per track end at a node (not a port): when its nearest flex pair starts at the end (the track's first flex middle is piece 1 at its start; its last flex middle is the second-last piece at its end), the end piece travels at least one game piece: s = STEP[end piece heading mod 8], least 2, between the end piece's two points (start: p = (t, 0), q = (t, 1); end: p = (t, n - 1), q = (t, n), n the piece count). Tag `lead <track name> start|end`. A track the program lets have total length 0 (D412) gets none.
 */
std::vector<PlanarPointRow> PlanarNodeLeadRows(const PlanarShapeVariant &v);
```
- `PlanarDrawOptions`: `bool node_lead = false; ///< Rule A probe (D400, engineer 2026-11-23): write PlanarNodeLeadRows as hard rows (tag lead).`
- In `CheckPlanarDrawable`, when `node_lead`: for each row of `PlanarNodeLeadRows(v)`, one row `s . (point[q] - point[p]) >= least` over the point variables, tag as given. Write them where the piece rows are written. Elastic mode treats them like any other non-game-rule row (they get slack).
- `PlanarPointRowHolds`: `s[0] * (q.x - p.x) + s[1] * (q.y - p.y) >= least`.
- With `node_lead` off, nothing changes.
- Note for the check: STEP of a diagonal heading is (±1, ±1), so a diagonal end piece of d half tiles gives 2d; least 2 means d >= 1. An axis piece of length L gives L; least 2 means one tile.

## 4. Tests
### Test 1 (in `planar_domain_encoding.cpp`): "planar draw: node lead rows on X1" (`[junction][planar][planardomain]`)
X1 with flex (the fixture). `PlanarNodeLeadRows(f.v)`: `REQUIRE` exactly the rows for the starts of A1, C1, B1 and D1 (AC1 and BD1 are port lines with no event before their node, so D412 lets them have total length 0 and they get none), each with least 2 and s = STEP of that end piece's heading. On the base geometry every row holds. On S1's leak geometry (B1 pieces 2=0, 3=2, 4=2, 5=0, 6=4 over the base) B1's start row fails. Red: the stub returning no rows fails on the count.

### Test 2: the harness
- `PLANAR_DOMAIN_RULES` takes an optional `+lead` suffix (`plain+lead`, `span+lead`, `zero+lead`, `all+lead`): rows accept then also needs every `PlanarNodeLeadRows` row to hold. In too strict, a failing lead row adds the key `lead`. In too loose, list the lead rows between the break's tracks too.
- Nothing else in the harness changes.

## 5. Do not
- No change to Shape, flex placement, the pipeline or its defaults. No other new row family.
- Do not change a keep-apart skip rule or a strict rule.
- Do not interpret or fix a disagreement.

## 6. Stop conditions
- Test 1's expected row set does not match (a node end missing, or an extra one): report the rows the function gives.
- A run of the harness errors or is slower than 5 s.
- Anything not covered.

## 7. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5` (Debug).
- `tools/junction/jt gate build-rel`.
- Release: for each of `plain+lead`, `span+lead`, `zero+lead`, `all+lead`: `PLANAR_DOMAIN_RULES=<set> PLANAR_DOMAIN_REPORT=/home/pete/OpenTTD/.claude/ng1/docs/junction-tool/wip/probes/domain/encoding-x1-<set with + as _>.txt timeout 120 build-rel/openttd_test "[planardomainenc]" > <scratch log> 2>&1`.
- `src/junction/planar/CLAUDE.md`, drawing program section, after the keep-apart read-out bullet: `- Node lead (\`node_lead\`, a probe, off by default; rule A, D400): a track end at a node whose nearest flex pair starts at that end travels one game piece before it (\`PlanarNodeLeadRows\`, tag \`lead\`), so no S-bend starts on a node; V's PSG 197 keeps it (36 node-side flanks, all at least one piece).`
- One commit: `Add: Junction: node lead rows, no S-bend before a leg's separation point (rule A probe, D400, D428)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report (short)
References read; commit; red evidence; types and members added; per rule set the total class counts (both accept, both reject, too strict, too loose, off grid) and the too-loose and too-strict group keys with counts; suite results with times; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
