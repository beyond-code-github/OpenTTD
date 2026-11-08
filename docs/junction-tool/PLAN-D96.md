# D96: near-real-time hover (plan)

**Goal:** the engineer said, "I'm envisaging you move it around and it updates in near-real time". The ghost should follow the cursor with a layout that fits the ground under it. The target is under 20 ms per hover step for the instant tier in Release, and at most D62's 200 ms.

## Where the time goes (P1, RelWithDebInfo)

- **One `GenerateJunction`** on the test spec: 1.1 s, running about 72 `BuildJunctionCandidate` calls (48 of which fail at heights 6–11).
- **The router is 91–95% of that time.** About 52% is the `best` map with a 9-tuple key. `Successors` is 21% (`TileFree` 7%, set lookups 6%), and the heap is 9%.
- **The signal pass, checker and scoring together are under 3%.**
- **A nearest-fit search with no fit:** 34 s.

## Steps

| Step | What | Expected gain |
|---|---|---|
| P1 | Profile (done) | n/a |
| P2 | Router lookups: packed keys, hash containers, no copy per pop. Same routes (golden JSON). | about 2× on everything |
| P3 | Router inner loop: dense grids over the request bounds for tracks, heads, crossable and goals; integer costs in the queue; parent-pointer moves instead of a string per node. Same routes. | 15–25% more |
| P4 | Fewer searches per generation: a cheap pre-check that rejects heights the zone can't fit (h6–h11 today) before routing; run the sweep's candidates on a thread pool and pick the result exactly as the serial sweep does (deterministic). | about 3× (pre-check), × cores |
| P5 | The instant tier: `JunctionCandidateLibrary`. When the spec changes, build and keep every rule-clean flat candidate. `FitJunctionCandidate(library, terrain)` tests each against the snapshot: every tile usable on one plane, plus foundation and clear cost, then the cheapest fit. This involves no routing, only tile lookups. | per hover in ms |
| P6 | GUI two tiers: every hover step runs the fit test on the main thread (or a fast worker) and draws at once. When the cursor rests, the full terrain generation runs as the refine tier, replacing the ghost if it scores better. The nearest-fit search uses the fit test first, and full generation only as the refine. | what the engineer feels |
| P7 | A fast reject for hopeless spots: if no plane has enough usable tiles in the footprint bounds, skip the refine tier. | S4 34 s down to near zero |

**Order:** P2, then P3 and P5 (P5 doesn't depend on P3), then P6, P4 and P7. Measure after each step with S1–S4 in `build-prof`.

## Invariants

- Optimisation steps (P2–P4) must not change any generated layout. Check this with the `[.golden]` test (added in P2).
- Determinism holds: the same spec and terrain give the same layout, whatever the thread timing.
- Routine gates use the Release suite (about 22 s). The Debug suite and `[.slow]` run at milestones.
