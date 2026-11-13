# DESIGN-G2: the graph generalised so any lane counts build (draft for the engineer)

Source: REVIEW-opus-since-move F2, F3 and F4. The rules below replace three special limits with one general rule each. Ids: D201 (graph built by rule), D221 (pairing), D224 (branch legs merge before joining), D227 (crossing defaults).

## The three limits today
- **F4: a split has at most two legs.** A lane that must reach 3 lanes is refused, so LLL_RRR × L_R fails at the graph.
- **F2: D224 is join-side only.** On the exit side, a main lane bound for two branch lanes gets two split nodes at its slot. Its row then has two legs, and structure refuses it. This is what stops LL_RR × LL_RR (any).
- **F3: a band edge carries one bridge, over one crossing.** A joiner that crosses two exit legs in the band is refused.

## G2.1 Every split and merge is a cascade of two-way nodes
A real switch is two-way, so a lane bound for k destinations splits as a binary cascade.
- **Level 1, by destination group:** through traffic versus each branch, i.e. a port side.
- **Level 2, by lane within the group.**
- **Within a level,** the ordered key list (lane order across the line) splits into two contiguous halves, recursively; for 3 keys that is 1 + 2. Contiguous halves keep legs in lane order, so a cascade never creates a crossing of its own.
- **Merges are the mirror image:** lanes merge pairwise in lane order, by lane within a group first and then by group.

## G2.2 Only the first node of a cascade sits on the main row; the rest sit in the band (D224 both sides)
- **Exit side:** one leg leaves the main lane, then splits in the band into the branch lanes it serves.
- **Join side:** unchanged (D224). Branch legs merge in the band, then one leg joins the main lane.

So each main row carries one in, one leg and one out for a T at any lane count and any pairing. The structure's row rule stays as it is, and is now true by construction instead of a refusal.

## G2.3 Band edges follow the same bridge rule as every other edge
- Consecutive crossings of one band edge share a bridge up to the series cap (2 crossers, `JUNCTION_BRIDGE_MAX_CROSSERS`) and within the span limit. Beyond that, the next crossing starts a new bridge.
- Over/under at each crossing follows D223/D227.
- This removes placement's "one bridge per band edge" refusal.

## What this does not cover (named, later)
- **Four-way hubs:** a main lane with two branch groups (left and right) needs two legs on one row. That is the next generalisation, rows with several legs, and it is not needed for T hubs at any lane count.
- **Tunnels as a cost choice for the diving leg** (D227 toggle candidate).

## Steps (each test-first; hashes of today's T and LL_RR are unchanged, because their cascades already have depth 1)
1. **G2a:** the cascade rule in the graph (F4). The red test is LLL_RRR × L_R building a graph with 2-way nodes only.
2. **G2b:** exit-side D224 (F2). The red test is LL_RR × LL_RR (any) passing structure.
3. **G2c:** band-edge bridges (F3). The red test is the CX1 dump's e5 placed with one bridge over e40 and e43.
4. **CX1:** crossing over/under per D223/D227, after G2c.
5. Then LL_RR × LL_RR in game.
