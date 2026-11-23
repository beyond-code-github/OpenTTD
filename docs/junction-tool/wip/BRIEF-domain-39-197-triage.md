# BRIEF domain-39 (diagnostic): V's 197 domain breaks, artefact or domain gap

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes wall. Diagnosis: fix nothing in the domain, the judge or the rows. Allowed: an env-gated dump in the hidden 197 test, a Python script under the worktree's `docs/junction-tool/wip/probes/domain/` (run with `python3 -I`), and a notes file. Never run git stash.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- domain-36: V's PSG 197 rebuilt from the extractor's files (`tools/junction/corpus/out/psg197/`: shape + steps as raw headings, lengths.txt, pins.txt start points) is judged ILLEGAL by `CheckPlanarDomain`: 64 breaks (span 13, connection 19, diamond 16, missing crossing 10, head 2, node 4) — report `docs/junction-tool/wip/probes/domain/answerkey-197.txt`. V's build is legal (it runs in the game), so each break is either an ARTEFACT of the rebuild (our points differ from V's tiles there) or a DOMAIN GAP (our tiles equal V's, and the domain refuses legal track).
- Read first: the 197 hidden test (`[.][planardomain197]`) in `src/tests/junction/planar/planar_domain_encoding.cpp`; `src/junction/planar/planar_domain.h` (what each break rule means; the rasteriser that turns points into tiles); `tools/junction/corpus/README.md` and `load.py` (V's real tiles from the save export; corpus root `--root` / `JT_CORPUS` / the default the README names); `tools/junction/corpus/hubs.tsv` (197's box).

## 2. Steps
1. Dump (env `PLANAR_DOMAIN_DUMP_197=<path>`, in the hidden test): the rasterised tiles of our rebuilt drawing as the domain sees them (per tile: x, y, the track pieces on it and which tracks), and every domain break with its rule, tile and tracks, as JSON.
2. V's tiles: with `load.py`, the rail pieces of 197's box from the save export, per tile (x, y, piece set). Map our half-tile map frame to tiles as pins.txt says (`tile (x, y) spans [2x, 2x+2]`), with the box shift the test applies (report it).
3. For every domain break: compare our pieces with V's on the break's tile and its 8 neighbours. Class: ARTEFACT (any difference: name it, e.g. "V's step is 1 tile later", "our L37 ends 1 tile short") or DOMAIN GAP (identical pieces on all nine tiles). Overall: count per class and per break rule.
4. If any DOMAIN GAP exists: describe the game situation in plain words (which pieces, what the game allows there), with the tile coordinates, so a hand case can be built in `[planardomaingame]`.
5. If the artefacts share a cause (e.g. a step placed at the wrong end of its stretch, a length offset at event pieces), name it and the count it explains.

## 3. Output
`docs/junction-tool/wip/probes/domain/triage-197.md` (counts, the class of every break in one line each, the shared causes, the domain gaps in full) and the script, committed: `Doc: Junction: V's 197 domain breaks triaged (D393)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. `tools/junction/jt fast build '[planardomain]' 5` must still pass.

## 4. Report (short)
References read; commit; counts per class and rule; shared causes; every domain gap in full; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
