"""G3P: offline prototype of the joint placement solve (DESIGN-G3 §G3.3, §G3.5 step 1).

Reads the placement systems dumped by the hidden test "[.g3dump]" (<name>.json: the last X and the last Y
system of one placement call, every relation with its reason and m tag, the bends and the chains), builds one
ILP over both axes with one length L per bend, solves it and its LP relaxation with HiGHS, and prints the result.

Usage: python solve.py [dump_dir [out_dir [names...]]]
Writes <name>.result.json and, for a feasible model, <name>.choices (the input of the hidden test "[.g3apply]") to out_dir.
"""

import json
import os
import sys
import time

import highspy
import numpy as np

VX = [0, 1, 0, -1]
VY = [-1, 0, 1, 0]
INF = highspy.kHighsInf
BIG = 100000  # big-M of the S fans' zero-size branch (coordinates stay far below it)
EPS = 1e-3    # tie break: EPS * sum L (the costs are multiples of 50, the sum of L stays below 1000)
NAMES = ["T", "LLRR_x_LR", "LR_x_LRR", "LR_x_LLRR", "LLRR_x_LLRR"]


class Model:
    def __init__(self):
        self.lo, self.hi, self.integer, self.names = [], [], [], []
        self.rows = []  # (lo, hi, {col: coef}, label)
        self.cost = {}

    def var(self, name, lo=0.0, hi=INF, integer=True):
        self.lo.append(lo)
        self.hi.append(hi)
        self.integer.append(integer)
        self.names.append(name)
        return len(self.lo) - 1

    def row(self, coefs, lo, hi, label):
        clean = {}
        for c, a in coefs.items():
            if a != 0:
                clean[c] = clean.get(c, 0) + a
        clean = {c: a for c, a in clean.items() if a != 0}
        self.rows.append((lo, hi, clean, label))

    def add_cost(self, expr, weight):
        for c, a in expr.items():
            self.cost[c] = self.cost.get(c, 0.0) + weight * a

    def highs(self, integral):
        h = highspy.Highs()
        h.setOptionValue("output_flag", False)
        h.setOptionValue("iis_strategy", 2)
        n = len(self.lo)
        cost = np.array([self.cost.get(i, 0.0) for i in range(n)])
        h.addVars(n, np.array(self.lo, dtype=float), np.array(self.hi, dtype=float))
        h.changeColsCost(n, np.arange(n, dtype=np.int32), cost)
        for lo, hi, coefs, _ in self.rows:
            idx = np.array(list(coefs.keys()), dtype=np.int32)
            val = np.array(list(coefs.values()), dtype=float)
            h.addRow(lo, hi, len(idx), idx, val)
        if integral:
            h.changeColsIntegrality(n, np.arange(n, dtype=np.int32), np.array([highspy.HighsVarType.kInteger if i else highspy.HighsVarType.kContinuous for i in self.integer]))
        return h


def side_of(b):
    return (b["hin"] + b["hand"] + 4) % 4


