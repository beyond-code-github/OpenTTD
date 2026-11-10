# FINDINGS O4b2b: main-line turn cost (D165), convergence

Measured in `/home/pete/OpenTTD/.claude/o4b2` (branch wt-o4b2, on the O4b2 WIP commit e790a645af). Test spec = `MakeTestJunctionSpec()` (rail_piece 100, tl 4, ml_gap 1, branch_gap 1), box W 44 x H 16, `max_feedback` 8, negotiation `max_rounds` 16 unless stated. ml_turn_cost values: 0 (baseline: the main lines pay the branch turn cost rail_piece / 2 = 50), 200 = rail_piece x tl / 2, 400 = rail_piece x tl, 800 = rail_piece x 2 x tl.
"E off row" / "W off row" count main-line pieces of EIn->EOut off row 0 and of WIn->WOut off row ml_gap + 1 = 2 (bridge pieces count as on-row when the bridge's heads are on the row); they are read from the routes of the last negotiation that converged, so "n/a" where iteration 0 did not converge.

## 1. D165 table (step 2), 16 rounds, no changes 4 and 5 yet

| ml_turn_cost | p | clean | why / violations left | iters | occupied | slivers | cost | E off row | W off row |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 14 | no | 2 conflicts left after 16 rounds: 11:ML tile in prio coverage does not hold the joiner; 7:less than one whole-train bay before the upstream split; 7:waiting room of 7 units, needs 8; 9:missing prio; 9:split inside prio coverage | 2 |  |  |  | 0 | 20 |
| 0 | 15 | no | a violation has no feedback: 6:signals too far apart; 7:less than one whole-train bay before the upstream split; 9:missing prio | 5 |  |  |  | 0 | 43 |
| 0 | 16 | yes |  | 7 | 267 | 44 | 39050 | 0 | 39 |
| 0 | 17 | no | the prio delays the main line | 5 |  |  |  | 0 | 33 |
| 0 | 18 | no | still unclean after the feedback iterations: 6:split in a bridge's block: no signal between the bridge and the merge; 6:split in a bridge's block: no signal between the split and the bridge; 9:missing prio | 8 |  |  |  | 45 | 42 |
| 0 | 19 | no | a violation has no feedback: 11:ML tile in prio coverage does not hold the joiner; 5:signal on head; 5:track on head; 6:signal on a bridge head; 9:prio block needs exactly one join; 9:split inside prio coverage | 6 |  |  |  | 0 | 36 |
| 0 | 20 | no | a violation has no feedback: 6:signals too far apart; 6:split in a bridge's block: no signal between the split and the bridge; 9:missing prio | 5 |  |  |  | 22 | 35 |
| 0 | 21 | yes |  | 3 | 230 | 41 | 33300 | 0 | 32 |
| 0 | 22 | yes |  | 3 | 230 | 39 | 33300 | 0 | 33 |
| 0 | 23 | yes |  | 3 | 230 | 39 | 33300 | 0 | 34 |
| 0 | 24 | yes |  | 3 | 230 | 39 | 33250 | 0 | 35 |
| 0 | 25 | yes |  | 3 | 230 | 39 | 33300 | 0 | 36 |
| 0 | 26 | yes |  | 3 | 230 | 39 | 33300 | 0 | 37 |
| 0 | 27 | yes |  | 3 | 230 | 39 | 33300 | 0 | 38 |
| 0 | 28 | yes |  | 3 | 230 | 39 | 33250 | 0 | 39 |
| 200 | 14 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 200 | 15 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 200 | 16 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 200 | 17 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 200 | 18 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 200 | 19 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 200 | 20 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 200 | 21 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 200 | 22 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 200 | 23 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 200 | 24 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 200 | 25 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 200 | 26 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 200 | 27 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 200 | 28 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 400 | 14 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 400 | 15 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 400 | 16 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 400 | 17 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 400 | 18 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 400 | 19 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 400 | 20 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 400 | 21 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 400 | 22 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 400 | 23 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 400 | 24 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 400 | 25 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 400 | 26 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 400 | 27 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 400 | 28 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 800 | 14 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 800 | 15 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 800 | 16 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 800 | 17 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 800 | 18 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 800 | 19 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 800 | 20 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 800 | 21 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 800 | 22 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 800 | 23 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 800 | 24 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 800 | 25 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 800 | 26 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 800 | 27 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |
| 800 | 28 | no | 2 conflicts left after 16 rounds | 1 |  |  |  | n/a | n/a |


Reading: ml_turn_cost 0 reproduces O4b2 (clean at p 16, 21-28; best occupied+slivers 269 at p 22; the W main line leaves its row for 32-39 pieces everywhere). At every nonzero value the negotiation itself fails at iteration 0 for every p: "2 conflicts left after 16 rounds". No clean layout exists at 16 rounds, so no compactness can be compared at that setting.

## 2. Why (step 3)

The conflict count per round at ml_turn_cost 400, test spec, p 20, 16 rounds: 6 4 2 2 2 2 2 2 2 2 2 2 2 2 2 2. The two conflicts left are both EIn->SOut (movement 1) against NIn->EOut (movement 5) on the surface at tiles 20..22, 2, bits LEFT/RIGHT against UPPER/LOWER, i.e. the E->S branch and the N->E branch cross on the ground at row 2, directly under the W main line, which now stays on row 2 and bridges over them (route moves of movement 2 contain `B3`, a 3-tile bridge heads at x 19 and 23). Every crossing of E->S with N->E needs a bridge of one over the other; the W main line's bridge occupies the bridge level of the same stretch, so the crossing has to find a different place or a longer bridge, and the history costs (one U per round) need many rounds to push them there. The moves (box coordinates, E main line straight):
```
m0 EIn->EOut : F x 43
m1 EIn->SOut : FFFFFFFFFFFFFFFFFFFLFFFLFFFFFFFFFFFF
m2 WIn->WOut : FFFFFFFFFFFFFFFFFFFB3FFFFFFFFFFFFFFFFFFF
m3 WIn->SOut : FFFFFFFFFFFFFFFFFRFFFFFRFFFFFFFFF
m4 NIn->WOut : FFFFFFFFFFRFFFRFFFFFFFFFFFFFFFFF
m5 NIn->EOut : FFFFFFFFFFFLFFFFFLFFFFFFFFFFFFFFFFFFF
```
Last round's routes (round 16, not converged, box coordinates, ASCII of the six routes):
```
- - - - - - - - - - - - - - - - - - - - # - - # - - - - - - - - - - - - - - - - - - - -
. . . . . . . . . . . . . . . . . . . . \ \ / / . . . . . . . . . . . . . . . . . . . .
- - - - - - - - - - - - - - - - - - # H = = = H - # - - - - - - - - - - - - - - - - - -
. . . . . . . . . . . . . . . . . . \ \ / / | . / / . . . . . . . . . . . . . . . . . .
. . . . . . . . . . . . . . . . . . . \ # . | / / . . . . . . . . . . . . . . . . . . .
. . . . . . . . . . . . . . . . . . . . | . # / . . . . . . . . . . . . . . . . . . . .
. . . . . . . . . . . . . . . . . . . . | . | . . . . . . . . . . . . . . . . . . . . .
. . . . . . . . . . . . . . . . . . . . | . | . . . . . . . . . . . . . . . . . . . . .
. . . . . . . . . . . . . . . . . . . . | . | . . . . . . . . . . . . . . . . . . . . .
. . . . . . . . . . . . . . . . . . . . | . | . . . . . . . . . . . . . . . . . . . . .
. . . . . . . . . . . . . . . . . . . . | . | . . . . . . . . . . . . . . . . . . . . .
. . . . . . . . . . . . . . . . . . . . | . | . . . . . . . . . . . . . . . . . . . . .
. . . . . . . . . . . . . . . . . . . . | . | . . . . . . . . . . . . . . . . . . . . .
. . . . . . . . . . . . . . . . . . . . | . | . . . . . . . . . . . . . . . . . . . . .
. . . . . . . . . . . . . . . . . . . . | . | . . . . . . . . . . . . . . . . . . . . .
. . . . . . . . . . . . . . . . . . . . | . | . . . . . . . . . . . . . . . . . . . . .
```
Experiments on the same case, one at a time, nothing kept:

| case at ml_turn_cost 400 | max_rounds 16 | max_rounds 48 | H = 20 (16 rounds) |
|---|---|---|---|
| test spec, p 20 | 2 conflicts left | converges (round 33; counts 6 4 2 ... 2 1 1 1 1 0) | 2 conflicts left |
| pinned spec, p 20 | 2 conflicts left | converges (round 33) | 2 conflicts left |
| TL6 narrow branch (ml_turn_cost 600), p 20 | converges (round 9) | converges (round 9) | converges (round 9) |
| TL6 narrow branch, p 24 (all of p 24-28 stop with 1 conflict at 16 rounds) | 1 conflict left (m1 against m5, tile 25,3: a surface LEFT/RIGHT piece against a piece of resource level 1) | converges (round 32) | converges (round 9) |

So the non-convergence is slowness, not impossibility: the rounds the history costs need (the reading; not tested separately) (one U added per contested resource per round, against a present-conflict price that doubles every round) exceed 16. `max_rounds` 48 converges every iteration-0 negotiation tried; H = 20 helps only the TL6 case. TL6 narrow at p 14-23 is different: iteration 0 converges, iteration 1 (and onwards) stops with "a violation has no feedback"; the violations with no feedback are "a signal block holds both a split and a merge" (rule enum value 8) at 2,3 (violations at p 20: split in a bridge's block x3, waiting room x2, less than one whole-train bay x3, a signal block holds both a split and a merge).
The TL6 narrow case at 48 rounds is clean at p 24-28 (occupied 193-214, slivers 44-61, iterations 4-5) and stops with the same unfed violation at p 14-23. The pinned spec at 48 rounds does converge at iteration 0 but then stops at iteration 1 with the same "a signal block holds both a split and a merge" ("a violation has no feedback"), at every p (violations at p 20: split in a bridge's block x2, waiting room 7 units needs 8, less than one whole-train bay, a signal block holds both a split and a merge at 7,3).

## 3. Supplementary: 48 rounds, test spec, before changes 4 and 5

(For the engineer's pick; `max_rounds` 48 is not in the code, only run for this table.)

| ml_turn_cost | p | clean | why / violations left | iters | occupied | slivers | cost | E off row | W off row |
|---|---|---|---|---|---|---|---|---|---|
| 200 | 14 | no | a violation has no feedback: 6:signals too far apart; 9:missing prio | 4 |  |  |  | 26 | 27 |
| 200 | 15 | no | a violation has no feedback: 6:signals too far apart; 9:missing prio | 4 |  |  |  | 48 | 51 |
| 200 | 16 | no | a violation has no feedback: 6:signals too far apart; 9:missing prio; 9:split inside prio coverage | 6 |  |  |  | 45 | 42 |
| 200 | 17 | yes |  | 4 | 210 | 27 | 31400 | 46 | 49 |
| 200 | 18 | no | a violation has no feedback: 6:signals too far apart; 9:missing prio; 9:split inside prio coverage | 6 |  |  |  | 43 | 42 |
| 200 | 19 | yes |  | 4 | 210 | 19 | 31500 | 44 | 49 |
| 200 | 20 | no | a violation has no feedback: 6:signals too far apart; 9:missing prio; 9:split inside prio coverage | 6 |  |  |  | 41 | 42 |
| 200 | 21 | yes |  | 4 | 210 | 19 | 31800 | 42 | 49 |
| 200 | 22 | no | a violation has no feedback: 11:ML tile in prio coverage does not hold the joiner; 6:signals too far apart; 9:missing prio; 9:split inside prio coverage | 4 |  |  |  | 41 | 45 |
| 200 | 23 | no | still unclean after the feedback iterations: 9:missing prio | 8 |  |  |  | 26 | 26 |
| 200 | 24 | no | still unclean after the feedback iterations: 6:split in a bridge's block: no signal between the split and the bridge; 9:missing prio | 8 |  |  |  | 26 | 48 |
| 200 | 25 | no | a violation has no feedback: 6:signals too far apart; 9:missing prio | 5 |  |  |  | 36 | 0 |
| 200 | 26 | yes |  | 4 | 222 | 49 | 33100 | 0 | 0 |
| 200 | 27 | no | the prio delays the main line | 6 |  |  |  | 0 | 41 |
| 200 | 28 | yes |  | 5 | 232 | 40 | 33300 | 0 | 23 |
| 400 | 14 | no | still unclean after the feedback iterations: 6:split in a bridge's block: no signal between the bridge and the merge; 9:missing prio | 8 |  |  |  | 0 | 0 |
| 400 | 15 | no | still unclean after the feedback iterations: 6:split in a bridge's block: no signal between the bridge and the merge; 9:missing prio | 8 |  |  |  | 0 | 0 |
| 400 | 16 | no | still unclean after the feedback iterations: 6:split in a bridge's block: no signal between the bridge and the merge; 9:missing prio | 8 |  |  |  | 0 | 0 |
| 400 | 17 | no | a violation has no feedback: 6:signals too far apart; 6:split in a bridge's block: no signal between the split and the bridge; 9:missing prio | 3 |  |  |  | 0 | 0 |
| 400 | 18 | no | still unclean after the feedback iterations: 9:missing prio | 8 |  |  |  | 0 | 0 |
| 400 | 19 | no | a violation has no feedback: 6:signals too far apart; 6:split in a bridge's block: no signal between the split and the bridge; 9:missing prio | 3 |  |  |  | 0 | 0 |
| 400 | 20 | no | still unclean after the feedback iterations: 6:split in a bridge's block: no signal between the bridge and the merge; 6:split in a bridge's block: no signal between the split and the bridge; 9:missing prio | 8 |  |  |  | 0 | 0 |
| 400 | 21 | no | a violation has no feedback: 6:signals too far apart; 6:split in a bridge's block: no signal between the split and the bridge; 9:missing prio | 3 |  |  |  | 0 | 0 |
| 400 | 22 | no | still unclean after the feedback iterations: 9:missing prio | 8 |  |  |  | 0 | 0 |
| 400 | 23 | no | a violation has no feedback: 6:signals too far apart; 6:split in a bridge's block: no signal between the split and the bridge; 9:missing prio | 3 |  |  |  | 0 | 0 |
| 400 | 24 | no | still unclean after the feedback iterations: 9:missing prio | 8 |  |  |  | 0 | 0 |
| 400 | 25 | yes |  | 4 | 222 | 40 | 34600 | 0 | 0 |
| 400 | 26 | no | still unclean after the feedback iterations: 9:missing prio | 8 |  |  |  | 0 | 0 |
| 400 | 27 | yes |  | 3 | 224 | 37 | 34350 | 0 | 0 |
| 400 | 28 | no | still unclean after the feedback iterations: 6:split in a bridge's block: no signal between the bridge and the merge; 9:missing prio | 8 |  |  |  | 0 | 0 |
| 800 | 14 | no | still unclean after the feedback iterations: 6:split in a bridge's block: no signal between the bridge and the merge; 9:missing prio | 8 |  |  |  | 0 | 0 |
| 800 | 15 | no | still unclean after the feedback iterations: 6:split in a bridge's block: no signal between the bridge and the merge; 9:missing prio | 8 |  |  |  | 0 | 0 |
| 800 | 16 | no | still unclean after the feedback iterations: 6:split in a bridge's block: no signal between the bridge and the merge; 9:missing prio | 8 |  |  |  | 0 | 0 |
| 800 | 17 | no | a violation has no feedback: 6:signals too far apart; 6:split in a bridge's block: no signal between the split and the bridge; 9:missing prio | 3 |  |  |  | 0 | 0 |
| 800 | 18 | no | still unclean after the feedback iterations: 9:missing prio | 8 |  |  |  | 0 | 0 |
| 800 | 19 | no | a violation has no feedback: 6:signals too far apart; 6:split in a bridge's block: no signal between the split and the bridge; 9:missing prio | 3 |  |  |  | 0 | 0 |
| 800 | 20 | no | still unclean after the feedback iterations: 9:missing prio | 8 |  |  |  | 0 | 0 |
| 800 | 21 | no | a violation has no feedback: 6:signals too far apart; 6:split in a bridge's block: no signal between the split and the bridge; 9:missing prio | 3 |  |  |  | 0 | 0 |
| 800 | 22 | no | still unclean after the feedback iterations: 9:missing prio | 8 |  |  |  | 0 | 0 |
| 800 | 23 | no | a violation has no feedback: 6:signals too far apart; 6:split in a bridge's block: no signal between the split and the bridge; 9:missing prio | 3 |  |  |  | 0 | 0 |
| 800 | 24 | no | still unclean after the feedback iterations: 9:missing prio | 8 |  |  |  | 0 | 0 |
| 800 | 25 | yes |  | 4 | 222 | 40 | 34600 | 0 | 0 |
| 800 | 26 | no | still unclean after the feedback iterations: 9:missing prio | 8 |  |  |  | 0 | 0 |
| 800 | 27 | yes |  | 3 | 224 | 37 | 34350 | 0 | 0 |
| 800 | 28 | no | still unclean after the feedback iterations: 6:split in a bridge's block: no signal between the bridge and the merge; 9:missing prio | 8 |  |  |  | 0 | 0 |


## 4. Supplementary: 48 rounds, ml_turn_cost 400, with changes 4 and 5 (the signal-gap penalty and the curved-merge penalty)

| ml_turn_cost | p | clean | why / violations left | iters | occupied | slivers | cost | E off row | W off row |
|---|---|---|---|---|---|---|---|---|---|
| 400 | 14 | no | still unclean after the feedback iterations: 6:signals too far apart; 6:split in a bridge's block: no signal between the bridge and the merge; 9:missing prio | 8 |  |  |  | 0 | 47 |
| 400 | 15 | no | still unclean after the feedback iterations: 6:split in a bridge's block: no signal between the bridge and the merge; 6:split in a bridge's block: no signal between the split and the bridge; 9:missing prio | 8 |  |  |  | 0 | 0 |
| 400 | 16 | no | still unclean after the feedback iterations: 6:split in a bridge's block: no signal between the bridge and the merge; 9:missing prio | 8 |  |  |  | 0 | 0 |
| 400 | 17 | no | still unclean after the feedback iterations: 9:missing prio | 8 |  |  |  | 0 | 0 |
| 400 | 18 | no | still unclean after the feedback iterations: 6:split in a bridge's block: no signal between the bridge and the merge; 9:missing prio | 8 |  |  |  | 0 | 0 |
| 400 | 19 | no | still unclean after the feedback iterations: 9:missing prio | 8 |  |  |  | 45 | 47 |
| 400 | 20 | no | still unclean after the feedback iterations: 9:missing prio | 8 |  |  |  | 0 | 0 |
| 400 | 21 | no | still unclean after the feedback iterations: 6:signals too far apart; 9:missing prio | 8 |  |  |  | 0 | 0 |
| 400 | 22 | no | still unclean after the feedback iterations: 9:missing prio | 8 |  |  |  | 0 | 0 |
| 400 | 23 | yes |  | 8 | 211 | 46 | 32350 | 0 | 0 |
| 400 | 24 | no | still unclean after the feedback iterations: 9:missing prio | 8 |  |  |  | 0 | 0 |
| 400 | 25 | yes |  | 7 | 218 | 39 | 33700 | 0 | 0 |
| 400 | 26 | yes |  | 4 | 225 | 36 | 34000 | 0 | 0 |
| 400 | 27 | no | still unclean after the feedback iterations: 9:missing prio | 8 |  |  |  | 0 | 0 |
| 400 | 28 | yes |  | 5 | 227 | 36 | 34150 | 0 | 0 |


Best clean: p 23 (211 occupied + 46 slivers = 257, cost 32350), p 25 (218 + 39 = 257, cost 33700); the baseline at 16 rounds is p 22 with 230 + 39 = 269 (old generator 190 + 53 = 243). Both main lines stay on their rows (0 pieces off row) in every case. The unclean cases are "missing prio@2,3" after 8 iterations (feedback counts at p 20: prio_rows 2, coverage 4, bridge 4, gap 0): the W merge near the port on a curve; after the penalty on the merge tile the merge does not move.
