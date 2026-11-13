# CM1: the junction CLAUDE.md files stand alone (D232)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2. Run `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt-cm1 <head>` with the head given in your launch prompt. Never use git stash. Docs only: no build, no tests.
- The rule (engineer): "Claude.MD should not know about or reference the decision log or any specific design plans - it stands alone as pure facts at the present point in time".
- Files: src/junction/CLAUDE.md, src/junction/{game,signals,tool,placement,terrain,legacy,topology,station,line}/CLAUDE.md (those that exist), src/tests/junction/CLAUDE.md.

## Change, per line of each file
1. Remove every reference to the decision log or a plan document: decision ids (`D38`, `D203(1)`, `D228a`), DESIGN sections (`G1 §11.8`, `DESIGN-G2 §G2.4`, `G2b`), brief, step, finding and card ids (`MJ1`, `RW1-RW6`, `R3`, `F3`, `P6b`, `HH2`, `IG3`, `TU1-TU4`, `C1`, `RC0`, `S0`...), file names under docs/junction-tool/ (DECISIONS.md, DESIGN-*.md, RECON-*, REVIEW-*, CARDS-*, briefs/, STATUS.md, FABLE-*), and phrases like "(engineer)" or "Pete ruled".
2. Keep: names of code (functions, types, constants, files under src/ and tools/), game source references (`CmdBuildBridge`, `tunnelbridge_cmd.cpp`), tool commands (`tools/junction/jt ...`), and the test budget table pointer `docs/junction-tool/TESTS.md` (it is a data table, not a plan; keep it verbatim).
3. If a line's meaning depends on a removed id (e.g. "placement refuses more (F3)", "a planned count may drop ... (P6b, HH2)", "until the RC0 audit shows full rule coverage"), rewrite it so the fact stands in plain words; say what is true now, not where it came from or what is planned. If a line only states a plan, an open item, or a process about the decision log (e.g. "Ids in brackets point to ...", "Grep DECISIONS.md for an id ... before asking", "a recorded decision is not re-asked", "(DECISIONS header)", "(briefs/ header)"), delete the line or the clause.
4. Do not add, drop or change any fact otherwise; do not merge or reorder lines; keep the wording of each line except where 1-3 require. Tidy the punctuation left behind (empty brackets, ", ;", trailing " ()").
5. Every remaining line is a present-tense fact.

## Check
- `grep -nE '\(D[0-9]|D[0-9]{2,3}\b|G1 §|G2|DESIGN|RECON|REVIEW|CARDS|STATUS|DECISIONS|FABLE|briefs?/' <files>` returns nothing, except the TESTS.md pointer if it matches.
- Before/after line counts per file; list each deleted line verbatim in the report (they are the only content removed), and each rewritten line as before/after pairs for lines changed by rule 3.

## Stop conditions
- A line you cannot make stand alone without changing its fact: leave it, list it, and continue with the rest; report them together.

## Commit
One commit: `Doc: Junction CLAUDE.md files stand alone, without decision or plan references`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
Commit hash; per-file line counts before/after; deleted lines; rule-3 rewrites; lines left for the orchestrator; `git status --porcelain` and `git log -1`.

When you have reported, stop.
