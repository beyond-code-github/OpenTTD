# BRIEF domain-37: a zero track is transparent to keep-apart (finishes F7)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. One production change; the wide gate runs once. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>` = the domain-35 WIP commit (`WIP: Junction: F7, gate failing`). You finish it.
- domain-35's finding: with BC1 = 0 (a trident, D433), keep-apart separates ABC1/C1 and A1/B1 although the domain accepts them: in the pairwise face walk (planar_draw.cpp ~1451-1600), "adjacent" means consecutive walls (`j == i + 1 || (i == 0 && j == n - 1)`), and BC1's zero-length wall sits between them. A zero track joins its two nodes into one (the engineer's transparency ruling for zero tracks, domain-25/35): its walls are points.
- Read first: planar_draw.cpp ~560-700 (domain-35's `leg_zero`, `port_stretch`/`port_zero`) and ~1451-1600 (the pair loop and its `skip` reasons); the F7 test and K1 exactness test in the test file.

## 2. The change (planar_draw.cpp, pairwise pair loop)
- A track is HELD AT ZERO when `leg_zero[t]` (domain-35) or it is a `port_zero` zero line (`options.port_zero && port_stretch` after D434's branch). Record that per track (`std::vector<bool> zero_track`) where those are decided.
- In each face's walk `w`, before the pair loop: the walls of zero tracks are not paired (skip reason `zero track`), and adjacency is computed on the walk with those walls removed: two walls are adjacent when, walking from one to the other in either direction, every wall strictly between belongs to a zero track (this includes the plain consecutive case). Comment: `/* A zero track's walls are points (D433, D434): they are not separated, and the walls either side of them are adjacent. */`
- Nothing else changes.

## 3. Tests
- The F7 test passes (no `[!shouldfail]`); K1 exactness is 0 too strict and 0 too loose again. Report K1's classes (expect both accept up from 14 now that the trident geometries are connected and accepted).
- A pinned behaviour test (D436 style, through the judge): "planar domain: walls across a zero trident are not kept apart (D433, F7)": the domain-35 first example (`BC1.4=0 A1.0=1 A1.1=0 A1.2=2 C1.0=1 C1.1=0 C1.2=0`, or the F7 geometry if simpler): judge and domain accept. Counterpart: the same with BC1 = 2 where A1 and B1 then come within a lane of each other off the node (choose a geometry the domain refuses and the judge refuses with `keep apart`; if none exists within K1, say so and skip the counterpart).

## 4. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5`, `tools/junction/jt fast build '[planardraw]' 30` (Debug); `tools/junction/jt gate build-rel` once. Any failure: report each failing test and its first assertion, commit as WIP, stop.
- All eight exactness tests 0/0. A new too strict or too loose elsewhere is a finding: report in full, do not fix.
- Amend the WIP commit into one commit: `Fix: Junction: a connected trident is drawable; zero tracks are transparent; the suite's geometries are connected (D433, F7)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. The planar CLAUDE.md keep-apart bullet gains the zero-track sentence (D225).

## 5. Report (short)
References read; commit; K1 classes before/after; other fixtures unchanged (yes/no); the pinned test's geometries; gates; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
