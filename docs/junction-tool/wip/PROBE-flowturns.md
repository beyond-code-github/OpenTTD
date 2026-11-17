# Probe: flow-placed crossing turns (T2): STOPPED, no solves run

Stop condition of the brief hit: the crossing piece rule has no two-sided placement for half of LP1's differences. Instrumented (spike patch + a channel/events dump + per-stretch heading and AddSeg source line, all in probe-flowturns.patch); baseline o0t2f1s1: area 906.25, 89x34, track 794.392, 97 ms.

How the 8 LP1 differences are placed (T2 o0t2f1s1; variant order; channels D = ch9 path h 0 1 2, F = ch11 path h 2 3 4 3 4; crossing X2 = D x F, D over):
- F1#3/#5, F2#2/#4 (4 of 8): the turn is a path piece change (4 -> 3) between F's X1 (piece 2, h4) and X2 (piece 3, h3). Moving it across X2 = X2 on piece 2 instead of 3, which a flag could do. But X2 would then have F at h4 and D at h0: parallel, which the piece rule's `suits` forbids (that is why piece 3 was chosen). Alone it is an illegal crossing.
- D1#1/#3, D2#0/#2 (other 4): the turn 1 -> 0 in stretch #1 is not a crossing piece change. It is the step from the stub segment (AddSeg line 950, h1) onto path piece 0 (line 1075, h0), and piece 0 already spans X2 and X0. "After X2" would mean X2 sits on the stub (h1), which is not a path piece, and h1 is diagonal on D, which is over at X2 (over must be axis). No flag in BuildPlanarPaths can place it.
- So LP1's 8 moves are one coupled shape (D at 45 degrees over F at the same crossing) that the rule excludes twice (over must be axis; crossing must not be parallel). It is a shape the LP relaxes, not one the paths can build.

Answer to "does less net turning give a smaller layout": not measured. Sum |b| 12 -> 8 could not be built.
