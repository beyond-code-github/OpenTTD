# Brief E5-FS-h: the drawing program pins points to an answer key; D412 port lanes

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `{HEAD}`. Work and commit only there.
- Plan: D393 (answer key), D409 (elastic), D412 (the last rows of `/home/pete/OpenTTD/docs/junction-tool/DECISIONS.md`, main worktree). The expert's PSG 197 is drawn by the extractor; a later step writes its point coordinates in the format below. This step makes the drawing program read and hold them, and adds D412.

## 2. The pins format (fixed; the extractor will write it)
```
# comment lines start with '#'
# Half tiles, map frame (x east, y south): tile (x, y) spans [2x, 2x+2] x [2y, 2y+2].
end <line> start <X> <Y>      # the line's first point
end <line> end <X> <Y>        # the line's last point
cross <lineA> <lineB> <X> <Y> # the crossing point of two lines (either order)
```
- `<line>` is a track name of the variant's map (`map.tracks[t].name`); for the extractor's topology these are `L1`, `L2`, ...

## 3. Read first (once each)
- `CODINGSTYLE.md` (C++ part); D393, D409, D412.
- `src/junction/planar/planar_draw.h` (whole); `planar_draw.cpp`: `var`, `row`/`eq`, the elastic slack code, the points (`point[t]`), the piece lengths and flex groups, the port rows, the crossings (`cross_point[c]`, `map.crossings[c].a/.b`), and the solve/result section.
- `src/tests/junction/planar/planar_draw.cpp`: `ShapeFirst`, and the X8 test "the extractor's PSG 197 loads (X8)" (how the extractor's files are read and shaped).

## 4. Changes
1. `planar_draw.h`:
   ```cpp
   /** Points of an answer key (D393): positions the drawing must keep within `window` half tiles, up to one common shift. */
   struct PlanarDrawPins {
   	struct Pin {
   		uint32_t track; ///< The track (end pins) or the crossing's index in map.crossings (cross pins).
   		bool cross; ///< A crossing pin.
   		bool start; ///< For an end pin: the track's first point, else its last.
   		std::array<int, 2> at; ///< Half tiles, map frame.
   	};
   	std::vector<Pin> pins;
   	int window = 2; ///< Half tiles, per axis.
   	std::vector<std::string> unmatched; ///< Lines of the text that named no track or crossing.
   };
   /** Read pins (format in docs/junction-tool/briefs/E5-FSh-pins.md section 2) against a variant's map; unknown names go to `unmatched`. */
   PlanarDrawPins ParsePlanarDrawPins(const PlanarShapeVariant &v, std::string_view text);
   ```
   `PlanarDrawOptions`, after `elastic`: `const PlanarDrawPins *pins = nullptr; ///< Answer-key pins (D393): every pinned point is within window of its position plus one common shift (two free integer variables).`
2. `planar_draw.cpp`:
   - **Pins:** two shift variables (sx, sy) in [-COORD_MAX, COORD_MAX], integer. Per pin, per axis: `at - window <= p - s <= at + window`, where p is `point[track].front()`/`.back()` or `cross_point[index]`. Tag `pin end <name> start|end` / `pin cross <a> <b>`. Pin rows are hard: no slack even in elastic mode (add `pin` to the families the elastic code exempts, beside `piece` and `box`).
   - **D412:** a port line's stretch from its port to its first event, when that event is a node, may have total length 0. Its pieces there take least length 0. A flex group's least-length row is dropped when the whole group lies in that stretch; otherwise, no change. Every other stretch is unchanged. Write it exactly as the existing flex-group least-length logic writes "may be 0", reusing that mechanism.
3. `src/junction/planar/CLAUDE.md`, "Drawing program": a bullet on pins (D393, `PlanarDrawPins`, `ParsePlanarDrawPins`, hard rows tagged `pin`, one common shift, window 2) and a sentence on D412.

## 5. Tests (outside-in), `src/tests/junction/planar/planar_draw.cpp`
1. **Inner `planar draw: pins hold points to an answer key`**, `[junction][planar][planardraw]`:
   - On `ShapeFirst(PlanarTText(1), 1)` with `{.clearance = false}`, solve once. Write pins from the result's `points`: every track's start and end, each shifted by (+100, +60). Parse them, solve again with the pins: feasible, and every pinned point is within 2 of its pin minus the shift found.
   - Move one pin by 40 half tiles along x: infeasible, with a `pin` row among the conflicts. Or, in elastic mode, `stretched` has no `pin` tag but some other family stretches.
   - A pin naming an unknown line goes to `unmatched`.
   - Red: write the test first and record the failure (compile error is not red: add the declarations with empty bodies first).
2. **Inner `planar draw: a port lane may be 0 long before its first node (D412)`:** find, on T1 or T2's shaped variant, a port line whose first event is a node. With `{.clearance = false}` and a pin forcing the node point onto its port's box edge, the drawing is feasible after the change and infeasible before (record the before). If T1 and T2 have no such line, say so and use the extractor's 197 (the in-port line `L99`, empty) in the hidden test below instead.
3. **Hidden measurement `planar draw: the extractor's PSG 197 with pins`**, `[.][planarkeepapart]`: load and shape the extractor's 197 as the X8 test does (flex and cells). If `tools/junction/corpus/out/psg197/pins.txt` exists, parse it, WARN the unmatched count and the first 10 unmatched lines, and run `{.elastic = true, .pins = &pins}` (60 s). WARN status, total slack, families (count, sum) and the first 40 stretched tags. Without the file, WARN "no pins yet" and return. Run it; at this step the file does not exist yet.

## 6. Do not
- Change other rows, defaults, Shape, or the extractor files.

## 7. Stop conditions
- Any existing test fails; anything not covered.

## 8. Gates
- Debug `build` -j 4 (to a file), `tools/junction/jt fast build '[planardraw]'`; Release `build-rel` -j 4, target `openttd_test` only (a GUI client runs from `build-rel/openttd`: do not rebuild or kill it), `JT_JOBS=4 tools/junction/jt gate build-rel` (about 31 s, OVER BUDGET at 30 s known; `FAILED:` lines inside passing cases are not new).
- Commit: `Change: Junction drawing program: answer-key pins (D393) and port lanes of length 0 (D412)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 9. Report (short, the full content)
- References read; commit; red evidence; types and members added
- Suite results per gate (count, time)
- D412: which line was used and the before/after
- Stops; state (`git status --porcelain`, `git log -1 --format='%h %s'`)

When you have reported, stop.
