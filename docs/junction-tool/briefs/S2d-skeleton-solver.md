# S2d: skeleton solver seam (difference constraints)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree `/home/pete/OpenTTD/.claude/s2d`, branch `wt-s2d`, head given in your launch prompt. You are the only writer there. Other agents work in sibling worktrees under `.claude/`; never touch them or the main worktree `/home/pete/OpenTTD`.
- Build: configure once, `cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > cfg.log 2>&1`, then `cmake --build build-rel --target openttd_test -j 8 > build.log 2>&1` (ccache is shared, so it is fast after the first build). Debug only if the gate asks.
- Tests: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/ptest.sh build-rel/openttd_test "<filter>" 8` (8 jobs: other agents share the machine). Iterate with your own tag; full `"~[.]"` only at the gate.
- Decisions: `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md` (read the rows named below; it is newer in the main worktree than in yours).
- Plan: `docs/junction-tool/PLAN-S2.md` §3 (the model) and §4 S2d. Decisions D144 (solver semantics), D113, D108, D110, D118. Background only: `docs/junction-tool/RECON-solver-vars.md` §6.

## Why (settled)
S2 replaces the generator's parameter sweep with a solve of difference constraints (x_a − x_b ≥ k) over the W-side positions. This step adds the solver and the W-side constraint builder as a pure seam: new files, no map, no generator change. Offsets are inputs with placeholder defaults; S2c pins the real values later.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`.
- `docs/junction-tool/PLAN-S2.md` §3.
- `src/junction_throughput.h` (as a style model for a small pure header) and `src/tests/CMakeLists.txt`, `src/CMakeLists.txt` (how junction files are registered).

