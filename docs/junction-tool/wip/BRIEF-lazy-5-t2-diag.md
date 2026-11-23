# BRIEF lazy-5 (diagnosis): why T2's variant is undrawable with every keep-apart row

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes wall; each solve capped at 15 s (elastic LPs are fast); no rerun loops. Diagnosis: fix nothing. A hidden test may be extended; commit it with the notes.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- lazy-4 (`docs/junction-tool/wip/probes/slots/lazy.md`): T2 (offers 1, D443 off), stage A with ALL keep-apart rows (4128 binaries) is Infeasible (proved by branching; empty conflict); with no keep-apart rows it is Optimal. Engineer: an infeasibility is an upstream problem, back to the unit tests. Either Shape's layout forces two walls closer than the rows allow (a Shape promise gap), or some rows are too strict for a situation T2 has and no suite configuration isolates (a suite gap).
- Read first: DECISIONS D409 (elastic), D436; planar_draw.h `elastic`, `elastic_families`, `elastic_integer`, `PlanarDrawResult::stretched`; the lazy test (stage A options via `SlotsOptions`).

## 2. Steps
1. Stage A's model with ALL keep-apart rows, elastic on `sep` only (`elastic = true`, `elastic_families = {"sep"}`, every other row hard), continuous (the LP; D409's diagnostic), then once with `elastic_integer = true` (15 s cap): total slack and the stretched `sep` rows (tag, slack), largest first.
2. If the LP's total slack is 0 but the integer one is not (or the integer is Error): the infeasibility is in the side choices' combination, not in any single pair: report the stretched pairs of the integer run if any, else say so.
3. For each stretched pair (at most 10, largest slack first): its two walls (track, piece, heading), where they are in T2 (which node, crossing, prio row, copy, port: from the map), the face, and what a drawing must do to satisfy it (the candidate and the shortfall). Then: is the closeness the drawing achieves legal in the game? Take the elastic drawing's points around the pair, and ask the domain on that local geometry (`CheckPlanarDomain(PlanarDomainInputOf(...))` on the whole elastic drawing; report the domain's breaks on or near those two tracks). Class each: TOO STRICT (the domain accepts the closeness: a suite gap; describe the local configuration that would isolate it) or UPSTREAM (the domain refuses it too: Shape's layout forces it; say which Shape decision).

## 3. Output
`docs/junction-tool/wip/probes/slots/t2-infeasible.md` (the stretched pairs, each classed with the evidence); commit `Doc: Junction: why T2 is undrawable with every keep-apart row (lazy-5)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 4. Report (short)
References read; commit; total slack (LP, integer); each stretched pair, class and evidence; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