def build(d):
    X, Y = d["X"], d["Y"]
    ref = X if X is not None and X["rows_known"] else Y
    anyc = X if X is not None else Y
    model = Model()
    npts = anyc["pt_count"]
    x = [model.var(f"x{p}") for p in range(npts)]
    y = [model.var(f"y{p}") for p in range(npts)]
    coord = [x, y]
    model.row({x[0]: 1}, 0, 0, "origin x")
    model.row({y[0]: 1}, 0, 0, "origin y")
    k = anyc["k"]
    bends = ref["bends"]
    L, Z = {}, {}
    notes = []
    for b in bends:
        src = b["source"]
        if src == "free":
            if b["kind"] == "SFan":
                L[b["id"]] = model.var(f"L{b['id']}", 0)
                Z[b["id"]] = model.var(f"z{b['id']}", 0, 1)
                model.row({L[b["id"]]: 1, Z[b["id"]]: -1}, 0, INF, f"fan {b['id']} L >= z")
                model.row({L[b["id"]]: 1, Z[b["id"]]: -BIG}, -INF, 0, f"fan {b['id']} L <= M z")
            else:
                L[b["id"]] = model.var(f"L{b['id']}", b["floor"])
        elif src == "corner_rows":
            L[b["id"]] = model.var(f"L{b['id']}", k)
        elif src == "rows":
            L[b["id"]] = model.var(f"L{b['id']}", 0)
        else:  # port_rows, fixed_k, one_row: held at the dumped size
            L[b["id"]] = model.var(f"L{b['id']}", b["m"], b["m"])
    bend_by_id = {b["id"]: b for b in bends}

    # The dumped relations: coord(v) - coord(u) - a L >= b (tagged) or >= c.
    tagged_y = set()
    counts = {"relations": 0, "tagged_linear": 0, "nonlinear_held": 0, "zero_branch": 0}
    nl_sites = {}
    for cap in (X, Y):
        if cap is None:
            continue
        axis = cap["axis"]
        for r in cap["relations"]:
            counts["relations"] += 1
            coefs = {coord[axis][r["v"]]: 1}
            coefs[coord[axis][r["u"]]] = coefs.get(coord[axis][r["u"]], 0) - 1  # v == u: a constant row
            label = ("X" if axis == 0 else "Y", r)
            bid = r["bend"]
            nl = r["nl"]
            if nl is not None and nl.startswith("zero-size") and bid >= 0 and bid in Z:
                counts["zero_branch"] += 1
                coefs[Z[bid]] = BIG  # only when the fan is empty
                model.row(coefs, r["c"], INF, label)
            elif nl is None and bid >= 0:
                counts["tagged_linear"] += 1
                coefs[L[bid]] = coefs.get(L[bid], 0) - r["a"]
                lo = r["b"]
                if bid in Z:  # an S fan's sized branch: only when the fan is not empty
                    coefs[Z[bid]] = -BIG
                    lo = r["b"] - BIG
                if axis == 1:
                    tagged_y.add(bid)
                model.row(coefs, lo, INF, label)
            else:
                if nl is not None and not nl.startswith("zero-size") and not nl.startswith("order"):
                    counts["nonlinear_held"] += 1
                    key = nl.split(":")[0]
                    nl_sites[key] = nl_sites.get(key, 0) + 1
                model.row(coefs, r["c"], INF, label)

    # Link rows per diagonal.
    links = 0
    for b in bends:
        i = b["id"]
        if b["source"] == "free":
            s = VX[side_of(b)]   # junction_placement.cpp: side = (hin + hand + 4) % 4, x sign VX[side] (S-bend size)
            sv = VY[b["hin"]]    # y sign VY[hin], rows m - 1 (S-bend size)
            model.row({x[b["last"]]: 1, x[b["first"]]: -1, L[i]: -s}, 0, 0, ("link", f"x bend {i}"))
            links += 1
            if i not in tagged_y:
                if i in Z:
                    model.row({y[b["last"]]: 1, y[b["first"]]: -1, L[i]: -sv, Z[i]: sv}, 0, 0, ("link", f"y bend {i}"))
                else:
                    model.row({y[b["last"]]: 1, y[b["first"]]: -1, L[i]: -sv}, -sv, -sv, ("link", f"y bend {i}"))
                links += 1
        elif b["source"] == "corner_rows":
            # The joiner Corner's span is y(first) - y(last) (refresh and the X size: |vy first - vy last|, floor ge(first, last, k)).
            model.row({y[b["first"]]: 1, y[b["last"]]: -1, L[i]: -1}, 0, 0, ("link", f"y corner {i}"))
            links += 1
        elif b["source"] == "rows":
            # A horizontal S-bend's size is |y(item) - y(first)| (refresh); its sign is the side's VY.
            sv = VY[side_of(b)]
            model.row({y[b["item_pt"]]: 1, y[b["first"]]: -1, L[i]: -sv}, 0, 0, ("link", f"y rows bend {i}"))
            links += 1

    # Objective: the flat build cost.
    prices = anyc["prices"]
    rail, signal, sd = prices["rail_piece"], prices["signal"], prices["sd"]
    bridge_a, bridge_b = prices["bridge1"] - prices["bridge0"], prices["bridge0"]
    left_out = []
    chain_len = {}  # (edge, copy) -> linear expression of its track pieces (signals)
    for ch in anyc["chains"]:
        elems = ch["elems"]
        expr = {}
        for j, el in enumerate(elems):
            if el["bend"] >= 0:
                bb = bend_by_id[el["bend"]]
                pieces = {L[el["bend"]]: 2}  # 2 L half pieces (2 L + 1 for a Corner: the constant is dropped)
                for c, a in pieces.items():
                    expr[c] = expr.get(c, 0) + a
            if el["k"] in ("Heads", "HeadsPair") and el["first"] != el["last"]:
                axis = 0 if VX[el["hin"]] != 0 else 1
                if el["k"] == "HeadsPair":
                    axis = 0
                t = model.var(f"span{ch['edge']}.{ch['copy']}.{j}", 0, INF, False)
                d = {coord[axis][el["last"]]: 1, coord[axis][el["first"]]: -1}
                model.row({t: 1, **{c: -a for c, a in d.items()}}, 0, INF, ("cost", "span"))
                model.row({t: 1, **d}, 0, INF, ("cost", "span"))
                if ch["tunnel"]:
                    left_out.append(f"tunnel price of edge {ch['edge']} (unset in the spec)")
                else:
                    model.add_cost({t: 1}, bridge_a)  # middle tiles = span - 1; the constant is dropped
            if j + 1 >= len(elems):
                continue
            nx = elems[j + 1]
            if nx["k"] == "Prio" or el["k"] == "Prio" or nx["first"] == el["last"]:
                continue
            axis = 0 if VX[el["hout"]] != 0 else 1
            t = model.var(f"run{ch['edge']}.{ch['copy']}.{j}", 0, INF, False)
            d = {coord[axis][nx["first"]]: 1, coord[axis][el["last"]]: -1}
            model.row({t: 1, **{c: -a for c, a in d.items()}}, 0, INF, ("cost", "run"))
            model.row({t: 1, **d}, 0, INF, ("cost", "run"))
            expr[t] = expr.get(t, 0) + 1
        chain_len[(ch["edge"], ch["copy"])] = expr
        model.add_cost(expr, rail)
    doubled = {ch["edge"] for ch in anyc["chains"] if ch["copy"] == 1}
    nsig = 0
    for p, path in enumerate(anyc["paths"]):
        copies = 2 if any(e in doubled for e in path["edges"]) else 1
        for c in range(copies):
            length = {}
            for e in path["edges"]:
                for col, a in chain_len.get((e, c if e in doubled else 0), {}).items():
                    length[col] = length.get(col, 0) + a
            n = model.var(f"signals{p}.{c}", 0)
            # n = floor(len / sd): sd n >= len - (sd - 1), minimised.
            model.row({n: sd, **{col: -a for col, a in length.items()}}, -(sd - 1), INF, ("cost", "signals"))
            model.add_cost({n: 1}, signal)
            nsig += 1
    model.add_cost({L[i]: 1 for i in L}, EPS)
    info = {"points": 2 * npts, "bends": len(L), "fan_binaries": len(Z), "links": links, "signal_counters": nsig, "counts": counts, "nonlinear_sites": nl_sites, "left_out": sorted(set(left_out))}
    return model, x, y, L, Z, info


