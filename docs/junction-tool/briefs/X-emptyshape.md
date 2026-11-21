# Brief X-es: empty lines get a heading in shape.txt

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. (Re-launch: a previous run stopped on cascade middle lines; the rule for them is now in section 3.) Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng2`, branch `wt-x`, head `{HEAD}`. Python only, `python3 -I`.
- Finding (X8 rerun): the C++ shape loader refuses the extractor's 197 with "port line L99 has no headings". L99 is an **empty line**: no track between two events. It is an E in-port lane whose split is on the box-edge tile itself (`node L99 -> L96, L94`). `report.txt`: 3 empty lines on 197. The shape writer (`write_shape`) writes only lines with track (`Sh.ES`). Every line in topology.txt needs a `shape` line (at least one heading).

## 2. Read first
- `extract.py`: `write_shape`, `build_text_model` (how empty lines are created: `newline`, `edge_line`, `T.empty`, keys `('e', a, b)` and others), and the node-piece heading helper added in e3f6d414df in `structure()` (it computes a node piece's heading in travel order from `X5.shared`/`ends`).
- `out/psg197/topology.txt` (which names are empty lines: the 3 in `T.empty`) and `tools/junction/topologies/psg197.shape.txt` (how a hand shape line looks; the loader's format).

## 3. The rule
- An empty line's shape is one heading: the travel heading at the point where it sits.
  - An empty line between two node pieces (`('e', a, b)`): the heading of piece a at its shared end with b, by the same helper `structure()` uses for a node piece.
  - An empty port line (a lane that is itself a node piece): the port heading (`HEAD_IN` / `HEAD_PORT`, as `structure()` uses for a port trunk).
  - A cascade middle line (key `('c', pc, ...)`: the line between two statements of one 3-way split or merge piece): the node piece's trunk heading. For a split, the heading its trunk arrives with (as `structure()` takes it); for a merge, the heading its out leg leaves with. Inside the node tile the middle line continues the trunk.
  - Any other empty-line kind: if it has no clear heading by these three rules, stop and report its key and where it is.
- `lengths.txt` gets `lengths <name>: 0` for each empty line (one piece of length 0). Note: the drawing program requires axis pieces of at least a tile; a 0 here is V's fact, not a rule change.
- The report's empty-lines line drops "they have no ... shape" and says the shape is one heading of length 0.

## 4. Checks
- Every line named in psg197's and psg186's topology.txt has exactly one `shape` line (a one-off check, not committed).
- `compare.py psg197`: shape lines with a match before (99 of 100) and after; any other difference class change.

## 5. Do not
- Change any other stage.

## 6. Commit
- `Fix: Junction corpus: empty lines get a heading in shape.txt`; regenerate and commit `out/psg197` and `out/psg186`; ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Do not commit `__pycache__`.

## 7. Report (short, the full content)
- References read; commit; each 197 empty line with its heading and rule; the checks
- Stops; state (`git status --porcelain`, `git log -1 --format='%h %s'`)

When you have reported, stop.
