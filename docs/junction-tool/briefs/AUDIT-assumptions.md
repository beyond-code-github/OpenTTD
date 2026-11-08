# Brief AUDIT: every hand-coded assumption in the generator and checker (read-only)

You execute this brief and nothing else. Don't edit any source file. Write exactly one file, `docs/junction-tool/AUDIT-assumptions.md`, and don't commit it (the orchestrator commits it). Keep your context small: read with offset and limit, and grep.

## 1. Why

The engineer: "How come we're having to make so many manual adjustments when the aim is that the math creates the smallest junction that works and doesn't slow trains down?"

The generator searches inside a hand-built skeleton (fixed routing order, windows, fences, lane programs, margins), and the checker has rules of mixed provenance. Each manual correction so far has exposed an assumption of ours that wasn't a real rule. Examples:
- a fixed `sd+1` margin;
- `−1` in the N→W window;
- a straight approach tile before bridge heads (G5);
- a fixed `x_sb = nw_max_x − 6`;
- straight-only prio rows;
- adjacent splits allowed.

We want the full list, so the assumptions can be removed systematically.

## 2. Sources

- **Code:**
  - `src/junction_generator.cpp`: `BuildJunctionCandidate`, `GenerateJunction`, `BuildJunctionLibrary`, `MakeJunctionFrame`;
  - `src/junction_router.cpp`: costs, the curve grammar and its rules;
  - `src/junction_signal_pass.cpp`: all phases;
  - `src/junction_rules.cpp`: every rule and clause.
- **Rules and decisions:**
  - `docs/junction-tool/RULES-coop.md` (sourced coop rules, R-ids);
  - `docs/junction-tool/DECISIONS.md` (D-ids, with the engineer's quotes);
  - `docs/junction-tool/GENERATOR-1b.md`.

## 3. Output: `AUDIT-assumptions.md`

One table per area: generator skeleton, router, signal pass, checker. Each row has:

| # | Location (file:line) | What it fixes or forbids | Provenance | Effect on footprint or throughput | Proposed replacement |
|---|---|---|---|---|---|

- **Provenance** is one of:
  - **GAME**: an OpenTTD mechanic. Cite the game file:line if you know it.
  - **COOP**: a sourced coop rule (an R-id), or an engineer ruling (a D-id with a quote).
  - **OURS**: our own choice or heuristic, a decision we made, or an unexplained constant.
- **Proposed replacement** for OURS rows is one of:
  - "remove; the checker already covers it via <rule>";
  - "replace with a checker rule <X> plus a sweep";
  - "keep: performance only (it prunes the search), with a note";
  - "ask the engineer".
- Be specific about constants: every literal number in the generator's skeleton (windows, offsets, lengths, loop bounds, slack ranges, margins) gets a row unless it's a pure frame value (`JunctionFrame`).

Then:
1. **The objective today:** what `ScoreJunctionLayout` and the selection actually optimise, against "smallest footprint that works and never slows an ML train". List the gaps. For example, slowdowns aren't measured during generation; bridge headway (D103) isn't checked; prio length isn't from the train model yet (M4).
2. **A ranked removal plan:** OURS items ordered by expected footprint gain and risk, grouped into steps of about 3–5 items each.
3. **Questions for the engineer:** the rows marked "ask the engineer", phrased as yes or no where possible, with an ASCII example if it helps.

## 4. Report

- the file path
- counts of GAME, COOP and OURS rows
- the top 10 OURS items by expected footprint gain
- the engineer questions

When you have reported, stop.
