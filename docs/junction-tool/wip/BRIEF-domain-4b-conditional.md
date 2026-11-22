# BRIEF domain-4b: the toward-sibling lead row only when its insert is used

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 45 minutes wall. No rerun loops. Never run git stash.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- domain-4 (39f15c79ac) binds the lead row to the first insert toward the sibling, but a hard row cannot depend on whether that insert is used, so a polyline whose toward insert is 0 still needs the travel (18 legal polylines rejected). The rule must read: **if the toward insert has length, the travel from the node to it along the end heading is positive.** With integer points that is linear: `C * travel - insert >= 0`, C = 64.
- This needs a point row over more than two points. The general form also serves the other row families later.

## 2. Changes (planar_draw.h / .cpp)
- Replace `PlanarPointRow`'s `p`, `q`, `s` by terms:
  ```cpp
  /** A row on fixed points: sum over terms of c . point(track, index) >= least. */
  struct PlanarPointRow {
  	struct Term {
  		std::array<uint32_t, 2> point; ///< (track, point index).
  		std::array<int, 2> c;
  	};
  	std::string tag;
  	std::vector<Term> terms;
  	int least;
  };
  ```
  `PlanarPointRowHolds` sums `c[0] * x + c[1] * y` over the terms. The program writes one row from the terms over the point variables, as it does now.
- `PlanarNodeLeadRows`: keep domain-4's choice of ends (legs of splits at their start, legs of merges at their end; trunks none; D412 in-port exemption) and of the sibling side. Change which rows:
  - **One row per toward insert** in the leading walk (not only the first): for a start row with insert m, h0 = pieces[0], h = pieces[m]: travel T = STEP[h0] . (point(t, m) - point(t, 0)); insert length I = STEP[h] . (point(t, m + 1) - point(t, m)) (a diagonal STEP has dot 2 per unit: use it as is). Row: `64 * T - I >= 0`, terms: point(t, m) with 64 * STEP[h0], point(t, 0) with -64 * STEP[h0], point(t, m + 1) with -STEP[h], point(t, m) with +STEP[h] (merge equal points into one term or keep both: either is fine). Tag `lead <track> start <m>`.
  - End rows mirrored: insert m walked toward the node; T = STEP[h_end] . (point(t, n) - point(t, m + 1)); I = STEP[pieces[m]] . (point(t, m + 1) - point(t, m)) negated if needed so I >= 0 for a used insert (state the sign you use in a comment and test it); tag `lead <track> end <m>`.
- Doc comment: the rule in these words, the 64, and that a diagonal end heading gives T >= 1 per half-tile unit (so the guarantee there is half a piece; the suite measures whether that matters).
- Update the harness only where it reads `p`, `q`, `s` (it should only call `PlanarPointRowHolds`).

## 3. Tests
- "planar draw: node lead rows on X1": the rows now (per track, per toward insert: list them from the sites, as domain-4 did); the domain-3 same-polyline geometry holds; the 18-polyline example with A1's run kept at piece 4 (pieces 0-2 = 0, piece 3 = 2) holds (the toward insert m = 1 is 0); a geometry with A1 piece 0 = 0 and piece 1 = 1 (a used toward insert on the node) fails `lead A1 start 1`. Red: domain-4's body fails the last two; record.

## 4. Gates and output
- `tools/junction/jt fast build '[planardomain]' 5`; `tools/junction/jt gate build-rel` (one rerun if only the LL² joint case fails).
- Release: `plain+lead` and `span+lead` (overwrite), timeout 120.
- CLAUDE.md node lead bullet: "if a leg's S-bend toward its sibling at a split (or merge) has length, the leg travels from the node before it (`64 T - I >= 0`); outward S-bends and trunks are free".
- One commit: `Change: Junction: node lead rows only bind a used S-bend toward the sibling (D430, D428)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 5. Report (short)
Commit; red evidence; the rows on X1; the two sets' lines (polylines: both accept, both reject, too strict, too loose, undecided) and their group keys with counts, and for any `lead` group the first example; suites; state. When you have reported, stop.
