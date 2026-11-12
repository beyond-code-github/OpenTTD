/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_positions.cpp Test functionality from junction_positions. */

#include "../stdafx.h"

#include "../3rdparty/catch2/catch.hpp"

#include "../junction_positions.h"

#include <random>

#include "../safeguards.h"

TEST_CASE("positions: differences give the least and greatest solution", "[junction][positions]")
{
	JunctionPositions p(3, 0, 20);
	CHECK(p.AddDifference(1, 0, 3));
	CHECK(p.AddDifference(2, 1, 4));
	CHECK(p.RestrictRange(2, 0, 15));
	CHECK(p.Lo(0) == 0);
	CHECK(p.Lo(1) == 3);
	CHECK(p.Lo(2) == 7);
	CHECK(p.Up(0) == 8);
	CHECK(p.Up(1) == 11);
	CHECK(p.Up(2) == 15);
}

TEST_CASE("positions: a negative cycle is infeasible", "[junction][positions]")
{
	JunctionPositions p(2, 0, 20);
	CHECK(p.AddDifference(1, 0, 2));
	CHECK_FALSE(p.AddDifference(0, 1, -1));
}

TEST_CASE("positions: a domain bitset moves lo and up to allowed values", "[junction][positions]")
{
	JunctionPositions p(2, 0, 10);
	CHECK(p.RestrictDomain(0, {2, 5, 9}));
	CHECK(p.RestrictRange(1, 3, 10));
	CHECK(p.AddDifference(0, 1, 1));
	CHECK(p.Lo(0) == 5);
	CHECK(p.Up(0) == 9);
}

TEST_CASE("positions: avoid", "[junction][positions]")
{
	{
		JunctionPositions p(2, 0, 20);
		CHECK(p.AddDifference(1, 0, 2));
		CHECK(p.AddAvoid(0, 1, 1));
		CHECK(p.Lo(0) == 2);
		CHECK(p.Lo(1) == 4);
	}
	{
		JunctionPositions p(2, 0, 20);
		CHECK(p.AddDifference(1, 0, 2));
		CHECK(p.RestrictRange(0, 0, 1));
		CHECK_FALSE(p.AddAvoid(0, 1, 1));
	}
}

TEST_CASE("positions: an infeasible propagation explains itself", "[junction][positions]")
{
	JunctionPositions p(4, 0, 100);
	CHECK(p.AddDifference(1, 0, 3, 7));
	CHECK(p.AddDifference(2, 1, 4, 8));
	CHECK(p.RestrictRange(0, 5, 5, 6));
	const JunctionPositions feasible = p;
	CHECK_FALSE(p.RestrictRange(2, 0, 10, 9));
	CHECK(p.Explain() == std::vector<uint32_t>{8, 7, 6, 9});
	CHECK(feasible.Explain(2, true) == std::vector<uint32_t>{8, 7, 6});
}

TEST_CASE("positions: undo restores exactly", "[junction][positions]")
{
	JunctionPositions p(3, 0, 20);
	CHECK(p.RestrictDomain(1, {1, 4, 8, 12, 19}));
	auto snapshot = [&p]() {
		std::vector<int> s;
		for (size_t v = 0; v < p.Size(); v++) {
			s.push_back(p.Lo(v));
			s.push_back(p.Up(v));
		}
		return s;
	};
	std::vector<int> before = snapshot();
	size_t mark = p.Mark();
	CHECK(p.AddDifference(1, 0, 3));
	CHECK(p.AddDifference(2, 1, 4));
	CHECK(p.RestrictRange(2, 0, 15));
	CHECK(snapshot() != before);
	p.Undo(mark);
	CHECK(snapshot() == before);

	/* Also after a failing call, and the domain bits are back: the same additions behave the same. */
	CHECK(p.AddDifference(1, 0, 3));
	CHECK_FALSE(p.AddDifference(0, 1, -2));
	p.Undo(mark);
	CHECK(snapshot() == before);
	CHECK(p.AddDifference(2, 1, 4));
	CHECK(p.Lo(1) == 1);
	CHECK(p.Lo(2) == 5);
	p.Undo(mark);
	CHECK(snapshot() == before);
}

