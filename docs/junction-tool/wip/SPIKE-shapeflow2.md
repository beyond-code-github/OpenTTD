# Spike 2: shape flow, tied crossing corners, jogs by cause (findings)

Code: spike-shapeflow2.patch (on top of spike-shapeflow.patch, on 72c03f4196). Raw: /tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/shapeflow2 (one file per variant; all/ = full T2 set; cls.py, p3.py = the cause and part 3 scripts).
Taken: a crossing's corner type A = a dart arriving and the next leaving on opposite sides of their lines' travel (opposite arms the same sign), turn x-4; type B turn -x; x in [1,3], one variable per crossing. To name causes, AddSeg got a spike-only defaulted `__builtin_LINE()` argument recorded per (strand, segment); each heading change is named by the source line of the segment that makes it (a return from a diagonal belongs to the diagonal's construct).

## Part 1: tied corners (LP2)
- The rough satisfies the tying everywhere (0 crossings with unequal x over all corners; no self-crossings, no x outside [1,3]), all 3 runs and all 28 T2 variants; 0 faces fail -8 again.
- LP2 integral every time. T2 o0t2f1s1: objective 4 (LP1 8, rough 12), 8 of 18 crossings change angle. T3 o1t2f1s1: 6 (LP1 12, rough 12), 15 of 45. LL2 o1t3f0s0: 10 (LP1 15, rough 30), 5 of 18. Full T2: objective 4 in all 28 (was 1-2 untied), 4 / 8 / 12 of 18 crossings change (16 / 8 / 4 variants).
- So tying makes LP2 meaningful but still far below the rough: shape freedom at crossings is worth 8 of T2's 12 turns.

## Part 2: jogs by cause (every heading change in a jog stretch is named; no "other")
Per jog, set of causes (jogs: T2 18, T3 34, LL2 18, full T2 490):
- T2: copy step 5; prio fray spread 4; exit step 3; fray leg + join leg + route turn 2; prio join close-up 1; exit step + close-up 1; cand step + copy step 1; cand step 1.
- T3: copy step 11; spread 6; exit step 4; fray leg + join leg + route turn 3; fray leg + (route turn or crossing detour) 3; close-up 2; exit + close-up 2; cand + copy 2; cand 1.
- LL2: spread 6; copy step 5; exit step 3; fork leg diag + path jog (fork-leg crossing jog) 3; join leg + path jog 1.
- Full T2 (490): copy 154, spread 112, exit 76, fray+join leg+route turn 48, close-up 28, exit+close-up 28, cand+copy 14, cand 14, join leg + (route turn or detour) 8, fray leg + (route turn or detour) 8.
Constructs: copy step = doubled bridge's lane stepping to and from its copy line (Double, Bundle, core; D333); spread = prio fray in-line step (Fray, Lt); close-up = prio join out-row step; exit step = PlanarMergeExitStep / gap steps (planar_gaps); cand = PlanarCandidateSteps variant bit (§3 source 5); route turn balanced by fray V-leg and join J-leg diagonals; path jog/detour = BuildPlanarPaths (fork-leg jog toward its side, back-and-forth under a prio channel).
Answer: no jog is a solver free choice, with two exceptions: (1) the candidate step is a variant bit (explicit choice, §3 source 5), (2) the lane-split Fork's two diagonals (Fork, AddSeg opt = true) are free binaries; they take part in 3 of LL2's 18 jogs, each of which also has a path jog. But not all are width changes and crossings: three further sources appear: exit steps (gap rules, D335, source 3), a route turn cancelled by the fray leg and join leg diagonals (stretches whose own turn is 0 only because fan and route turn cancel: 48 of 490 full T2), and the fork-leg path jog. All are fixed by topology and variant before the solve.

## Part 3: flow shapes solved: stopped (no single place to set a stretch's net turn)
- T2 o0t2f1s1: all 8 LP1 differences move one unit of turn along a line across a crossing (D1#1/#3, D2#0/#2, F1#3/#5, F2#2/#4: each line's total unchanged). Where a turn sits relative to a crossing is the crossing's path piece in BuildPlanarPaths ("first piece at or after the last that suits", settled by passes): a rule, not a choice; the only choices are over bits, ties, candidate steps, triangle flips. Nothing sets a stretch's net turn.
- LL2 o1t3f0s0: 31 differing stretches on 26 lines, 23 of those lines change their total turn (e.g. WO1#0 0 -> -1): the line's total is fixed by its channel's route turn plus fixed fan turns, so no path or choice builds it.
- Not solved, area and legality not measured.
