# Brief RA (read-only audit, pass 1): every geometric constraint of the planar composer, traced to its source

You execute this brief and nothing else. Do not edit, build or run anything except reading files and writing the one output file named below. Keep your context small: read with offset and limit, grep scoped, read each file once.

## State
- Code: `/home/pete/OpenTTD/.claude/ng1`, commit `ea0f45d018` (branch probe-lj: the probe line the engineer's rig layouts come from; it carries 15+ probe commits not on the landed `wt-tracks`). Another agent switches branches in that worktree: read files only via `git -C /home/pete/OpenTTD/.claude/ng1 show ea0f45d018:<path>`, never the working tree. Mark a row `probe-only` in an extra column 7 when its code is behind an environment variable or a `PROBE` comment, or is absent on wt-tracks (`git -C ... show wt-tracks:<path>` to compare the function).
- Docs: `/home/pete/OpenTTD/docs/junction-tool/` (`DECISIONS.md`, `DESIGN-tracks.md`, `ROADMAP.md`).
- Output: `/home/pete/OpenTTD/docs/junction-tool/wip/AUDIT-rules.md` (do not commit).

## Why
Twice this week a "rule" holding layouts open turned out to be composer construction with no ruling behind it (fixed-length slot S-bends; a prio joiner crossing all nearer lanes on one diagonal, D356). The engineer wants every constraint audited, then the doubtful ones checked against the PSG corpus (pass 2, not yours).

## Scope (ea0f45d018)
- `src/junction/planar/planar_components.cpp` (composers: `Fray`, `Join`, `Fork`, `Double`, `PairDecks`, `Bundle`, `Flex`, `build_core`, `SlotBend`, port rows, side rules, fan rule, bay length, deck rows, prio rows, rungs, box).
- `src/junction/planar/planar_model.cpp/.h` (`AddPiece`, `PieceCurves`, `Cross`, `Deck`, clash separation, curve rows).
- `src/junction/planar/planar_slots.cpp` (slot rules) and `planar_paths.cpp` (`CheckPlanarShape`, default shape rules).
- `src/junction/planar/CLAUDE.md` for the stated facts.

## What to list
One row per distinct constraint (group identical rows from one code site): every `TagScope` row family, every `AddSeg` whose lower or upper bound is a fixed number other than 0 / none, every fixed piece sequence a component builds (e.g. "prio fray: stub, in-lane rise, fan"), every `Eq` that could have been a `Ge`, every up-front side or half-plane rule, every slot-layer rule, every shape rule, every refusal (`M.Fail(...)` text) that rejects a geometry.

Columns:
1. **Id** (R1, R2, ...).
2. **Constraint** in one plain sentence (what the track must do).
3. **Code** (`file:line`, tag or function).
4. **Source**: the decision id(s) whose ruling states it (quote the 5-15 words of the ruling that state it), or `game: <function/table>` for an OpenTTD mechanic (CLAUDE.md "Game rules the model mirrors"), or `checker: <rule>` when it mirrors a checker rule, or `none` (construction only). Grep DECISIONS.md for the constraint's key words before writing `none`; when a decision is related but does not state this constraint, write `none (related: Dxxx)`.
5. **Kind**: one of `exact-where-at-least-would-do`, `fixed length`, `fixed piece sequence`, `half-plane over whole segment`, `refusal`, `minimum spacing`, `other`.
6. **Corpus test**: a statement pass 2 could check on built PSG hubs, phrased as an observable ("in a prio join, a joiner crosses every nearer main lane on one straight diagonal"); `-` for game rules.

Order the rows: `none` sources first, then `none (related)`, then the rest. End with a count per Source class and per Kind.

## Do not
Edit any file but the output; build; run tests; touch ports 4077/4079; start a game client; use git stash; switch branches in the worktree.

## Report
Reply with the counts, the ids of the `none` rows each with its one-line constraint, and the output path. When you have reported, stop.
