# N1: incremental position propagation (D183, DESIGN-nodes §9, FABLE-nodes-perf §1.1)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Why
The node model places nodes by solving integer positions under constraints that are all min-closed and max-closed: differences (x_a - x_b >= k), unary domains (allowed positions as a bitset), and "avoid" (an edge [x_a, x_b] must not cover column c). Such a system has a least solution `lo` and a greatest `up`, found by monotone propagation with no branching; a backtracking structure search needs it incremental with undo. This step adds the solver alone, proven against brute force. Nothing in generation uses it yet (SolveJunctionSkeletonW stays).

## State
Worktree `/home/pete/OpenTTD/.claude/n1`, branch `wt-n1`, head given at launch (feature/junction-tool). Create `build` (Debug) and `build-rel` (Release) like the main ones (clang-18 with ccache); build `openttd_test` only. Tests only through `tools/junction/jt` with bracketed tags: `'[positions]'` (new; budget 1 s Release, 5 s Debug); `jt gate build-rel` once before the commit. Output files in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/n1/`. Another agent works in `.claude/n0a` on `junction_chain.*`: do not touch those files.

## Read first (once each, offset/limit)
`CODINGSTYLE.md`; `docs/junction-tool/FABLE-nodes-perf.md` §1.1; `src/junction_skeleton.h` and `.cpp` (style and doc conventions only; do not change them); `src/tests/CMakeLists.txt` and the `junction_skeleton` lines in `src/CMakeLists.txt` (~292).

## Tests, red first (new `src/tests/junction_positions.cpp`, tags `[junction][positions]`)
1. "positions: differences give the least and greatest solution": 3 vars in [0, 20]; AddDifference(1, 0, 3), AddDifference(2, 1, 4), RestrictRange(2, 0, 15). Expect lo = {0, 3, 7}, up = {8, 11, 15}. Red: the stub leaves bounds untouched; record the failing CHECK.
2. "positions: a negative cycle is infeasible": x1 - x0 >= 2 and x0 - x1 >= -1 -> the second Add returns false.
3. "positions: a domain bitset moves lo and up to allowed values": var in [0, 10], allowed {2, 5, 9}; x0 - x1 >= 1 with x1 lo 3 -> x0 lo 5 (not 4); up 9.
4. "positions: avoid": x0 (edge start), x1 (edge end), x1 - x0 >= 2, both in [0, 20], AddAvoid(0, 1, 1) (must not cover column 1): lo = {2, 4}; with x0 restricted to [0, 1] -> infeasible (false).
5. "positions: undo restores exactly": Mark, three Adds that move bounds, Undo(mark): every Lo/Up equals the values before; also after an Add that returned false.
6. "positions: agree with brute force": `std::minstd_rand` seeded 1; 300 systems; 2..4 vars, domain [0, 12]; per system 1..6 random constraints of the three kinds (difference k in -6..6 over distinct vars; domain = random subset with each value kept with probability 0.6; avoid with c in 0..12). Brute force enumerates all assignments. Assert: Add sequence returns all true iff brute force finds a solution; when feasible, Lo(v) == min over solutions and Up(v) == max over solutions for every v, AND the assignment x = lo satisfies every constraint, and so does x = up. Red: with the stub, fails.

## Changes
New `src/junction_positions.h` / `.cpp` (file header and include style as junction_skeleton.*), registered in `src/CMakeLists.txt` next to junction_skeleton; test file registered in `src/tests/CMakeLists.txt`.

```cpp
/**
 * Integer node positions under min- and max-closed constraints (DESIGN-nodes §9, FABLE-nodes-perf §1.1): differences, unary domains and "avoid".
 * Lo() is the least solution and Up() the greatest; both are kept by monotone propagation (FIFO, deterministic), and every change is recorded on a trail so Undo() restores an earlier state.
 * A call that makes some variable's lo exceed its up returns false; the state is then inconsistent until Undo() to a mark taken before the call.
 */
class JunctionPositions {
public:
	JunctionPositions(size_t vars, int min_value, int max_value); ///< Every variable starts with the domain [min_value, max_value].
	bool AddDifference(size_t a, size_t b, int k); ///< x_a - x_b >= k.
	bool RestrictRange(size_t v, int min_value, int max_value); ///< x_v in [min_value, max_value].
	bool RestrictDomain(size_t v, const std::vector<int> &allowed); ///< x_v in the given values (others removed from its bitset).
	bool AddAvoid(size_t a, size_t b, int c); ///< Not (x_a <= c <= x_b): an edge from x_a to x_b must not cover column c.
	size_t Mark() const; ///< A trail position for Undo().
	void Undo(size_t mark); ///< Restore lo, up and domains to the state at the mark; constraints added since are removed.
	int Lo(size_t v) const;
	int Up(size_t v) const;
	size_t Size() const;
};
```
Internals (private, named here so nothing else is invented): `std::vector<int> lo, up;` domain bitsets `std::vector<std::vector<uint64_t>> domain` over [min_value, max_value]; constraint lists `struct Difference { size_t a, b; int k; }; struct Avoid { size_t a, b; int c; };` with per-variable index lists of the constraints touching it; a trail of `struct TrailEntry { enum class Kind : uint8_t { Lo, Up, DomainWord, Difference, Avoid } kind; size_t index; int64_t old; };` (Difference/Avoid entries pop the constraint on undo); a FIFO `std::deque<size_t>` of variables whose lo or up changed.
Propagation rules (to a fixpoint, each a monotone step):
- difference a,b,k: lo[a] = max(lo[a], lo[b] + k); up[b] = min(up[b], up[a] - k).
- after lo[v] rises: lo[v] = next allowed value >= lo[v] in the bitset; after up[v] falls: up[v] = previous allowed value <= up[v]. None -> infeasible.
- avoid a,b,c (x_a <= x_b assumed by the caller's other constraints): if lo[a] <= c && c <= lo[b] then lo[a] = c + 1; if up[a] <= c && c <= up[b] then up[b] = c - 1.
- lo[v] > up[v] for any v -> return false.
Termination: every step moves a bound inside the finite domain.

## Do not
No change to the skeleton, generation, the router or other tests; no SolveJunctionSkeletonW replacement in this step. No `std::to_string`/`std::printf`; no `pkill`, broad `kill`, bare `git stash`.

## Stop conditions
Test 6 finds a disagreement after the change (report the smallest system); OVER BUDGET (if test 6 is over the Debug budget, reduce systems to 150 and report); anything not covered.

## Gates
0 warnings; `jt fast build '[positions]' 5`, `jt fast build-rel '[positions]' 1`, `jt gate build-rel`. Add a `[positions]` row to `docs/junction-tool/TESTS.md` with the measured times. One commit on `wt-n1`: `Feature: Junction incremental position propagation (D183)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
References read; commit; red evidence per test; types and members added; jt lines with times; stops; `git status --porcelain`, `git log -1`.

When you have reported, stop.