## Interface (new files `src/junction_skeleton.{h,cpp}`, `src/tests/junction_skeleton.cpp`)
```cpp
/** Why a skeleton constraint exists (PLAN-S2 §3). */
enum class JunctionSkeletonRule : uint8_t { AdjacentSplits, BridgeEntry, Span, EqualLength, HeadClear, BridgeExit, PrioCoverage, NorthWestCap, ShiftBack, Extra };

/** x[a] - x[b] >= k. Index 0 is the origin, fixed at 0, for bounds. */
struct JunctionSkeletonConstraint { uint8_t a; uint8_t b; int k; JunctionSkeletonRule rule; };

/** Which extreme a variable is fixed at (D144). */
enum class JunctionSkeletonPush : uint8_t { Down, Up };

struct JunctionSkeletonSolution {
	std::vector<int> x; ///< Per variable, index 0 the origin (0); empty when infeasible or unbounded.
	std::vector<JunctionSkeletonRule> conflict; ///< The rules of the constraints on a negative cycle, when infeasible.
	int unbounded = -1; ///< The first variable with no bound in its push direction, or -1.
};

/**
 * Solve difference constraints (D144): variables 1..n are fixed one at a time in 'order', each at its extreme
 * (smallest for Down, largest for Up) consistent with every constraint and the earlier fixes.
 */
JunctionSkeletonSolution SolveJunctionSkeleton(size_t variables, const std::vector<std::pair<uint8_t, JunctionSkeletonPush>> &order, const std::vector<JunctionSkeletonConstraint> &constraints);
```
Method: Bellman–Ford on the constraint graph (edge b→a weight −k for x_a − x_b ≥ k, i.e. shortest paths give upper bounds; run it from the origin in both directions to get each variable's current [lower, upper]). For each variable in `order`: compute its bounds given the constraints plus equality constraints for the variables fixed so far; fix it at lower (Down) or upper (Up); a missing bound sets `unbounded`. A negative cycle at any point → infeasible; report the rules of the cycle's constraints (each once, in cycle order). About 10 variables: no optimisation needed.

W-side builder, same files:
```cpp
/** Inputs of the W-side doubled section (PLAN-S2 §3). x runs along the W row; travel is -x, so upstream is larger x. */
struct JunctionSkeletonInputs {
	int zmin[2], zmax[2]; ///< Crossing set on lane A's row [0] and lane B's row [1].
	int nmax = 10;        ///< Largest bridge middle length.
	int cover = 0;        ///< Prio coverage in tiles (spec.prio_tiles).
	int x_cap = 0;        ///< Largest N->W join x (xI - 2).
	bool shift_back = true; ///< Parallelogram style: has a shift-back x_sb.
	/* Rule offsets; placeholders until S2c pins them. */
	int k_split = 2;
	int k_entry[2] = {4, 4};
	int k_exit[2] = {4, 4};
	int k_head = 1;
	int k_w = 1;
	int k_sb = 1;
	int x_sb_min = 2;
	std::vector<JunctionSkeletonConstraint> extra; ///< Learned constraints (S3), added as given.
};

struct JunctionSkeletonW {
	int x_sl, x_ls, x_he[2], x_hw[2], x_lm, x_nw, x_sb; ///< x_sb is 0 when !shift_back.
	std::vector<JunctionSkeletonRule> conflict; ///< Non-empty when infeasible.
};

JunctionSkeletonW SolveJunctionSkeletonW(const JunctionSkeletonInputs &in);
```
Variables (indices fixed as a file-local enum): origin 0, x_heA, x_heB, x_hwA, x_hwB, x_ls, x_sl, x_lm, x_nw, x_sb.
Constraints (L in {A, B}; rule tag in brackets):
- x_sl − x_ls ≥ k_split [AdjacentSplits]
- x_ls − x_heL ≥ k_entry[L] [BridgeEntry]
- x_heL − x_hwL ≥ 2 and x_hwL − x_heL ≥ −(nmax + 1) [Span]
- Equal lane lengths with the D118 stagger [EqualLength]: x_heA − x_heB ≥ s, x_heB − x_heA ≥ −s, x_hwA − x_hwB ≥ s, x_hwB − x_hwA ≥ −s, where s = zmax[0] − zmax[1] when that equals zmin[0] − zmin[1], else 0. Both heads of a lane shift together by the crossing set's offset between the rows, so the spans are equal (D118). Document this in the builder's comment.
- zmin[L] − x_hwL ≥ k_head, x_heL − zmax[L] ≥ k_head [HeadClear] (as constraints against the origin)
- x_hwL − x_lm ≥ k_exit[L] [BridgeExit]
- x_lm − x_nw ≥ cover + k_w [PrioCoverage]
- x_cap − x_nw ≥ 0 [NorthWestCap]
- shift_back: x_nw − x_sb ≥ k_sb, x_sb ≥ x_sb_min [ShiftBack]; else x_sb is fixed at 0 by x_sb − 0 ≥ 0 and 0 − x_sb ≥ 0.
- `extra` as given.
Order: x_heA Down, x_heB Down, x_hwA Up, x_hwB Up, x_ls Down, x_sl Down, x_lm Up, x_nw Up, x_sb Up.

## Tests (red first, `[junction][skeleton]`; the outer test first)
1. "SolveJunctionSkeletonW: staggered crossing gives staggered heads" (outer): zmin {40, 41}, zmax {42, 43}, nmax 10, cover 15, x_cap 28, defaults otherwise → x_he {43, 44}, x_hw {39, 40}, x_ls 48, x_sl 50, x_lm 35, x_nw 19, x_sb 18, conflict empty. Red: builder stub returning zeros.
2. "SolveJunctionSkeletonW: a span over the limit names the span": zmin {40, 40}, zmax {52, 52}, rest as test 1 → conflict contains `Span` (and is non-empty).
3. "SolveJunctionSkeleton: Down and Up pick the extremes": x1 − 0 ≥ 3, 0 − x1 ≥ −9, x2 − x1 ≥ 2, 0 − x2 ≥ −20; order (x1 Down, x2 Up) → x1 = 3, x2 = 20; order (x2 Down, x1 Up) → x2 = 5, x1 = 3.
4. "SolveJunctionSkeleton: negative cycle reports its rules": x1 − x2 ≥ 1 [Span], x2 − x1 ≥ 1 [HeadClear] → x empty, conflict {Span, HeadClear} in either cycle order.
5. "SolveJunctionSkeleton: unbounded variable": x1 − 0 ≥ 3 only, order (x1 Up) → unbounded = 1.
Record each red.

## Do not
- Touch the generator, router, checker or any existing file other than the two CMakeLists.
- Add C9 (curve spacing) or E-side constraints: later steps.

## Stop conditions
- A test's expected values contradict the stated constraints (report the arithmetic).
- The EqualLength rule as written cannot express a case in tests 1-2.

## Gates
1. Build `build-rel` `openttd_test`: no errors, no new warnings.
2. `ptest.sh build-rel/openttd_test "[junction][skeleton]" 8`, then `ptest.sh build-rel/openttd_test "~[.]" 8`.
3. Commit: `Add: Junction skeleton solver for the W-side doubled section`, body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Commit only your files.

## Report
- References read
- Commit: hash and subject
- Red evidence per test (the failure message seen before the code existed)
- Types, functions, members, constants added or removed: names only
- Suite results and wall times
- Stops, and anything left undone
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
