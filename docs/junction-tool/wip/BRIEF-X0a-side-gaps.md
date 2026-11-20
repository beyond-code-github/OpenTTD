# BRIEF X0a: Structure hands down the side gaps (Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-slots, head {HEAD}. Release build dir /home/pete/OpenTTD/.claude/ng1/build-rel (persistent; do not reconfigure).
- Implements: wip/SPEC-slots.md §1 ("Added to Structure's output") and §7 X0a; ruling D377 (Q3) in docs/junction-tool/DECISIONS.md. Slots may not read the spec (D366), so Structure carries the port pins down, as it already carries `PlanarStructure::limit`.

## Read first (once, with offset/limit)
- /home/pete/OpenTTD/docs/junction-tool/DECISIONS.md: grep for `| D377 |` and read that row only.
- /home/pete/OpenTTD/.claude/ng1/src/junction/planar/planar_structure.h (whole, short).
- /home/pete/OpenTTD/.claude/ng1/src/junction/planar/planar_structure.cpp: `StructureLimit` (about line 70), `BuildPlanarStructure`'s variant assembly (about lines 305-325), `CheckPlanarStructurePromise`'s P5 block (about lines 455-470).
- /home/pete/OpenTTD/.claude/ng1/src/tests/junction/planar/planar_structure.cpp: `MakeHandCase`, `RequireOnly`, and the test "planar structure: P5 a structure's limit that is not the spec's" (about line 289): your test copies its form.
- /home/pete/OpenTTD/.claude/ng1/src/junction/planar/CLAUDE.md, section "Structure".
- CODINGSTYLE.md (C++ part).

## Test first (red, then green)
Add after the P5 limit test in src/tests/junction/planar/planar_structure.cpp:
```cpp
TEST_CASE("planar structure: P8 a side gap that is not the spec's", "[junction][planar][planarstructure]")
{
	const HandCase h = MakeHandCase();
	PlanarStructureVariant v = h.v;
	v.side_gap[static_cast<size_t>(PlanarSide::E)] += 1;
	RequireOnly(h, v, "P8");
	CHECK(CheckPlanarStructurePromise(h.tv.topology, h.spec, v)[0].find("side gap") != std::string::npos);
}
```
Order: add the member and `PlanarSideGaps` (changes 1-2) and fill them (changes 3-4) so the test compiles; run it and record the failure (expected: `RequireOnly` finds no P8 message); then add the oracle row (change 5) and see it pass.

## Changes
1. planar_structure.h, in `PlanarStructureVariant` after `structures`:
   `std::array<int, 4> side_gap{}; ///< Per box side (index PlanarSide: N, E, S, W), the slot distance between the last line of one port and the first line of the next port on that side: gap + 1 (ml_gap on E and W, branch_gap on N and S; D377). Slots reads it, never the spec.`
2. planar_structure.h, a declaration with a doc comment in the file's style:
   `std::array<int, 4> PlanarSideGaps(const JunctionSpec &spec);` returning `{spec.branch_gap + 1, spec.ml_gap + 1, spec.branch_gap + 1, spec.ml_gap + 1}` (index order N, E, S, W); defined in planar_structure.cpp next to `StructureLimit`.
3. `BuildPlanarStructure`: when assembling each kept variant, `v.side_gap = PlanarSideGaps(spec);` (next to `v.structures = ...`).
4. `MakeHandCase` (test file): wherever it fills `h.v`, set `h.v.side_gap = PlanarSideGaps(h.spec);` so the valid hand variant stays clean. If `h.v` comes from a function that already goes through `BuildPlanarStructure`, change nothing here.
5. `CheckPlanarStructurePromise`: after the P5 block, a P8 block:
   `if (v.side_gap != PlanarSideGaps(spec)) out.push_back(fmt::format("P8: side gaps {}/{}/{}/{} (N/E/S/W), the spec gives {}/{}/{}/{}", ...));` (the word "side gap" must appear in the message: write "P8: side gaps ...").
6. The header comment of `CheckPlanarStructurePromise` (and any comment listing "P1-P7") now says P1-P8.
7. src/junction/planar/CLAUDE.md, "Structure" section: after the `limit` bullet add: "- Each variant carries `side_gap` (index `PlanarSide`): the slot distance between consecutive ports on that side, gap + 1 (`ml_gap` on E and W, `branch_gap` on N and S; D377), from `PlanarSideGaps(spec)`; P8 checks it. Slots reads it, never the spec." In the oracle bullet, "P1-P7" becomes "P1-P8".

## Do not
- Touch Shape (`planar_shape_layer*`), the track map, or any other layer; add a gap per port pair or per port; read ports from the topology here.
- Change any existing test's expectations.
- Run the wide gate or any other heavy job.

## Stop conditions
- `MakeHandCase`'s variant cannot get `side_gap` by the line in change 4 (e.g. it is built in several places): stop and report where.
- Any existing test turns red.

## Gates and commit
- `/home/pete/OpenTTD/tools/junction/jt fast /home/pete/OpenTTD/.claude/ng1/build-rel '[planarstructure]' 10` green.
- `/home/pete/OpenTTD/tools/junction/jt fast /home/pete/OpenTTD/.claude/ng1/build-rel '[planarshape]' 10` green (Shape's oracle runs Structure's P1-P7 as H9; it must stay clean).
- One commit on wt-slots: `Add: Junction planar Structure hands down the side gaps (P8)` ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report (short, this format)
- References read
- Commit: hash and subject
- Red evidence: the P8 test's failure message before change 5
- Types and members added: names only
- Suite results: per jt run, cases, passed, time
- Stops, anything left undone
- State: `git status --porcelain` and `git log -1 --oneline`
When you have reported, stop.
