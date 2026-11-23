# BRIEF node-cusp: a prio joiner's S-bend at its node (D449 node shapes, step 1; D451)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes in all. Each solve in the probe run (part 4) is capped at 15 s; run the probe at most twice (before and after). No rerun loops: a result you do not expect is reported, not retried with other knobs.

## 1. State
- Run 2. Part 5.1 is DONE (440cdf2263, `PlanarLegTurnedHeading`); start from it. The first run's parked diff is `/home/pete/OpenTTD/docs/junction-tool/wip/node-cusp-wip.patch` (reference only: it holds temporary hooks; do not apply it whole). Commit only the second commit of part 9.
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>` (= feature/junction-tool). Builds: Debug `build`, Release `build-rel`.
- Design: DECISIONS D449 (the bend pre-program decides S-bends; node shapes are its first part) and D451 (this rule). Evidence: `docs/junction-tool/wip/probes/slots/flank-lock.md`, last section ("Shape's node waiver at prio joins").

## 2. Read first (once each, with offset/limit)
- `CODINGSTYLE.md` (C++ part).
- `src/junction/planar/CLAUDE.md`, the "Drawing program" section only (node_flank, flank rows, node lead).
- `src/junction/planar/planar_shape_layer.cpp` lines 547-597 (`CheckH3`: how `turned` is computed).
- `src/junction/planar/planar_draw.cpp` lines 555-640 (flex groups, `node_flank`, the per-piece bounds `free_piece` / `capped` / `held`), and the `flank_turn` block (grep `flank_turn`).
- `src/junction/planar/planar_draw.h` lines 115-125 and 170-180 (`PlanarDrawResult`, `node_flank`, `flank_turn`).
- `src/tests/junction/planar/planar_domain_encoding.cpp` lines 7275-7345 (how the `[slotsstep2]` probes build T2's variant and read the env knobs) and the probe at line 6664 (`[slotsstep2]`, "stage B from ContractedVariant of stage A") where `PLANAR_NODE_FLANK` is read (line 6721).

## 3. The rule (D451)
**Run 2 (after the first agent's stop, 2026-11-25):** a cusp is a leg that is itself a Line track, is not the site's straight leg, and whose heading at the node equals the trunk heading h. The OTHER tracks at the node may be Rows or copies (prio joins carry a row: T2 node 8 holds prio:B1, node 9 prio:B2, node 10 prio:E1); CheckH3's all-Lines filter is NOT used here. On T2 this gives B1 (node 8), B2 (node 9), D1 (node 10). B1 and B2 are single pieces with no flex (parent chains of their rows): they are reported missing and left as they are (they do not break F8 today); D1 is the one forced. Such a cusp gets, at its node end:
- the **insert** = the flex middle in its **node-end flex group** nearest the node whose heading is `turned` (same heading mod 8; `turned` exactly as `CheckH3` computes it): its length's lower bound is the least length (variable lower bound 1, as any required piece; coefficient unchanged), its upper bound `coord_max`; nothing else caps it;
- every piece strictly between that insert and the node (the flank after it, other inserts, the last flank): lower bound 0 and upper bound 0 (held at 0). This overrides `node_flank` for this leg end.
So the joiner arrives on the turned heading, flush at the node, as the game's prio join does (it turns in on the merge tile).

Definitions (pieces of the leg's `PlanarShapeTrack`, n pieces; flex groups as in planar_draw.cpp: consecutive middles 2 apart):
- merge leg (node at the track's end): the node-end group is the group whose largest middle is n - 2; search its middles from the largest down; held pieces are (insert, n - 1].
- split leg (node at the start): the node-end group is the group whose smallest middle is 1; search from the smallest up; held pieces are [0, insert).
- If there is no node-end group, or no middle in it on `turned`, the cusp is **missing**: record the reason, leave the leg as before. This is an upstream gap (Shape must offer the insert); do not invent another piece.

Expected on T2 node 10 (from the evidence): D1's last pieces are `10: h1 middle, 11: h2, 12: h3 middle, 13: h2`, trunk ED1 h2. Report the `turned` you get for D1 and which piece is the insert; do not assert a heading the code does not give.

## 4. Tests, outermost first
**Test A** (new, `src/tests/junction/planar/planar_domain_encoding.cpp`, tags `[junction][planar][planarcusp]`): "planar draw: a prio joiner's node S-bend is forced at its node on T2 (D451)".
- Build T2's variant exactly as the `[slotsstep2cdiag]` test does (topology `PlanarTText(2)`, `BuildPlanarStructure(topology, spec, 1)`, `ShapePlanarStructureVariant`, strip the flex, `PlacePlanarShapeFlex(v, why, true, false, 1)`).
- `PlanarNodeCusps(v)` returns exactly three cusps, at nodes 8, 9 and 10, tracks B1, B2, D1 (by `map.tracks[t].name`); B1 and B2 missing with reason "no flex group at the node end"; D1 not missing, `insert` 10, `held` {11, 12, 13}, and `pieces[insert]` is the same heading mod 8 as `PlanarLegTurnedHeading(...)` for that leg and not the trunk's; `held` is exactly the pieces between the insert and the node.
- Then build the stage A program with `build_only = true`, `program_out = true`, options as the step2b probe's stage A with `flank_turn`, `node_flank` and `node_cusp` true; assert `PlanarDrawResult::cusps_forced == 1`, `cusps_missing.size() == 2`, and that no row's tag is `flank D1 10 after` (merge leg: the node side is `after`).
- Red first: add the declarations and a `PlanarNodeCusps` that returns `{}` and a `node_cusp` that does nothing, build, run, record the assertion failure text. Then implement.
- Budget: `tools/junction/jt fast build '[planarcusp]' 5` in Debug. Over budget is a stop.

**Test B** (existing): `CheckH3` behaviour is unchanged after the extraction (part 5.1): `tools/junction/jt fast build '[planar]'` and the gate.

## 5. Changes
1. `planar_shape_layer.{h,cpp}`: extract `int PlanarLegTurnedHeading(const PlanarShapeVariant &v, const PlanarStructureSite &site, uint32_t leg)`: the `sigma` and `turned` lines of `CheckH3` (straight leg, fork, `Side` fallback to `site.side`) moved verbatim; `CheckH3` calls it. Pure refactor; keep the comments.
2. `planar_draw.h`: 
   ```cpp
   /** A cusp at a topology node (D451): a non-straight Line leg at the trunk heading, and its node-end S-bend insert on the turned heading. */
   struct PlanarNodeCusp {
   	uint32_t track = 0; ///< The leg.
   	uint32_t node = 0; ///< The site's node.
   	uint32_t insert = UINT32_MAX; ///< The insert's piece, UINT32_MAX when missing.
   	std::vector<uint32_t> held; ///< The pieces between the insert and the node, held at 0.
   	std::string missing; ///< Why there is no insert (empty when there is one).
   };
   /** Every cusp of the variant's sites, in site order (D451). */
   std::vector<PlanarNodeCusp> PlanarNodeCusps(const PlanarShapeVariant &v);
   ```
   `PlanarDrawOptions::node_cusp` (bool, default false, a probe; doc comment citing D451), `PlanarDrawResult::cusps_forced` (size_t: cusps whose insert was forced) and `PlanarDrawResult::cusps_missing` (std::vector<std::string>: "track node: reason").
3. `planar_draw.cpp`: `PlanarNodeCusps` (cusp test exactly as `CheckH3`'s topology + `SameHeading(heading, h)` branch). In the program, with `node_cusp`: compute the cusps once; in the per-piece bound loop, the insert gets lower 1, upper `coord_max` and is not `capped`/`held`/`zero`/`reference`; the held pieces are `free_piece` true and capped (upper 0). Grid and `positive` follow from those bounds as for any piece.
4. Flank rows, generic (loosen, not special): in the `flank_turn` run beyond a flank, a piece whose upper bound is 0 (capped) is skipped in the heading comparison (it has no length, so it makes no turn); the run continues past it (to the next piece with a least length, or the track end and its node neighbour as now). Name in the report every row this removes on T1 and T2 (compare `flank_rows` and tags with `build_only` before and after). If anything other than the cusp inserts' node-side rows and rows next to inserts held at 0 disappears, stop and report.
5. `[slotsstep2]` probe (line 6721) and the step 2c probe (line 7332): read `PLANAR_NODE_CUSP` into `node_cusp` beside `PLANAR_NODE_FLANK`; print `cusps_forced` and `cusps_missing` in the stage A line.
6. `src/junction/planar/CLAUDE.md`, Drawing program section: one bullet for `node_cusp` (D451) and amend the flank rows bullet for the capped skip. Docs `docs/junction-tool/TESTS.md`: add the measured Debug time to the `[planar]` row text.

## 6. Probe run (after the tests are green; Release build)
From `/home/pete/OpenTTD/.claude/ng1/build-rel`, output to files under your session scratchpad:
`PLANAR_SLOTS_HUB=T2 PLANAR_FLANK=1 PLANAR_NODE_FLANK=1 PLANAR_NODE_CUSP=1 JUNCTION_MILP_INCUMBENT=1 ./openttd_test '[slotsstep2]'` (the step 2b probe). Run it once without `PLANAR_NODE_CUSP` and once with. Report from each: stage A status and time, whether an incumbent came back, `cusps_forced`, node turns (F6/F8) and any failing node lines, stage B, judge and domain lines. Do not chase a failure; report it.

## 7. Do not
- Do not touch Shape's heading program (forcing the joiner to turn in Shape was tried and reverted: it makes Shape infeasible).
- Do not change `node_flank`, `node_lead`, any default, or any other probe's knob.
- Do not add a piece, insert or flex group anywhere; a missing insert is reported.
- No Python rewrites of source files; edit with the Edit tool. No `git stash`. Never touch the rig (ports 4077/4079), game clients or other worktrees.

## 8. Stop conditions
- T2's cusps are not exactly B1, B2, D1 at nodes 8, 9, 10, or D1's insert/held differ from 10 / {11, 12, 13}.
- A node lead row (`lead`) names a forced insert, or the forced bounds contradict another row by construction (report the tag).
- The capped skip removes a row other than those named in 5.4.
- Any `[planar]` test other than test A changes result; over budget.

## 9. Gates and commit
- `tools/junction/jt fast build '[planarcusp]' 5`, `tools/junction/jt fast build '[planar]'`, `tools/junction/jt gate build-rel` (a known flaky SIGSEGV/SIGILL in legacy/rules tests that passes alone is CPU instability: rerun that test alone and say so).
- Commits: `Codechange: Junction: CheckH3's turned heading as PlanarLegTurnedHeading` (the refactor alone), then `Add: Junction: node cusp probe, a prio joiner's node S-bend forced at its node (D451)`. End each message with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 10. Report
- References read
- Commits: hash and subject
- Red evidence: test A's failure text before the code
- Types and members added or removed: names only
- Suite results (counts and times), naming any failure you did not cause
- D1's `turned`, the three inserts (track, piece, heading) and held pieces; the flank rows the capped skip removed on T1 and T2
- Probe run: the lines listed in part 6, before and after
- Stops, and anything left undone, with the reason
- State: `git status --porcelain` and `git log -1`

When you have reported, stop.
