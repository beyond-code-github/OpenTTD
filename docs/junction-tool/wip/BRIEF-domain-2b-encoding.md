# BRIEF domain-2b: the encoding report on X1 (our rows against the domain)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes wall in total. Per solve a 5 s cap; the whole enumeration stops itself at 300 s wall (it should take well under 60 s in Release). Run the measurement at most twice (once to see it work, once for the committed report). No rerun loops.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>` (given at launch). Commit there; do not merge.
- Plan: `/home/pete/OpenTTD/docs/junction-tool/wip/CARD-domain-rules.md`, design point 2 and step 2. Ruling D428. In place: `CheckPlanarDomain` (planar_domain.h) and `PlanarDomainInputOf` (planar_domain_map.h, with its X1 test in `src/tests/junction/planar/planar_domain_map.cpp`).
- This step is a measurement harness (test code only, like `[planarnode]`): every geometry of three small configurations is judged by our rows and by the domain, and every disagreement is reported. Nothing is fixed here.

## 2. Read first (once each)
- `src/tests/junction/planar/planar_domain_map.cpp` (X1 texts and loading; reuse its texts verbatim).
- `src/junction/planar/planar_draw.h` lines 20-55 (`PlanarDrawResult`: `feasible`, `status`, `conflict`, `points`) and lines 72-135 (`PlanarDrawOptions`; the fields named in section 4).
- `src/junction/planar/planar_domain.h`, `src/junction/planar/planar_domain_map.h`.
- `src/junction/planar/planar_shape_layer.h` lines 155-165 (`PlacePlanarShapeFlex`).

## 3. The configuration
X1 as in planar_domain_map.cpp, loaded, then `PlacePlanarShapeFlex(v, why)` (default arguments). Map tracks by name. After flex the headings must be exactly (stop otherwise):
- AC1: 2 1 2 3 2
- A1: 2 1 2 3 2 1 2 3 2 (its crossing on piece 4)
- C1: 1 0 1 2 1 0 -1 0 1 0
- BD1: 0 -1 0 1 0
- B1: 0 -1 0 1 0 -1 0 1 0 (its crossing on piece 4)
- D1: 1 0 1 2 1 2 1 2 3 2

A geometry is a length per piece (half tiles along its heading); a track's points are its start, then the end of each piece: `p[k+1] = p[k] + len[k] * STEP(h[k] mod 8)` with STEP 0 (0,-1), 1 (1,-1), 2 (1,0), 3 (1,1), 4 (0,1), 5 (-1,1), 6 (-1,0), 7 (-1,-1). Starts: AC1 (0,5), A1 and C1 (4,5), BD1 (11,12), B1 and D1 (11,10).

Base lengths (the 2a hand drawing; a piece not listed is 0):
- AC1: piece 0 = 4. A1: piece 4 = 16. C1: piece 0 = 3, piece 5 = 2. BD1: piece 0 = 2. B1: piece 4 = 10. D1: piece 0 = 1, piece 5 = 8.

Enumerated (every combination; the listed pieces replace the base, the rest stay base):
- **S1, S-bend on the crosser at the crossing:** B1 piece 2 in {0,2,4}, piece 3 in {0..4}, piece 4 in {2,4,...,14}, piece 5 in {0..4}, piece 6 = 4.
- **S2, S-bend on the separator at the crossing:** A1 piece 2 in {0,2,4}, piece 3 in {0..4}, piece 4 in {2,4,...,16}, piece 5 in {0..4}, piece 6 = 4.
- **N1, legs at a node:** C1 piece 0 in {0..3}, piece 1 in {0,2,4}, piece 2 in {0..3}, piece 5 = 2; A1 piece 0 in {0,2,4}, piece 1 in {0..3}, piece 4 = 16.

## 4. Judging one geometry
- **Grid filter:** if `RasterPlanarDomainTrack` reports an error on any track, the geometry is counted as "off grid" and judged no further.
- **Rows:** `CheckPlanarDrawable(v, 5.0, o)` with `o.fix_points = &points`, `o.fix_window = 0`, `o.tidy = false`, `o.clearance = true`, `o.clear_rows = true`, `o.pairwise = true`, `o.pairwise_all_faces = true` (no `sides_from`: every keep-apart pair keeps its binary side choice, the exact encoding), `o.strict_span = o.strict_zero = o.crossing_flanks = true`, `o.node_gaps = true`, `o.bays = false`, `o.curves = false` (not tile rules; a later step), `o.skip_rows = {"port"}` (the enumerated ends leave their port edges). Accept = `feasible`; status other than Optimal/Infeasible (time limit, error) is counted as "rows undecided".
- **Domain:** `CheckPlanarDomain(PlanarDomainInputOf(v, points, notes))`. Accept = `legal`.
- Classes: both accept; both reject; **too strict** (rows reject, domain accepts); **too loose** (rows accept, domain rejects); off grid; rows undecided.

## 5. The test (file `src/tests/junction/planar/planar_domain_encoding.cpp`, one TEST_CASE "planar domain: our rows against the domain on X1", tags `[.][planardomainenc]`)
1. Load X1 with flex; check the headings (section 3).
2. The base geometry: `REQUIRE` the domain accepts it and the rows accept it. If the rows reject it, `FAIL` with the conflict tags (this is a stop condition, section 7).
3. Enumerate S1, S2, N1 in that order; stop the enumeration (and say so in the report) past 300 s wall.
4. Write the report to the path in env `PLANAR_DOMAIN_REPORT` (`REQUIRE` it is set): per configuration the six class counts; then for **too strict**, groups keyed by the sorted set of conflict tag families (each tag up to its first space), and for **too loose**, groups keyed by the sorted set of domain break rules; each group with its count and its first 3 examples (the varied lengths as `piece=len` per track, the full conflict tags or the breaks with `why`, and the notes). Then the same counts summed.
5. `WARN` the per-configuration counts.
No assertions on the class counts: disagreements are the finding.

## 6. Do not
- Change no production file. No new option, env hook (beyond reading `PLANAR_DOMAIN_REPORT`) or helper outside the test file.
- Do not turn on `elastic`, `grid`, `sides_from`, `sep_extremal`, `corridor` or curves; do not change a range in section 3.
- Do not interpret or fix a disagreement.

## 7. Stop conditions
- The headings after flex differ from section 3, or a track name is missing.
- The base geometry is rejected by the domain or by the rows: report the breaks or the conflict tags and stop.
- More than 5% of the on-grid geometries are "rows undecided": stop and report the count and one example.
- Anything not covered.

## 8. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5` (Debug; the hidden test is not in it).
- Build Release (`build-rel`), run `PLANAR_DOMAIN_REPORT=/home/pete/OpenTTD/.claude/ng1/docs/junction-tool/wip/probes/domain/encoding-x1.txt build-rel/openttd_test "[planardomainenc]" > <scratch log> 2>&1` with an outer `timeout 600`.
- `tools/junction/jt gate build-rel`.
- One commit with the test file, its CMakeLists line and the report: `Add: Junction: the encoding report, our rows against the domain on X1 (D428)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 9. Report (short)
References read; commit; the per-configuration class counts and the run's wall time; the top 3 groups of each disagreement class (key and count only); suite results; stops; `git status --porcelain` and `git log -1`. When you have reported, stop.
