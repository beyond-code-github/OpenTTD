# BRIEF domain-51: a line may run along a tunnel over its middles (D438; closes F9)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes wall. One production change (the keep-apart pair loop); the wide gate once. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- D438 (engineer): a crosser may run along a tunnel's line above its middle tiles (V's L40 over L57, L89 over L74 on PSG 197). F9 (domain-50, `docs/junction-tool/wip/probes/domain/f9-diag.md`, read it in full) is the same shape: on X2, B1 crosses A1's tunnel on its diagonal (D414) then runs east ALONG the tunnel over a middle tile; the keep-apart rows (`sanctioned` exempts only the crosser's own crossing piece against the span piece) refuse it; the domain accepts. A blanket exemption leaks (B1's next piece on the PORTAL tile is accepted), so the end clearance is part of the rule.
- Read first: f9-diag.md; DECISIONS D437, D438, D436; planar_draw.cpp the pairwise pair loop, `span_piece`, `sanctioned`; the X2 fixture and its `[!shouldfail]` exactness test; the 197 hidden test.

## 2. The rule (planar_draw.cpp, keep-apart pair loop)
For a TUNNEL structure: a wall pair (separator's span piece, a piece of one of the structure's crossers) is skipped (reason `over middles`) when the crosser's piece lies within the span piece's middle part: along the separator's axis, both of the crosser piece's endpoints are at least the structure's end run (in the separator's own length units: `end_run` pieces of the separator's heading, as the end-run rows measure it) from both ends of the span piece, and across the axis within one lane of the separator's line. Bridges are unchanged (D438: a deck is the surface). Comment: `/* D438: ground over a tunnel's middles is free; its portals are not (end clearance). */` If "within the middle part" cannot be read from points that are variables at row-writing time (the pair rows are written before the solve), use what the pair loop already knows (piece indices relative to the crossing piece and Shape's headings) and say exactly what condition you implemented; if neither works, stop and report.

## 3. Tests
- Pinned behaviour test (through the judge): "planar domain: a line may run along a tunnel over its middles (D438, F9)": X2's first F9 example (B1 = N2, NE3, E2, ... from f9-diag.md) on X2: judge and domain accept. Counterpart: the same family with E4 (B1's next piece reaches the portal tile, f9-diag.md's leak): judge and domain refuse. Show the first test fails without the change (scratch run).
- X2's exactness: remove `[!shouldfail]`; all eleven configurations 0/0 (a new too loose anywhere is a finding: report in full and stop before committing).
- The 197 answer key (hidden, Release once): report whether the 13 rows of L40/L57 and L89/L74 (keep apart 10, deck 2, port order 1) clear; list what remains by rule. If they do not clear, say which condition differs from X2's case (e.g. V's run is longer, or deck/order rows need their own change) — do not change those rows.

## 4. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5`, `tools/junction/jt fast build '[planardraw]' 30` (Debug); `tools/junction/jt gate build-rel` once (a one-off SIGSEGV/SIGILL that passes on rerun is the known CPU instability: report it).
- One commit: `Fix: Junction: a line may run along a tunnel over its middles (D438, F9)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`; planar CLAUDE.md keep-apart bullet updated (D225).

## 5. Report (short)
References read; commit; the exact condition implemented; red evidence; exactness; 197 refusals after; gates; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
