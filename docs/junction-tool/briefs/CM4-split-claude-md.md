# CM4: split the CLAUDE.md draft into per-folder CLAUDE.md files

You execute this brief and nothing else. Make exactly the changes listed. Do not add any file the brief does not name. If something is not covered, or you would have to choose between approaches, stop and report the question. Keep your context small.

## State
- Repo /home/pete/OpenTTD, branch feature/junction-tool, head given in your launch prompt. Work in the main checkout. This is a docs-only step, so there is no build. Never use git stash. Ignore docs/junction-tool/wip/.
- MV1 moved the tool into src/junction/. docs/junction-tool/PATHS.md maps the old paths to the new ones.
- The draft is docs/junction-tool/CLAUDE-draft.md: about 225 present-tense fact lines, verified by CM2/CM3.
- D225 (in DECISIONS.md): CLAUDE.md files hold only current, decided facts, with no Open section and no open questions. Every commit that promotes a fact updates the matching CLAUDE.md.

## Placement (each fact goes in exactly one file, the folder whose code enforces it)
- `src/junction/CLAUDE.md`, which every junction agent loads:
  - a title;
  - a folder map: one line per folder, in pipeline order (terrain → topology → placement → tool → game), then line, station, legacy, and the root files (layout, route, train_model, rules checker);
  - Pipeline;
  - Game rules the model mirrors, except the lines that belong to terrain/ (below);
  - Working rules for agents, plus one new rule: "Every commit that changes behaviour or a rule updates the CLAUDE.md of the folder whose code enforces it, in the same commit, adding promoted facts and removing superseded ones; open questions never go in a CLAUDE.md (D225)."
  - Facts about the rule checker (rules.cpp) and the shared layout and route types.
- `src/junction/terrain/CLAUDE.md`: terrain snapshot, facts, terraform propagation, and the foundation and slope rules (GetRailFoundation, build_on_slopes) that facts.cpp mirrors.
- `src/junction/topology/CLAUDE.md`: Notation and topology; Graph, order and structure; Bridges and tunnels (as structure: which crossings are bridges or tunnels, doubling, series cap); Signals and prio. Add one line: "Horizontal placement (difference constraints, relation families, node positions) currently lives in junction_topology.cpp; placement/ holds the solver and the level solve."
- `src/junction/placement/CLAUDE.md`: Placement (relations, facts, choices); Heights (levels, foundations, terraform cost, cone, ramp rule, bridge heads, tunnel portals).
- `src/junction/tool/CLAUDE.md`: Hover and GUI (window, topology window, presets, persistence, near search, cancel, hover time target, cache, outlines, tooltip, hotkeys).
- `src/junction/game/CLAUDE.md`: estimate, build sequence, pricing, spent vs quoted, and network or command facts.
- `src/junction/legacy/CLAUDE.md`, a few lines:
  - the code here is superseded and is not extended;
  - who still calls it: generator from tool/junction_gui.cpp (an unreachable branch) and console_cmds.cpp; generator.h types used by many headers; nodes from game/junction_game.cpp;
  - new code must not add callers.
- `src/tests/junction/CLAUDE.md`: Testing (identity tests, tags, budgets, jt modes, red-first, what every step runs). Use the jt usage that MV1 found: the build dir is given relative to the worktree, and the tag is passed in brackets, e.g. `tools/junction/jt fast build '[topology]'`. Check this against tools/junction/jt.
- line/ and station/: create a CLAUDE.md only if the draft holds facts specific to them. If it holds none, create none.
- Facts that would fit two folders go in the one whose code enforces them. Do not duplicate them. If you cannot tell, put the fact in src/junction/CLAUDE.md.

## Also
1. Rewrite every file path in the facts to the new paths, using PATHS.md.
2. The roughly 20 lines CM3 left unchecked must each be confirmed against the code (now under src/junction/) or deleted: the signals checker rules (G9b/G9c), D70-D75, D116 prio-join geometry, waiting bays, D101/D137/D141, hotkeys, build sequence, pricing, D58/D182, and the YAPF and terraform price lines.
3. Keep the format: present tense, no dates, quotes or "why", ids in brackets. Each file starts with a one-line `#` title naming its folder.
4. `git rm docs/junction-tool/CLAUDE-draft.md`.

## Do not
Edit code, other docs, briefs or decisions. Do not add facts that are not in the draft, except the two named lines above.

## Gate and commit
- `git grep -n 'src/junction_' -- 'src/**/CLAUDE.md' 'src/CLAUDE.md'` returns nothing.
- Total lines across all the files: 200-300.
- One commit: `Doc: Junction tool CLAUDE.md files per feature folder`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
- Files and their line counts.
- Unchecked lines confirmed, fixed or deleted (counts, plus the deleted ones listed).
- The commit hash.
- `git status --porcelain`.

When you have reported, stop.
