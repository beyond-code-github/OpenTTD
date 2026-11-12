# IG5 (recon by experiment): why half the sloped straights still end flat when ramps are allowed (IG1 finding; engineer: "the terraform seems to be making everything flat rather than changing heights and working with the terrain")

Recon by experiment: change code in your worktree to measure, commit nothing; save experiments as a patch in /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/ig5.patch and leave the worktree clean. Keep your context small: output to files, report only what matters. Never use git stash. Never start a game client or touch the engineer's game or rig servers. Mark anything not measured or read as INFERRED.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-ig5 <HEAD>` (given at launch). Another agent works in /home/pete/OpenTTD/.claude/nodes2: never touch it. Use absolute paths.
- IG1 (docs/junction-tool/RECON-in-game-red-flat.md): with ramp_levels 3-5 (strong trains), 53-57% of originally sloped STRAIGHT track tiles still end flat after the terraform (curves/diagonals must be flat: the game). Not traced. Since then: HC1 (exact clamp cost), IG3 (bridge heads as the game), IG4 (ramp rule across bridges) merged at 6a19f837ce.
- The level solve: src/junction_levels.cpp (greedy start = cheapest flat, coordinate descent with move range d = 1, exact clamp objective; ramp rule windows; ties: nodes/switches/curves/heads flat, prio tiles tied to the main line).

## Experiments (Release, the 48 real fixtures at their centred offsets and the near-search offsets that plan; ramp_levels 3 and 5 via a spec with a strong train, as IG1 did)
1. For each sloped straight tile that ends flat: classify why it could not ramp: (a) a tie forces it (adjacent node/switch/curve/head within the same run: the run between two flat-forced tiles is too short to climb and come back within the ramp rule), (b) the ramp rule window (count), (c) cost: ramping is legal (feasible assignment exists with it ramped, everything else equal or re-solved) but costs more under the exact objective (show the cost terms both ways for 3 examples), (d) the descent never tries it (a cheaper legal assignment exists that the descent misses: find by a local exhaustive search over that run's levels for 3 examples).
2. For class (c): is the cost comparison right? (terraform under the ramped run vs under the flat run; foundation costs; the game's price for a ramp tile vs a flat tile with a foundation.)
3. For class (d): what search change would find it (e.g. start from ground-following levels instead of cheapest flat; moves of a whole run's levels by +-1; d = 2), and the plans/cost/time effect of the cheapest one on the 7344 offsets (LEVELHASH method: planned, mean cost on common plans, ms mean/max, HOVERNEAR max).
4. The share of track tiles at a level different from the run's neighbours' (i.e. how many "height groups" a junction has) before/after the best change: does the junction step with the terrain (D218 "fan up/down") or sit on one plane?

## Output
docs/junction-tool/RECON-why-flat.md (untracked, ~60 lines) and the patch path.

## Report
10 lines: the class counts, whether the cost comparison is right, the search change and its numbers, the height-group counts before/after, and a recommendation. `git -C /home/pete/OpenTTD/.claude/nodes status --porcelain` (clean). When you have reported, stop.
