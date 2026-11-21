# Brief X-cyc: the extractor writes a loop in PSG 197's topology (L98 -> L113 -> L98)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng2`, branch `wt-x`, head `{HEAD}`. Work and commit only there. Python only, `python3 -I`.
- Finding (X8, 2026-11-22): the C++ parser dies with `std::bad_alloc` on `tools/junction/corpus/out/psg197/topology.txt`. The route walker loops. The topology has a cycle:
  - line 72: `node L98=D1, L99=D1 -> L113=D1`
  - line 73: `node L113 -> L98, L96`
  - L98 is both an input of the merge into L113 and an output of the split from L113: one line name for two different stretches, or a 2-in-2-out piece written as merge then split with a name reused. The report also lists `lattice-like ... L80 x L98`.
- A topology must be acyclic: every line runs from a port or node to a node or port, and following lines from in ports must never return to a line.

## 2. Read first (once each, offset/limit)
- `tools/junction/corpus/README.md`.
- `tools/junction/corpus/extract.py`: `lines()` (~178 onwards: switch nodes, kinds `split`/`merge`/`cross`, how lines get their ids), `write_topology`.
- `out/psg197/topology.txt` lines 60-80 and `out/psg197/report.txt`.

## 3. Steps
1. **Diagnose first, then report in the commit body and the report.** Find the pieces and tiles behind L98, L99, L113 and L96, and say which mechanism produces the reused name: a 2-in-2-out (`cross` kind) node piece, a line id given twice, or something else. Write that down before changing code.
2. **Guard (always):** in `extract.py`, before writing, check that the line graph (node in-lines to out-lines) is acyclic. On a cycle, the report gets a checkpoint line `cycle: L98 -> L113 -> L98` and `topology.txt` is not written (the other files may be). It is a named failure, never silent.
3. **Fix:** make the mechanism you found produce distinct lines (each line one stretch between two events), generically: no hub-, line- or tile-specific case. If the right fix needs a choice (for example how a 2-in-2-out node piece is written in the notation: as two nodes with a line between them, or another form), stop after step 2, commit the guard, and report the choice with its options.

## 4. Checks
- psg197: the topology is acyclic. `compare.py psg197` shows no new difference class compared to before the fix (record before and after). Count lines, nodes and crossings before and after.
- psg186 (Brainmelter): the extraction runs; report whether it is acyclic.
- A one-off `python3 -I` script (not committed) that walks every route's paths over the new psg197 topology and prints the largest path count per route, to show the parser's walk will end.

## 5. Do not
- Change `hubs.tsv`, the lane-direction code, `load.py`'s geometry, or the hand-authored files under `tools/junction/topologies/`.

## 6. Commit
- `Fix: Junction corpus: no reused line in a node chain (acyclic topology)` (or, when stopping at the guard, `Change: Junction corpus: refuse a cyclic topology`), body with the diagnosis; regenerate and commit `out/psg197` and `out/psg186`; ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Do not commit `__pycache__`.

## 7. Report (short)
- References read; commit; the diagnosis (mechanism, tiles)
- psg197 before/after counts, compare.py differences, the path-count check; psg186 outcome
- Stops and choices; state (`git status --porcelain`, `git log -1 --format='%h %s'`)

When you have reported, stop.
