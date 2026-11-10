# O4 reconciliation (draft for the engineer, 2026-11-10)

The engineer: "I feel like we're diverging from the plan a little." Revision 3 was: route all movements together (negotiation), feed rule violations back as prices, add prios after, judge and score. This lists everything added since, what it is, and what it costs, so revision 4 is chosen from evidence. Pending: X1 (one CP-SAT model for the whole T, off-repo) and O4b2d (forced crossings enumerated).

## Time budget
Target: one generation < 100 ms (D151). Measured: one negotiation 53-83 ms (O4b1, before rules). Worst case today: up to 8 feedback iterations x up to 48 rounds x ~2 reroutes x ~5 ms, per branch position (x8), per crossing case (x8 in O4b2d). Every step from here reports per-generation time.

## Mechanisms
| Mechanism | Added | Kind | Keep? | Time cost |
|---|---|---|---|---|
| Line router per movement (A*, heights, bridges, tunnels) | O1-O2, O4a | core | yes | ~1-8 ms per single-track route |
| Negotiation rounds (present + history prices) | O4b1 | core (packing) | yes | rounds x reroutes |
| Switch rule (split: same In port, same entry edge; merge: same Out port, same exit edge) | O4b1 amendment 6 | game rule | yes | none |
| Exclusive bridge heads/portals; no head on a port tile | O4b1 amendment 1-2 | game rule | yes | none |
| Box boundary holds only ports | O4b2 amendment 1 | checker rule | yes (or move into the frame) | none |
| Old finishing pipeline (crop, row extension, one-tile branch tail D115, normalise, signal pass) | O4b2 | reuse | yes | small |
| Feedback: prio row reserved | O4b2 | rule as price after the fact | replace: prio rows as router constraint or token | a full renegotiation per iteration |
| Feedback: coverage window switch penalty (D108) | O4b2 | rule as price after the fact | replace: switch illegal in coverage during routing | same |
| Feedback: split before bridge (D113) | O4b2 | rule as price after the fact | replace: router state | same |
| Feedback: waiting room / bay, move split upstream | O4b2 | rule as price after the fact | replace: router state (private length since split) | same |
| Feedback: signals too far apart | O4b2b | rule as price after the fact | replace or keep as rare fallback | same |
| Feedback: missing prio on a curved merge | O4b2b | rule as price after the fact | likely gone with straight main lines | same |
| Feedback: split and merge in one block (rule 8) | O4b2c | rule as price after the fact | replace: router state | same |
| Main-line turn cost (D165) | O4b2b | engineer ruling | yes | slows convergence |
| Hug preference (D166 a) | O4b2c | compactness tie-break | yes (measure) | slight |
| Clean-up pass (D166 b) | O4b2c | needless bridges | yes, once its bug is fixed | ~6 routes |
| Forced crossings enumerated (D167) | O4b2d | discrete choices by branch and bound | yes, lazily (branch only on stuck crossings); per-structure decisions for multi-lane (engineer: lanes are separate movements) | x cases |
| Branch position p swept (8 values) | O4b2 | search | replace with a smarter search or bound | x8 |

## Direction proposed for revision 4
1. Rules become router constraints, so the first negotiation is rule-clean and the feedback loop disappears (engineer: "I thought we were just adding constraints to the simultaneous solver?").
2. Forced crossings branched lazily with a bound; main-line lift first (D156).
3. One short rounds loop; hug and clean-up kept.
4. p searched by bound, not swept.
5. Or, if X1 shows a constraint model solving the T in well under 100 ms, the solver replaces negotiation as the core and the rules are added to it as constraints.

## Open
- Multi-lane (LLLL_RRRR): movements are lane -> destination (engineer), so forced crossings grow fast; LL_RR is the first test before building more.
- D80 doubled-bridge move (main-line lift fully rule-clean).
