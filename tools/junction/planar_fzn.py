#!/usr/bin/env python3
"""Write an exported drawing program (PlanarDrawOptions::export_path) as FlatZinc, for offline CP solver benchmarks (D450).

Every variable must be integer (export with nothing continuous); real coefficients and bounds are scaled to integers per
row, as tools/junction/planar_cpsat.py does. Usage: planar_fzn.py EXPORT.json OUT.fzn [--objective]
"""
import json
import math
import sys
from fractions import Fraction

BIG = 1 << 30


def frac(x):
	return Fraction(x).limit_denominator(1 << 20)


def main():
	src, dst = sys.argv[1], sys.argv[2]
	use_objective = '--objective' in sys.argv
	with open(src) as f:
		program = json.load(f)
	out = []
	for i, v in enumerate(program['variables']):
		lb = v['lb'] if v['lb'] is not None and math.isfinite(v['lb']) else -BIG
		ub = v['ub'] if v['ub'] is not None and math.isfinite(v['ub']) else BIG
		out.append(f'var {math.ceil(lb - 1e-9)}..{math.floor(ub + 1e-9)}: x{i};')
	for r in program['rows']:
		coefs = [frac(c) for _, c in r['terms']]
		bounds = [frac(b) for b in (r['lb'], r['ub']) if b is not None and math.isfinite(b)]
		scale = 1
		for q in coefs + bounds:
			scale = scale * q.denominator // math.gcd(scale, q.denominator)
		cs = [int(c * scale) for c in coefs]
		xs = [f'x{j}' for j, _ in r['terms']]
		if not xs:
			continue
		lb = math.ceil(frac(r['lb']) * scale) if r['lb'] is not None and math.isfinite(r['lb']) else None
		ub = math.floor(frac(r['ub']) * scale) if r['ub'] is not None and math.isfinite(r['ub']) else None
		cl, xl = ','.join(map(str, cs)), ','.join(xs)
		if lb is not None and ub is not None and lb == ub:
			out.append(f'constraint int_lin_eq([{cl}],[{xl}],{lb});')
			continue
		if ub is not None:
			out.append(f'constraint int_lin_le([{cl}],[{xl}],{ub});')
		if lb is not None:
			out.append(f'constraint int_lin_le([{",".join(str(-c) for c in cs)}],[{xl}],{-lb});')
	if use_objective:
		terms = [(int(round(v['cost'] * 1000)), i) for i, v in enumerate(program['variables']) if v['cost']]
		out.append('var int: obj :: output_var;')
		out.append(f'constraint int_lin_eq([{",".join(str(c) for c, _ in terms)},-1],[{",".join(f"x{i}" for _, i in terms)},obj],0);')
		out.append('solve minimize obj;')
	else:
		out.append('solve satisfy;')
	with open(dst, 'w') as f:
		f.write('\n'.join(out) + '\n')


if __name__ == '__main__':
	main()
