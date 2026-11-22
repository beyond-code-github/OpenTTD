# BRIEF domain-5: the whole drawing program judged on fixed points

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 90 minutes wall. No solves in the harness. No rerun loops. Never run git stash.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- The X1 suite (`src/tests/junction/planar/planar_domain_encoding.cpp`, hidden `[planardomainenc]`) judges keep-apart and lead rows on fixed points. Its 57 too-loose polylines under `span+lead` are owned by other row families (cross, order, deck, end, span, piece), which it does not judge.
- With every point fixed, every other variable of the program is determined (piece lengths from the points, crossing parameters and crossing points from where the two pieces meet), except the keep-apart binaries and the box. So the whole program can be judged generically: record its rows, solve the determined variables from the equality rows, check every row whose variables are all known.

## 2. Read first
- `src/junction/planar/planar_draw.cpp` lines 214-340 (`var`, `add_variable`, `row`, `eq`, the export of variables and rows at 233-306 and 1740-1745), and 340-350 (`point`).
- `src/junction/planar/planar_draw.h` (`PlanarDrawResult`, `PlanarDrawOptions`, the read-out types near the top).
- The harness file, whole.

## 3. Production changes (planar_draw.h / .cpp)
```cpp
/** The drawing program as written (D428 read-out): its variables and rows, and which variables are the track points. */
struct PlanarProgram {
	struct Row {
		std::vector<std::pair<uint32_t, double>> terms;
		double lower;
		double upper;
		std::string tag;
	};
	std::vector<double> lower; ///< Per variable.
	std::vector<double> upper;
	std::vector<bool> integer;
	std::vector<std::string> group; ///< Per variable, its `var` group ("point", "param", "cross", "other", ...); lengths as their group.
	std::vector<Row> rows;
	std::vector<std::vector<std::array<uint32_t, 2>>> point; ///< Per track and point, its x and y variable.
};

/** What a program says about fixed points. */
struct PlanarProgramCheck {
	std::vector<std::string> broken; ///< Tags of rows that fail (each once, in row order), and "integer <group>" for a determined integer variable off an integer.
	std::vector<std::string> undetermined; ///< Families (tag up to the first space) of rows skipped because a variable stayed unknown, each once.
};

/** Fix the points, solve the variables the equality rows determine, and check every row whose variables are all known (tolerance 1e-6). */
PlanarProgramCheck CheckPlanarProgramOnPoints(const PlanarProgram &program, const std::vector<std::vector<std::array<int, 2>>> &points);
```
- `PlanarDrawOptions::program_out` (bool, default false, a read-out): fill `PlanarDrawResult::program` (new member) with every variable (bounds, integer flag, group) and every row as `row` writes it (after its own merging, before any solve), and `point`. Works with `build_only`. With it off nothing changes.
- `CheckPlanarProgramOnPoints`:
  1. Known: the point variables at the given values.
  2. Repeat until nothing changes: (a) every equality row (lower == upper) with exactly one unknown variable determines it; (b) group the equality rows whose unknowns number exactly 2 by that pair; a pair with two rows whose 2x2 system is non-singular (|det| > 1e-9) determines both. Then the rows' consistency is checked in step 3 like any row.
  3. Every row whose variables are all known: broken when its value is below `lower - 1e-6` or above `upper + 1e-6`. Every determined variable outside its bounds: broken as `bound <group>`. Every determined integer variable more than 1e-6 off an integer: broken as `integer <group>`.
  4. Rows with an unknown variable: their family goes to `undetermined`.

## 4. Tests (fast, `[junction][planar][planardomain]`, in the harness file)
- "planar draw: the program on fixed points agrees with the program on X1":
  - X1 base (domain-2b geometry, its section 5 options with the three strict rules OFF, no node_lead): `build_only` + `program_out`; `CheckPlanarProgramOnPoints` on the base: `broken` empty (the program with these points fixed was Optimal, domain-2b test 2). `undetermined` lists `sep`/`sepdir` (binaries) and nothing that holds track geometry; list it in the report.
  - The v1 base (A1 piece 4 = 16, B1 piece 4 = 10, C1 piece 5 = 2, D1 piece 5 = 8, every last piece 0): `broken` contains `piece` rows of C1, B1, A1 and D1's last pieces (v1's conflict tags `piece C1 9`, `piece B1 8`, `piece A1 8`, `piece D1 9`).
  - Red: a stub returning nothing fails the second; record.

## 5. Harness change
- Build the program read-out once per configuration (same options as the keep-apart read-out, plus `program_out`).
- In the polyline search, a full assignment is accepted only when, in addition to every keep-apart pair and lead row, `CheckPlanarProgramOnPoints` gives `broken` empty. Count each full-assignment check as one search node. Keep the caps.
- Too strict key: the sorted set of the failing rows' families on the enumerated representation (`kept` values for keep-apart pairs as now, `lead`, and each `broken` tag's family). Too loose as now, plus the witness's `undetermined` families.
- Report, once per run, the `undetermined` families seen.

## 6. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5`; `tools/junction/jt gate build-rel` (one rerun if only the LL² joint case fails).
- Release: `plain+lead` and `span+lead` (overwrite), timeout 120.
- `src/junction/planar/CLAUDE.md`, drawing program section, after the keep-apart read-out bullet: `- Program read-out (D428, \`program_out\`): the program's variables and rows as written; \`CheckPlanarProgramOnPoints\` fixes the points, solves the variables the equality rows determine (one unknown, or two in a pair of rows) and checks every row it can, listing the families it could not.`
- One commit: `Add: Junction: the whole drawing program judged on fixed points (D428)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 7. Stop conditions
- The base check breaks a row: report the tags; do not change the program.
- `undetermined` contains a family other than `sep`, `sepdir`, `box`, `lane`, `land`, `clear`, `room`, `along`: report it (a variable the points should determine stayed unknown).
- A harness run over 120 s.
- Anything not covered.

## 7. Report (short)
Commit; red evidence; the base check's `undetermined`; the two sets' lines in polylines (both accept, both reject, too strict, too loose, undecided) with group keys and counts; for each new too-strict group (a row family) its first example; suites with times; state. When you have reported, stop.
