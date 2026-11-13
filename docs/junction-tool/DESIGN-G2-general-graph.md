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
- **Splits before merges (D24):** along every path, every split of its cascade comes before any merge. With pairing any, a main lane's exit cascade splits fully, and only then do legs from different main lanes merge into each branch lane. The graph test asserts D24 on every path.

## G2.2 Only the first node of a cascade sits on the main row; the rest sit in the band (D224 both sides)
- **Exit side:** one leg leaves the main lane, then splits in the band into the branch lanes it serves.
- **Join side:** unchanged (D224). Branch legs merge in the band, then one leg joins the main lane.

So each main row carries one in, one leg and one out for a T at any lane count and any pairing. The structure's row rule stays as it is, and is now true by construction instead of a refusal.

## G2.3 Band edges follow the same bridge rule as every other edge
- Consecutive crossings of one edge may share a bridge. No rule caps how many tracks one bridge crosses (D228 replaces the provisional cap of 2 in D209).
- What bounds a bridge is the game's maximum bridge length and the heights. Whether a long crossing set gets one bridge or several in series, with track and signals between, is a cost choice (D209, "cost decides").
- Over/under at each crossing follows D223/D227.
- This removes placement's "one bridge per band edge" refusal.

## G2.4 A doubled bridge on a row's out edge (D229)
- Where a joiner merges into a main lane before a crossing of that lane (D229, merge then bridge), the lane's out edge (joiner merge to the main out port) carries its doubled bridge.
- One rule covers the leg and the out edge: the two copies run from a start item A to an end item B of the row (A != B, each the Lane or the Copy item). Copy 1 (`programs.copies[e]`) S-bends from A to B before the bridge (`LaneSplit`); copy 0 (`programs.edges[e]`) stays on A over the bridge and S-bends to B after it (`LaneMerge`). Each copy has exactly one S-bend, so both have equal pathfinder cost (D100, D105); copy 1's heads sit one tile upstream of copy 0's (D118, D145).
- Leg: A is the item the exit split sits on (`north` when the leg is doubled), B is Lane (today, unchanged).
- Out edge: A is Lane (the joiner merge sits on the lane), B is the row's Copy item; the out edge's Fan runs from the Copy item to the port. Programs: `edges[e]` = `[Over..., SBend(copy, LaneMerge), Fan(copy)]`, `copies[e]` = `[SBend(copy, LaneSplit), Over...]`.
- A row's one Copy item (Copy or PrioAndCopy, either side of Lane) serves every doubled edge of the row; the bridges lie at different x.
- Run floors: joiner merge to lane split, and lane merge to fan, are the chain run (2, D217).
- In edge (the mirror, D229 "in or out edge"; RECON-crossed-in-edge): A is the row's Copy item, where the in edge's Fan arrives, and B is the item its node sits on (`north` when the leg is doubled, else Lane). Programs: `edges[e]` = `[Fan(copy), Over..., SBend(B, LaneMerge)]`, `copies[e]` = `[SBend(B, LaneSplit), Over...]`. The node's item stays B. An in edge's doubled bridge does not make the leg doubled.

## G2.5 Rows and bands have sizes; fans are their tapers at the ports (engineer, 2026-11-13: "maybe rows need to grow or shrink and then they push everything else up with them. So instead of a fan being a change of row, it's a change in size of a row?")
- Every row (W/E) and every S port group's column band has an inside size. At its port the size is the notation spacing (D38); inside the box it may grow to hold what the row needs (prio rows, doubling copies, joiner approaches, crossings: D203(1)). A fan is the taper between the two.
- A row that grows pushes every row beyond it outward; the stack and band relations are "at least" chains, so the solve already behaves this way.
- D230's halves rule is how an S band grows about its port block: its west lanes move west, its east lanes east. A band need not grow evenly, so fan sizes stay per lane; the band names and bounds them. The fan objective (REVIEW-port-fans S4) is: keep each band's growth minimal.
- A band that moves without changing size is a bundle shift (all lanes of one direction S-bending together at one place, as doubled copies already do). Not built; for whole-line moves around terrain and obstacles (D216) when the search needs them.

## G2.6 An S-bend token names the node it serves
- A band edge's S-bend tokens carry what they serve: Diverge (the split at the edge's start: it starts on the split's point), Converge (the merge at the edge's end, including a joiner's arrival on its lane: it ends on the merge's point), or None (a crossing's S-bend, placed by its bridge). Placement pins an S-bend to a node only by its role, never by its token position (RECON-s2-cycle: a lone S-bend that is both first and last token was pinned to both nodes, making its edge rigid).

## What this does not cover (named, later)
- **Four-way hubs:** a main lane with two branch groups (left and right) needs two legs on one row. That is the next generalisation, rows with several legs, and it is not needed for T hubs at any lane count.
- **Tunnels as a cost choice for the diving leg** (D227 toggle candidate).

## Steps (each test-first; hashes of today's T and LL_RR are unchanged, because their cascades already have depth 1)
1. **G2a:** the cascade rule in the graph (F4). The red test is LLL_RRR × L_R building a graph with 2-way nodes only.
2. **G2b:** exit-side D224 (F2). The red test is LL_RR × LL_RR (any) passing structure.
3. **G2c:** band-edge bridges (F3). The red test is the CX1 dump's e5 placed with one bridge over e40 and e43.
4. **CX1:** crossing over/under per D223/D227, after G2c.
5. Then LL_RR × LL_RR in game.
