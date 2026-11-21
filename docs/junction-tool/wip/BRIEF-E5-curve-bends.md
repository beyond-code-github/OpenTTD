# BRIEF E5-curve-bends: corridor steps and committed flex inserts as curve bends (Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head: given in the launch message. Release build `build-rel`, `-j 4` only.
- Source: DECISIONS D421 (its amendment: corridor opening and closing steps are real bends in the curve rows, their fixed lengths count as travel) and wip/gemini/2026-11-23-optional-bend-curves-reply-digest.md (committed flex inserts: unused held at 0, used ones are real bends with curve rows). Both add bends to the existing curve walk (`options.curves` block in `CheckPlanarDrawable`).

## 2. Read first (once each, offset/limit)
- `src/junction/planar/planar_draw.cpp`: the curves block and `once_row`; the corridor block (`options.corridor`, `PlanarBesideOpening`, `PlanarBesideClosing`, the `Beside` struct); the flex groups and `flex_zero_from` handling (lines about 300-345).
- `src/junction/planar/planar_draw.h`: `PlanarDrawOptions` (`curves`, `corridor`, `flex_zero_from`, `flex_zero_below`).
- `src/tests/junction/planar/planar_draw.cpp`: `DrawnCurveGaps`, the curves tests, the corridor tests.

## 3. Tests (outermost first; red before green), tag `[junction][planar][planardraw]`
1. **"planar draw: a prio row's corridor steps keep curve spacing"**: `ShapeFirst(PlanarTText(1), 1)`, `{.clearance = false, .curves = true, .corridor = true}`, REQUIRE feasible. Extend `DrawnCurveGaps` with a `bool corridor` parameter: on a route path entering a beside track b (`part_of[b] == UINT32_MAX`) at its from-node, add a bend-generating virtual piece of heading (h0 + s) mod 8 and length 2 before b's pieces, and one of heading (hn - s) mod 8 and length 2 after them (s as in the corridor rows). CHECK every gap >= its least. Red: before 4 b the CHECK fails (premise: if it already passes, STOP and report the gaps).
2. **"planar draw: a committed flex insert keeps curve spacing"**: the same T, solve once `{.clearance = false, .curves = true}` and take `r.lengths`; build a reference copy where every flex middle is 0 except the first flex middle found (any track): set it to 2 (half tiles). Solve `{.clearance = false, .curves = true, .curve_flex_from = &reference}`. CHECK that the committed insert's length is >= 2 and every uncommitted insert's length is 0, and that every gap from `DrawnCurveGaps` (extended with `const std::vector<std::vector<double>> *committed`: a flex middle whose reference length is >= `curve_flex_above` is a real piece for bends; a middle below it counts as length only) is >= its least. Red: before 4 c the length or gap CHECK fails (premise: if it already passes, STOP).

## 4. Changes
a. `PlanarDrawOptions`, after `corridor`: `const std::vector<std::vector<double>> *curve_flex_from = nullptr; ///< Flex commitment (D422 probe; Gemini digest 2026-11-23): per flex middle, its length in this reference (half tiles, as PlanarDrawResult::lengths); at or above curve_flex_above the insert is committed (length >= its least, a real bend pair in the curve rows), below it the insert is held at 0.` and `double curve_flex_above = 0.25; ///< With curve_flex_from.`
b. Curves block, corridor steps (when `options.corridor && options.curves`): a bend position is `Terms` plus a `double` constant. Extend the local `Bend` struct with `double constant = 0`; the running travel keeps a constant too. On entering a beside track b of the corridor rows at its from-node (the same b, chain and s the corridor block uses), before its first piece: process a virtual piece of heading (h0 + s) mod 8 and constant length 2 (a bend where the heading changes, then constant += 2); after its last piece, a virtual piece of heading (hn - s) mod 8, constant length 2. Rows: lower bound = least - (constant_j - constant_i).
c. Curves block, committed inserts (when `options.curve_flex_from`): a flex middle with reference length >= `curve_flex_above` is walked as a real piece (bends at its heading changes); every other flex middle stays as today (length only). Tag a row with a committed insert's bend as either end `fmt::format("curve flex {} {}", map.tracks[t].name, piece)` (the first committed insert involved), others keep `curve <route>`. Bounds: in the lengths block, a committed middle gets lower bound its least (axis: 1 in variable units, i.e. 2 half tiles), an uncommitted one upper bound 0 (as `flex_zero_from` does).
d. Comments: Doxygen on the new fields; the file comment's curve line mentions corridor steps and committed flex.

## 5. Do not
- No default changes (`corridor` stays false; `curve_flex_from` null).
- No ratchet or rounds in the library: the rounds are a probe the orchestrator runs.
- No change to keep-apart, bays, the corridor rows themselves, or Shape.

## 6. Stop conditions
- A test premise fails (the CHECK already passes before the change).
- The T has no flex middle (test 2).
- Anything outside `planar_draw.h`, `planar_draw.cpp`, `src/tests/junction/planar/planar_draw.cpp` seems needed.

## 7. Gates
- `tools/junction/jt fast build-rel '[planardraw]' 5`; `tools/junction/jt gate build-rel` (30 s). Known flake: "planar components: T2 mapped lands in its box at the anchor"; rerun alone and report.
- One commit: `Add: Junction: corridor steps and committed flex inserts in the curve rows`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 8. Report (short, fixed format)
- References read
- Commits: hash and subject
- Red evidence: each new test and its failure before the change
- Types and members added or removed: names only
- Suite results, naming any failure you did not cause
- Stops and anything left undone, with the reason
- State: `git status --porcelain` and `git log -1 --oneline`

When you have reported, stop.
