# Probe 10 (2026-11-16): LLL² attempt, then what the solve time is made of

Run from a folder holding these files beside `planar-probe8.py` and `planar-probe9.py`, copied as `probe8.py` and `probe9.py` (the scripts import them by those names), with a Python that has `highspy`.

- `probe10.py`: LLL² under D325, which is T3 plus a 3x3 choice block on D, C and F. The base model is feasible; no variant converges within the 15 s budget (the clash loop stops at round 6 with about 90 clashes left). Parked: see D326.
- `lll2.topo`: its topology text. `ParsePlanarTopology` accepts it, with 5 crossings per choice block.
- `known.py`: LL² with every clash side and optional-bend choice taken from its own finished solution and given up front.
- `sides.py`, `sides9.py` (logs `sides9.log`): the pairs the clash loop separates on LL² and T3, and the direction it picked.
- `rules9.py`: T3 with experiment flags:
  - `--rule=EF,AB,CD,E`: clash sides from fixed rules (in-port fan order, E-side port order).
  - `--nobin` and `--twopass`: continuous steps.
  - `--tied`: one switch per channel transition.
  - `--on=tag:heading,...`: those switches fixed.
  - `--steps` and `--groups`: list which optional steps the solution uses (`steps9.log`).

Findings: DESIGN-components §8.