def chain_text(model, iis_rows):
    parts = []
    for ri in iis_rows:
        label = model.rows[ri][3]
        if isinstance(label, tuple) and isinstance(label[1], dict):
            r = label[1]
            where = ", ".join(f"{n} {r[n]}" for n in ("edge", "node", "row") if r[n] >= 0)
            tag = f" [m: bend {r['bend']}, a {r['a']}, b {r['b']}]" if r["bend"] >= 0 and r["nl"] is None else ""
            parts.append(f"{label[0]} {r['rule']}{' (' + where + ')' if where else ''}: p{r['v']} - p{r['u']} >= {r['c']}{tag}")
        else:
            parts.append(f"{label[0]} {label[1]}")
    return " <- ".join(parts)


def solve(d):
    model, x, y, L, Z, info = build(d)
    out = {"name": d["name"], "seed_places": d["seed_places"], "error": d["error"], **info, "vars": len(model.lo), "rows": len(model.rows)}
    for integral in (True, False):
        h = model.highs(integral)
        t0 = time.perf_counter()
        h.run()
        dt = time.perf_counter() - t0
        status = h.modelStatusToString(h.getModelStatus())
        key = "ilp" if integral else "lp"
        res = {"status": status, "time_ms": round(dt * 1000, 2)}
        if h.getModelStatus() == highspy.HighsModelStatus.kOptimal:
            sol = list(h.getSolution().col_value)
            res["objective"] = h.getInfo().objective_function_value
            res["L"] = {str(i): sol[c] for i, c in L.items()}
            res["integral"] = all(abs(sol[c] - round(sol[c])) < 1e-6 for c in L.values()) and all(abs(sol[c] - round(sol[c])) < 1e-6 for c in Z.values())
            res["integral_all"] = all(abs(v - round(v)) < 1e-6 for i, v in enumerate(sol) if model.integer[i])
            if integral:
                # The box for the report: the least east and south edge with these lengths (a second LP, lengths fixed).
                h2 = model.highs(False)
                for i, c in L.items():
                    h2.changeColBounds(c, round(sol[c]), round(sol[c]))
                for i, c in Z.items():
                    h2.changeColBounds(c, round(sol[c]), round(sol[c]))
                anyc = d["X"] if d["X"] is not None else d["Y"]
                n = len(model.lo)
                cost = np.zeros(n)
                cost[x[anyc["xe_pt"]]] = 1
                cost[y[anyc["se_pt"]]] = 1
                h2.changeColsCost(n, np.arange(n, dtype=np.int32), cost)
                h2.run()
                s2 = list(h2.getSolution().col_value)
                res["box"] = [round(s2[x[anyc["xe_pt"]]]) + 1, round(s2[y[anyc["se_pt"]]]) + 1]
                res["sol"] = sol
        elif h.getModelStatus() == highspy.HighsModelStatus.kInfeasible and not integral:
            st, iis = h.getIis()
            res["iis_rows"] = list(iis.row_index_)
            res["iis_chain"] = chain_text(model, list(iis.row_index_))
        out[key] = res
    return out, model, L


