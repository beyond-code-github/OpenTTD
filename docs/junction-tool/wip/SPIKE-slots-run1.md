# Spike: slot layer on T2 (stopped: model infeasible)

Result: the model as the brief states it is infeasible for o1t3f1s0, o1t2f1s0 and o0t2f1s1 (same IIS each). No comparison tables, no forced/optional list.

IIS (T2, channels as numbered by BuildPlanarChannels; 7 = B1,B2, 10 = E1,E2, 1 = CB, 2 = ED):
port-out CB1/CB2, port-out ED1/ED2, straight B1->CB1, B2->CB2, E1->ED1, E2->ED2, order ch7 post 0 and 1, order ch10 post 1 and 2, port-in EF1/EF2, straight EF1->E1, EF2->E2.

Reading: sites 4 and 5 are prio Joins with straight channels B and E. Each lane of B and E gets a D349 row in its `post` interval, so consecutive lanes are at least 2 slots apart there (row, lane, row, lane). The straight continuation pins s(CB) = s(B at post) and s(ED) = s(E at post) with step 0, but the port-out pins put CB1/CB2 and ED1/ED2 exactly 1 apart. Rows make the lanes 2 apart and the ports make them 1 apart, with no step allowed between: infeasible. In the built layout the lanes close up after the join (the exit step), which the brief's model has no variable for (the step is only an objective term across a link that is an equality).

Rules read from code:
- Copy side: planar_components.cpp:1284-1288 (far_left = join's side > 0 when the join's straight channel is c and prio, else true) and :1351-1356 (away, is_far, sg = is_far ? -away : away). Copy direction sg: +1 is right of travel (Turn(h, sg)).
- `join` for far_left is site_of(c, false) (planar_components.cpp:1187-1192): the first non-Fray site where c is straight or other. The brief's "far" rule is used for every over channel; the composer's turning-channel rule (left lane copy left, right lane copy right) differs and was not used (no full-share turning over channel on T2).
- Prio row side: planar_components.cpp:1020 (`prio_rows.push_back({S[k], ms[k], h, -sigma})`, sigma the joiner side = resolved site side).
- Variant tag fields: o = BuildPlanarPaths choice (planar_components.cpp:2995), t = ResolvePlanarSites ties (:1171), f = FlipPlanarTriangles flips (:2995); s (steps) is not part of the brief's model and is ignored.
- Interval cuts, port pins, straight links: the brief; straight links read from PlanarNodeLegs::straight of the site's nodes, from = split ? node in : legs.straight, to = split ? legs.straight : node out.

Open questions:
- Should the straight link allow a step where a prio row sits in the last interval (rows widen the lanes before the join, the out line closes up after it)? As stated the model cannot predict the exit step.
- The Why docs (REVIEW-sideways-modelling.md, DESIGN-tracks.md) exist in /home/pete/OpenTTD/docs/junction-tool but not in this worktree, and I did not read them; `grep D312|D333|D349` finds no rows in this worktree's DECISIONS.md. The rules above come from the code and the brief only.
