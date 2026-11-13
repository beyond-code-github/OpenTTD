# G3.3b-1: HiGHS in the build behind one wrapper, JunctionMilp (D237, DESIGN-G3 §G3.3)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
The fork links HiGHS, and the junction code reaches it only through one small class, `JunctionMilp`: variables, rows, objective, solve, values, and the infeasible set. Nothing calls it yet, so no layout changes.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes`. First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-g33b1 <HEAD given at launch>`. Builds `build` (Debug) and `build-rel` exist.
- HiGHS 1.16.0 is installed, static, at `/home/pete/.local/highs`, with its CMake package at `/home/pete/.local/highs/lib/cmake/highs`. Reconfigure both builds with `-Dhighs_DIR=/home/pete/.local/highs/lib/cmake/highs`. Install nothing.
- Output files in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/g33b1/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- `CMakeLists.txt` 120-185 and 305-330;
- `vcpkg.json`;
- `src/junction/placement/CMakeLists.txt`, `src/junction/placement/junction_positions.h` (the style of a small solver class here);
- `docs/junction-tool/tools/g3proto/solve.py` 40-70 and 260-285 (how the prototype set HiGHS options and read the IIS);
- `/home/pete/.local/highs/include/highs/Highs.h`, only by grep: `passModel`, `addVar`, `addRow`, `changeColIntegrality`, `setOptionValue`, `run`, `getModelStatus`, `getSolution`, `getIis`;
- `src/tests/junction/placement/` (one existing test file, for its style) and `src/tests/CMakeLists.txt`;
- `src/junction/placement/CLAUDE.md`.

## Tests, red first (`src/tests/junction/placement/junction_milp.cpp`, tag `[junction][milp]`, registered in the tests CMakeLists)
Red: add the class with `Solve()` returning `Status::Error`. Run it, record the failure text, then write the body.
1. "milp: integer optimum differs from the relaxation". Minimise `x + y`, with x and y integer in [0, 10] and the row `x + 2y >= 3.5`. Expect Optimal, objective 2, x = 0, y = 2. Then the same model with both variables continuous: objective 1.75.
2. "milp: difference constraints solve to the least solution". Variables x0, x1, x2 integer in [0, 100]. Rows `x1 - x0 >= 2` and `x2 - x1 >= 3`. Minimise `x2`. Expect Optimal and x2 = 5.
3. "milp: an infeasible model names its conflicting rows". One variable x continuous in [0, 100], one variable y continuous in [0, 100]. Rows: row 0 `x >= 3`, row 1 `y >= 1`, row 2 `x <= 1`. Expect Infeasible and `InfeasibleRows()` == {0, 2} (sorted).

## Changes
1. **`vcpkg.json`:** add `{ "name": "highs" }` to `dependencies`, in alphabetical order. Leave the baseline as it is.
2. **`CMakeLists.txt`:**
   - after `find_package(Threads REQUIRED)`, add `find_package(highs CONFIG REQUIRED)`;
   - after `link_package(LZO)`, add `target_link_libraries(openttd_lib highs::highs)`.
   If the tests target does not get HiGHS through `openttd_lib`, STOP and report rather than linking it a second way.
3. **`src/junction/placement/junction_milp.h/.cpp`**, added to the placement CMakeLists. The header must not include any HiGHS header: the implementation holds a `std::unique_ptr` to a struct defined in the .cpp.
   ```cpp
   /** A mixed-integer linear program and its solve: the junction code's one seam to the solver (HiGHS). */
   class JunctionMilp {
   public:
   	static constexpr double INF = std::numeric_limits<double>::infinity();
   	enum class Status : uint8_t {
   		Optimal, ///< Solved to proven optimality.
   		Infeasible, ///< No solution exists; InfeasibleRows names a conflicting set.
   		Error, ///< Anything else (unbounded, solver error, limits).
   	};
   	JunctionMilp();
   	~JunctionMilp();
   	uint32_t AddVariable(double lower, double upper, double cost, bool integer); ///< Returns the variable's index.
   	uint32_t AddRow(std::span<const std::pair<uint32_t, double>> terms, double lower, double upper); ///< lower <= sum(coef * x) <= upper; returns the row's index.
   	Status Solve(); ///< Minimises the objective.
   	double Value(uint32_t variable) const; ///< After Optimal.
   	double Objective() const; ///< After Optimal.
   	std::vector<uint32_t> InfeasibleRows() const; ///< After Infeasible: an irreducible infeasible set of the LP relaxation, sorted.
   private:
   	struct Impl;
   	std::unique_ptr<Impl> impl;
   };
   ```
   - The variables and rows are kept in the Impl's own vectors. `Solve()` builds the HiGHS model from them each time it is called.
   - Options, set on every solve: `output_flag` false, `mip_rel_gap` 0, `random_seed` 0, `threads` 1. These make the solve quiet, exact and deterministic.
   - `INF` maps to HiGHS's infinity.
   - `InfeasibleRows()` takes the IIS of the LP relaxation, with `iis_strategy` set as the prototype set it (solve.py ~55), on a copy of the model with integrality removed.
   - A HiGHS call that returns an error status gives `Status::Error`. Never throw and never print.
4. **`src/junction/placement/CLAUDE.md`:** add "- Linear and integer programs go through `JunctionMilp`, the one seam to the solver (HiGHS); it solves quietly, exactly and deterministically (one thread, zero gap, fixed seed)."
5. **Root `src/junction/CLAUDE.md`, under Scope and folders:** add "- The fork links HiGHS (MIT), found by CMake's `find_package(highs)` and listed in `vcpkg.json`; only `JunctionMilp` includes it."

## Do not
- Call `JunctionMilp` from placement or anywhere else.
- Add warm starts, callbacks, time limits, or any member not listed.
- Vendor HiGHS sources into the repo, or change the HiGHS install.
- Touch `.claude/nodes2` or the main worktree's builds.
- Use `pkill`, a broad `kill`, or a bare `git stash`.

## Stop conditions
- `find_package(highs)` fails with the given `highs_DIR`, or linking needs extra libraries beyond `highs::highs`. Report the error text.
- Any existing test changes.
- Anything not covered.

## Gates
- 0 new warnings (HiGHS's own headers compile only in the .cpp; a warning from them is reported, not silenced);
- the three new tests, by name, in Debug;
- `tools/junction/jt fast build '[topology]' 5`;
- `tools/junction/jt gate build-rel`;
- `tools/junction/jt identity build-rel`;
- the `openttd` target builds in `build-rel`.

One commit on wt-g33b1: `Add: Junction MILP seam on HiGHS`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- red evidence;
- the three tests' results and times;
- the configure line used;
- gate and identity counts and times;
- the size of `openttd` before and after, in `build-rel`;
- stops;
- `git status --porcelain` and `git log -1`.
When you have reported, stop.