def compare(d, out):
    """Per free bend and joiner Corner: today's settled size against the ILP's."""
    if d["choices"] is None or "L" not in out["ilp"]:
        return []
    ref = d["X"]
    m = {(e, t): v for e, t, v in d["choices"]["m"]}
    corner = {(e, t): v for e, t, v in d["choices"]["corner"]}
    diffs = []
    for b in ref["bends"]:
        if b["copy"] != 0 or b["source"] not in ("free", "corner_rows"):
            continue
        old = m[(b["edge"], b["tok"])] if b["source"] == "free" else corner[(b["edge"], b["tok"])]
        new = round(out["ilp"]["L"][str(b["id"])])
        diffs.append({"bend": b["id"], "edge": b["edge"], "tok": b["tok"], "kind": b["kind"], "source": b["source"], "old": old, "new": new})
    return diffs


def old_objective(d, model, L):
    """The objective at today's sizes: the ILP with every free size and joiner Corner span fixed to the settled choices."""
    if d["choices"] is None:
        return None
    m = {(e, t): v for e, t, v in d["choices"]["m"]}
    corner = {(e, t): v for e, t, v in d["choices"]["corner"]}
    h = model.highs(True)
    for b in d["X"]["bends"]:
        if b["copy"] != 0 or b["source"] not in ("free", "corner_rows"):
            continue
        v = m[(b["edge"], b["tok"])] if b["source"] == "free" else corner[(b["edge"], b["tok"])]
        h.changeColBounds(L[b["id"]], v, v)
    h.run()
    if h.getModelStatus() != highspy.HighsModelStatus.kOptimal:
        return h.modelStatusToString(h.getModelStatus())
    return h.getInfo().objective_function_value


def main():
    folder = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.abspath(__file__))
    out_dir = sys.argv[2] if len(sys.argv) > 2 else folder
    names = sys.argv[3:] or NAMES
    for name in names:
        d = json.load(open(os.path.join(folder, name + ".json")))
        out, model, L = solve(d)
        out["diffs"] = compare(d, out)
        out["old_objective"] = old_objective(d, model, L)
        ilp, lp = out["ilp"], out["lp"]
        print(f"== {name}: seed {'places' if d['seed_places'] else 'fails: ' + d['error'][:100]}")
        print(f"   vars {out['vars']} rows {out['rows']} (bends {out['bends']}, fan binaries {out['fan_binaries']}, links {out['links']}); relations {out['counts']}")
        print(f"   non-linear sites held at the dump: {out['nonlinear_sites']}")
        print(f"   ILP {ilp['status']} {ilp['time_ms']} ms obj {ilp.get('objective')} box {ilp.get('box')}; LP {lp['status']} {lp['time_ms']} ms obj {lp.get('objective')} integral L {lp.get('integral')} all {lp.get('integral_all')}")
        if out["old_objective"] is not None:
            print(f"   objective at today's sizes: {out['old_objective']}")
        for df in out["diffs"]:
            if df["old"] != df["new"]:
                print(f"   differs: bend {df['bend']} edge {df['edge']} tok {df['tok']} {df['kind']}/{df['source']}: {df['old']} -> {df['new']}")
        if "iis_chain" in lp:
            print(f"   IIS ({len(lp['iis_rows'])} rows): {lp['iis_chain']}")
        if out["left_out"]:
            print(f"   left out: {out['left_out']}")
        res = {k: v for k, v in out.items()}
        res["ilp"] = {k: v for k, v in ilp.items() if k != "sol"}
        json.dump(res, open(os.path.join(out_dir, name + ".result.json"), "w"), indent=1)
        if "L" in ilp:
            # Choices for the fixed path: free sizes and joiner Corner spans; the port offset is today's seed's, else the dump's
            # port row of slot 0 (the W side's first slot, notation row 0), 0 when the dumped call never set the port rows.
            ref = d["X"] if d["X"] is not None and d["X"]["rows_known"] else d["Y"]
            offset = d["choices"]["port_offset"] if d["choices"] is not None else ref["port_row"][0]
            with open(os.path.join(out_dir, name + ".choices"), "w") as f:
                f.write(f"{offset}\n")
                for b in ref["bends"]:
                    if b["copy"] != 0:
                        continue
                    if b["source"] == "free":
                        f.write(f"m {b['edge']} {b['tok']} {round(ilp['L'][str(b['id'])])}\n")
                    elif b["source"] == "corner_rows":
                        f.write(f"c {b['edge']} {b['tok']} {round(ilp['L'][str(b['id'])])}\n")


if __name__ == "__main__":
    main()
