# BRIEF G2e: beside tracks and their parents as rigid bundles (probe, Sonnet)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/ng1, branch wt-slots, head {HEAD}. Release build dir `/home/pete/OpenTTD/.claude/ng1/build-rel`. One file changes: /home/pete/OpenTTD/.claude/ng1/src/tests/junction/planar/planar_slots_probe.cpp.
- Why: G2d showed the same-channel clashes are beside tracks (copies and prio rows, `map.part_of[b] == UINT32_MAX` is NOT a beside track; see how the beside rows block picks them) against their parents: B1, B2, prio:B1, prio:B2. A beside track is held one lane from its parent chain by ONE equality per piece (section "beside rows", search `/* The beside point: an event on the piece`), but both tracks get their own optional step places and flex inserts, so elsewhere on the piece they drift into each other. Engineer's rule: valid by construction (rows from the map and Shape's headings before the solve; no solution read; no loop).
- Read once: /home/pete/OpenTTD/docs/junction-tool/wip/SPEC-slots-lengths.md §8.2 (steps, beside rows) and §10 (flex points); DECISIONS rows D381, D383 (grep `^| D381 ` and `^| D383 ` in /home/pete/OpenTTD/docs/junction-tool/DECISIONS.md).

## Changes
All `off` bits are taken, so the switch is a file-scope flag: add `static bool g2_rigid_beside = false;` next to `g1_time_limit` (search `static double g1_time_limit`), read by `SolveG1Raw`. Default false, so every existing run is unchanged.
1. **Shared pieces, computed before the element loop.** At the start of the element-building section (before the loop over tracks that builds `te`, search `e.stretch_begin.push_back(e.elems.size());` and go to the enclosing track loop's start), when `g2_rigid_beside` is true, build `std::set<std::pair<uint32_t, size_t>> rigid_piece`: for every beside track b (the same test the beside rows block uses to pick b, and `ParentChain(map, v.structure.sites, b, chain)` true, and the same `sb.pieces.size() == chain_piece.size()` check), insert (b, j) for every piece j of b, and (pt, jj) for every chain piece `chain_piece[j]` (build `chain_piece` exactly as the beside rows block does; copy that loop, do not refactor it into a function).
2. **No sideways moves on rigid pieces.** In the element loop, when `g2_rigid_beside` and `rigid_piece.contains({t, j})`: no optional step place on that piece (treat it as if `place_piece != j` and `every_piece` and `insert_place` are false for this piece) and no flex insert (`flex_place` false for this piece). Cusp steps (the `cusp(...)` calls) stay unchanged. Count `rigid_pieces_no_step` (places removed) and `rigid_pieces_no_flex` (flex inserts removed) in `G1Result` (two `size_t` members next to `nbr_pairs`, one doc comment line).
   - If removing a step place leaves a stretch with no step place where the code assumes one exists (a crash or an assert), stop and report.
3. **Beside rows at both ends of each piece.** In the beside rows block, when `g2_rigid_beside` is true, replace the one row per piece by: for piece j of b with chain piece (pt, jj):
   - if j > 0: a row at the START: beside point `position(b, eb.piece_begin[j])`, parent point `position(pt, te[pt].piece_begin[jj])`;
   - if j + 1 < pieces: a row at the END: beside point `position(b, eb.piece_end[j])`, parent point `position(pt, te[pt].piece_end[jj])`;
   - each row exactly as today: `dot = Dot(beside - parent, STEP[(h + 2) % 8])`, `= 2 s` with `s` from `map.Side(...)` computed as today (use the stretch of the element at that point on each track, as today's code does for its point), tag `beside <name> piece <j> start` / `... end`, and push each to `beside_rows` as today. The prio-row option (off 32768) branch stays as it is for prio rows, applied to each of the two rows.
   - A one-piece beside track (j = 0 is also the last piece) gets no row: count it in `no_chain` as today.
4. **Test.** In `"planar probe: G2 chords on T2"` keep the `runs` array as it is and run the whole list twice: first with `g2_rigid_beside = false`, then `true` (set it back to false at the end). Print a `rigid=0/1` field after `run=` in the G2 line, and `rigid_no_step= rigid_no_flex=` after `frame_open=`.
5. **Commit:** `Add: Junction planar probe: G2e, beside tracks and parents as rigid bundles`.

## Evidence
- Probe: no red test. Run the test once with env `JUNCTION_G2_DIAG=1`, output to a file. For each rigid run that is infeasible, report its `why=` and the first 10 rows of its conflict. For each solved rigid run, report the count of `G2C` lines with `same_channel=1` grouped by track pair (two names), as `grep '^G2C' ... | grep 'rigid=1' | grep same_channel=1` then the pair counts (`awk` on the two track-name fields, `sort | uniq -c`). If `G2C` lines do not carry the `rigid=` field, add it there the same way as on the G2 line.

## Run (Release, one at a time)
- `cmake --build /home/pete/OpenTTD/.claude/ng1/build-rel --target openttd_test -j8 > /home/pete/OpenTTD/.claude/ng1/build-rel/g2e-build.txt 2>&1`, grep for `error`.
- `cd /home/pete/OpenTTD/.claude/ng1/build-rel && JUNCTION_G2_DIAG=1 timeout 300 ./openttd_test "planar probe: G2 chords on T2" > g2e-run.txt 2>&1; grep '^G2 ' g2e-run.txt > g2e.txt`
- `/home/pete/OpenTTD/tools/junction/jt gate /home/pete/OpenTTD/.claude/ng1/build-rel`

## Do not
- Change any other row, chord, neighbour row, default or off bit; read a solution to decide a row; loop solves; refactor shared code into functions; use functions `safeguards.h` forbids (`std::stoi`, `std::isdigit` and the like: the build fails); touch any other file.

## Stop conditions
- The beside-track test or `ParentChain` behaves differently from the beside rows block (a beside track found in one place and not the other): report and stop.
- A crash or assert from a stretch without a step place: report and stop.
- Infeasible rigid runs: do not change the rule; report as in Evidence, commit, stop.

## Time box
- Total about 1 hour. The probe's 15 s cap per solve. At most one diagnostic run beyond the planned run.

## Report (short)
- Commit hash; per run and rigid flag: solved, ms, steps, W x H, clashes (same_channel/face/noface), curve, chords, nbr_pairs, rigid_no_step, rigid_no_flex; the same-channel pair counts per solved rigid run; any conflict rows; gate (count, time); `git status --porcelain`, `git log -1`.
When you have reported, stop.
