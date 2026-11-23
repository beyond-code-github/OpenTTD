#!/usr/bin/env python3
"""Offline feasibility check of an exported drawing program (PlanarDrawOptions::export_path) with CP-SAT.

A measurement aid only (CP-SAT is Apache-2.0, never linked into the game). Every variable must be
integer (export with the grid off and nothing continuous); real coefficients and bounds are scaled
to integers per row. Usage: planar_cpsat.py EXPORT.json [seconds] [workers] [--objective]
Prints the status, the wall time and, when infeasible, nothing more (CP-SAT gives no IIS here).
"""
import json
import math
import sys
from fractions import Fraction

from ortools.sat.python import cp_model


def as_fraction(x):
	return Fraction(x).limit_denominator(1 << 20)


def main():
	path = sys.argv[1]
	seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 60.0
	workers = int(sys.argv[3]) if len(sys.argv) > 3 else 4
	use_objective = '--objective' in sys.argv
	with open(path) as f:
		program = json.load(f)
	model = cp_model.CpModel()
	BIG = 1 << 30
	xs = []
	continuous = 0
	for i, v in enumerate(program['variables']):
		if not v['integer']:
			continuous += 1
		lb = v['lb'] if v['lb'] is not None and math.isfinite(v['lb']) else -BIG
		ub = v['ub'] if v['ub'] is not None and math.isfinite(v['ub']) else BIG
		xs.append(model.NewIntVar(math.ceil(lb - 1e-9), math.floor(ub + 1e-9), f'v{i}'))
	if continuous:
		print(f'warning: {continuous} continuous variables treated as integer')
	for r in program['rows']:
		coefs = [as_fraction(c) for _, c in r['terms']]
		bounds = [as_fraction(b) for b in (r['lb'], r['ub']) if b is not None and math.isfinite(b)]
		scale = 1
		for q in coefs + bounds:
			scale = scale * q.denominator // math.gcd(scale, q.denominator)
		expr = sum(int(c * scale) * xs[j] for (j, _), c in zip(r['terms'], coefs))
		lb, ub = r['lb'], r['ub']
		if lb is not None and math.isfinite(lb):
			model.Add(expr >= math.ceil(as_fraction(lb) * scale))
		if ub is not None and math.isfinite(ub):
			model.Add(expr <= math.floor(as_fraction(ub) * scale))
	if use_objective:
		model.Minimize(sum(int(round(v['cost'] * 1000)) * xs[i] for i, v in enumerate(program['variables']) if v['cost']))
	solver = cp_model.CpSolver()
	solver.parameters.max_time_in_seconds = seconds
	solver.parameters.num_workers = workers
	status = solver.Solve(model)
	print(f'variables {len(xs)} rows {len(program["rows"])} status {solver.StatusName(status)} wall {solver.WallTime():.2f} s')


if __name__ == '__main__':
	main()
