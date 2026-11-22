# CARD: curve-aware discovery (Scheme A), amending D422's split

Status: for the engineer's decision. Written 2026-11-23 after the hub runs (CARD-E5-flexstep.md, last bullets) and Gemini's replies (wip/gemini/2026-11-23-hubs-and-d422.md, -whack-a-mole.md).

## Context
D422's pipeline discovers with inserts free and **no curve rows on inserts**, then commits the inserts it used (their curve rows switch on), then prunes. Every correct rule change moves the discovery path, and the commit then lands a unit short on rows it has no move for: PSG 197 at the default price ends with one plain curve row 1 short (L95#5 to L87#0); at box price 1.0 discovery packs 197 into 69x81 and the commit blows it to 177x214; T3 and LL2 end with curve slack 6 and 3 after the commit. The prune guard and slack prices only move where it stops.

## Options
1. **Scheme A, damped (recommended to probe):** from discovery round 2, every insert gets its curve rows as an elastic family priced w(i, k) = W * min(1, m_i / tau), with m_i a damped memory of its length: m_i(k) = max(s_i(k-1), gamma * m_i(k-1)), gamma = 0.5, tau = 1 half tile, W = 1 (the slack price). Inserts stay free (>= 0) throughout, so an insert whose spacing is too dear shrinks to 0 smoothly; the damping stops the 2 -> 0 -> 2 cycle (an insert that just shrank keeps paying for a few rounds). Discovery capped at K = 4 rounds after round 1. Then the commit as today (inserts >= 1 committed, hard-ish curve rows), at most one direction re-pick round.
   - Consequence: discovery sees each step's curve footprint before committing, so it cannot pack past what is curve-legal; the commit should become a small correction, not a repair.
   - Cost: one more elastic family in discovery rounds (a few hundred rows on 197); the rounds stay LPs.
2. **Boundary binaries (fallback, Gemini Q4):** after discovery, fix every insert clearly unused (< 0.25) at 0 and every clearly used one (>= 2, its curve rows satisfied) as committed, and give only the ambiguous ones (expected 10-30) a binary with big-M curve rows in one integer round.
   - Consequence: exact on the contested inserts; earlier whole-integer optional-flex solves were too slow on 197, but those had every insert binary.
3. **Keep D422 as it is:** accept that E5's verdict on 197 at the default price and on T2/T3/LL2 is "not legal"; move on to upstream promise checks.

## Recommendation
Probe option 1 behind a pipeline option (`curve_discovery`, default off), on T1, T2, T3, LL2 and 197 at the default prices, one run each.
- Tripwire: if after the commit the last round still has curve slack > 0 on two or more of the five hubs, or any pipeline exceeds 15 s of LP time, stop and probe option 2.
- Success: T1 stays legal, and 197 and T2 reach slack 0.

## Proof obligations before building
- The damped memory is monotone non-increasing once an insert stays at 0, so every insert's price reaches 0 within log2(W / epsilon) rounds after it leaves; with K capped the scheme terminates by construction. It is a heuristic for the yes/no per insert, not exact; option 2 is the exact fallback on the contested ones.
- Curve rows on a shrunk insert price a bend that may not exist; with the price following the damped length, the overcharge decays geometrically. This is the same argument as IRL1's reweighting for sparsity.

## Prior art checked
D422 (the split and its prices), D419/D423 (the LP rounds are E5's verdict), IRL1 discovery prices (CARD-E5-flexstep.md), Gemini's Scheme B (rejected: vacuous without s <= M alpha, the weak big-M relaxation with it).

Reversible: yes (an option, default off).
