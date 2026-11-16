# Probe 7 snapshot (2026-11-16): LL² hub, area 807, checker-clean

The hub the engineer judged "a great hub". Keep it as the reference for the C++ port.

- `probe7.py`: LL² generator (HiGHS MIP). Reproduce: `python probe7.py --fdiag --routes=routes-ll2.txt --export=out.json`
  (best tag `-1+0F1+0s3e1/2S`, area 807, 64x46). Needs `highspy`.
- `probe6.py`: T2 generator (area 963).
- `checked-ll2g.json`, `checked-t2.json`: the layouts built in the rig (checker-clean; T2 has the known G1 doubling-ambiguity gap).
- `routes-ll2.txt`: route programs for the C++ checker hook (`wip/probe6-checker-hook.patch`).
- `pete-ll2*.json`: dumps of the engineer's rig edits (`dump_rig.py`), the source of the D318 variants.
- `hub807.sav`: the rig save with both hubs (T2 at 6,6; LL² at 6,50; company 2 Orange).
- `asc.py` renders a layout JSON as ASCII; `clear_rig.py` demolishes a layout.