struct BruteConstraint {
	enum class Kind { Difference, Domain, Avoid } kind;
	size_t a, b;
	int k; ///< Difference offset, or avoided column.
	std::vector<bool> allowed;
};

static bool Satisfies(const BruteConstraint &c, const std::vector<int> &x)
{
	switch (c.kind) {
		case BruteConstraint::Kind::Difference: return x[c.a] - x[c.b] >= c.k;
		case BruteConstraint::Kind::Domain: return c.allowed[x[c.a]];
		case BruteConstraint::Kind::Avoid: return !(x[c.a] <= c.k && c.k <= x[c.b]);
	}
	return false;
}

static bool SatisfiesAll(const std::vector<BruteConstraint> &cs, const std::vector<int> &x)
{
	for (const BruteConstraint &c : cs) {
		if (!Satisfies(c, x)) return false;
	}
	return true;
}

TEST_CASE("positions: agree with brute force", "[junction][positions]")
{
	std::minstd_rand rng(1);
	auto uniform = [&rng](int lo, int hi) { return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1)); };

	for (int system = 0; system < 300; system++) {
		size_t vars = static_cast<size_t>(uniform(2, 4));
		std::vector<BruteConstraint> cs;
		JunctionPositions p(vars, 0, 12);
		bool all_true = true;
		int count = uniform(1, 6);
		for (int i = 0; i < count; i++) {
			int kind = uniform(0, 2);
			size_t a = static_cast<size_t>(uniform(0, static_cast<int>(vars) - 1));
			size_t b = static_cast<size_t>(uniform(0, static_cast<int>(vars) - 2));
			if (b >= a) b++;
			if (kind == 0) {
				int k = uniform(-6, 6);
				cs.push_back({BruteConstraint::Kind::Difference, a, b, k, {}});
				all_true = p.AddDifference(a, b, k) && all_true;
			} else if (kind == 1) {
				std::vector<bool> allowed(13);
				std::vector<int> values;
				for (int v = 0; v <= 12; v++) {
					allowed[v] = uniform(0, 9) < 6;
					if (allowed[v]) values.push_back(v);
				}
				cs.push_back({BruteConstraint::Kind::Domain, a, a, 0, allowed});
				all_true = p.RestrictDomain(a, values) && all_true;
			} else {
				/* The caller guarantees x_a <= x_b for an avoid (see the header), so the system carries that difference too. */
				int c = uniform(0, 12);
				cs.push_back({BruteConstraint::Kind::Difference, b, a, 0, {}});
				cs.push_back({BruteConstraint::Kind::Avoid, a, b, c, {}});
				all_true = p.AddDifference(b, a, 0) && all_true;
				all_true = p.AddAvoid(a, b, c) && all_true;
			}
		}

		std::vector<int> best_lo(vars, 13), best_up(vars, -1);
		bool feasible = false;
		std::vector<int> x(vars, 0);
		for (;;) {
			if (SatisfiesAll(cs, x)) {
				feasible = true;
				for (size_t v = 0; v < vars; v++) {
					best_lo[v] = std::min(best_lo[v], x[v]);
					best_up[v] = std::max(best_up[v], x[v]);
				}
			}
			size_t v = 0;
			while (v < vars && x[v] == 12) x[v++] = 0;
			if (v == vars) break;
			x[v]++;
		}

		INFO("system " << system);
		REQUIRE(all_true == feasible);
		if (!feasible) continue;
		std::vector<int> lo, up;
		for (size_t v = 0; v < vars; v++) {
			CHECK(p.Lo(v) == best_lo[v]);
			CHECK(p.Up(v) == best_up[v]);
			lo.push_back(p.Lo(v));
			up.push_back(p.Up(v));
		}
		CHECK(SatisfiesAll(cs, lo));
		CHECK(SatisfiesAll(cs, up));
	}
}
