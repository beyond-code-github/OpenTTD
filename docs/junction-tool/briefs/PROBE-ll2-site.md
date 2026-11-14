# PROBE: LL² keep's last conflict (bridge over S-bend, edge 13) and the tunnel case

You execute this brief and nothing else. It is a probe on the probe branch: nothing is merged. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. Keep your context small: send command output to files and report only what failed.

## Where we are
`docs/junction-tool/PROBE-ll2-elastic.md`, section "Correction": LL² keep (any gap tried, prio 9 or 14, tunnels or not) fails on one local conflict only: "bridge over S-bend (edge 13) <- lane split run (edge 15, node 4, row 3) <- bridge heads hug crossers (edge 15, row 3)", slack 5-6 on edge 13's bridge-over-S-bend row and 1 on the hug. With 4 single bridges set to tunnels (instance 5) the bridge-over-S-bend row is still there.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes`, branch `wt-probe-ll2` (HEAD: its "real gap 7" WIP commit). The probe test is `src/tests/junction/placement/junction_probe_ll2.cpp`, run with `PROBE_INSTANCE=<n> build-rel/openttd_test "[.probe]"`; results go to `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/probe-ll2/`.
- Never use git stash; never start a game client (the engineer's is running); never touch other worktrees or ports 4077/4079.

## Read first (offset/limit)
- `src/junction/topology/CLAUDE.md` (bridges and tunnels), `src/junction/placement/CLAUDE.md` (the single-bridge crossing forms, the joint).
- `src/junction/placement/junction_placement.cpp` and `junction_joint.cpp`: where "bridge over S-bend" rows are made (grep the reason string), and where a crossing's bridge `tunnel` flag is read (grep `tunnel`).

## Tasks
1. **The tunnel case.** Does the "bridge over S-bend" family (and the hug, heads, clearance families) apply to a single crossing whose `JunctionTrackBridge` has `tunnel = true`? Answer with file:line. Our rules for a tunnel (topology/CLAUDE.md "Bridges and tunnels"): a wormhole, portals on inclines, surface track may cross the tiles between portals, no heads hugging. If the families apply where the rules say they should not, make the minimal probe-only change that skips them for tunnels (in a test-local copy if the code is not reachable otherwise; if it needs a non-test source change, make it on this probe branch only and list it), rerun instance 5, and report its slack table and, at zero slack, place, render, `CheckJunctionRules`, the rendered ASCII and the layout JSON (as instance 3's first run did).
2. **The site.** For instance 3 (keep, gap 7), round 0: describe the conflict in plain terms. Which edges are 13 and 15 (from, to, which movement in coop terms), which crossing it is (edge 13 under which bridge), which crossing form (S-bend or square, and why the square form does not rescue it: run the joint with that group's form fixed to square and report its slack), what the 5 columns are (which point must move and which row stops it), and how node 4's lane split and the hug of edge 15's heads tie in. Use the instance's dump and ASCII; draw the site in ASCII with the edges labelled.
3. **If task 1 places LL² keep:** also run instance 2's topology at gap 1 with tunnels (a new instance 6), same outputs.

## Output
Append "## Site and tunnel probe" to `/home/pete/OpenTTD/docs/junction-tool/PROBE-ll2-elastic.md` (main tree); results under `docs/junction-tool/wip/probe-ll2/site/`. Update the probe patch (`git diff <first probe commit>^ > docs/junction-tool/wip/probe-ll2-elastic.patch` or equivalent) and commit WIP on the branch so the worktree is clean.

## Report
Task 1's answer and rerun table, task 2's plain description, any placed layout's checker violations by family and the JSON path, `git status --porcelain` and `git log -1`. When you have reported, stop.
