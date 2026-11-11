/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_positions.h Junction node positions: incremental propagation of min- and max-closed integer constraints (D183). */

#ifndef JUNCTION_POSITIONS_H
#define JUNCTION_POSITIONS_H

#include <cstdint>
#include <deque>
#include <vector>

/**
 * Integer node positions under min- and max-closed constraints (DESIGN-nodes §9, FABLE-nodes-perf §1.1): differences, unary domains and "avoid".
 * Lo() is the least solution and Up() the greatest; both are kept by monotone propagation (FIFO, deterministic), and every change is recorded on a trail so Undo() restores an earlier state.
 * A call that makes some variable's lo exceed its up returns false; the state is then inconsistent until Undo() to a mark taken before the call.
 */
class JunctionPositions {
public:
	JunctionPositions(size_t vars, int min_value, int max_value); ///< Every variable starts with the domain [min_value, max_value].
	bool AddDifference(size_t a, size_t b, int k); ///< x_a - x_b >= k.
	bool RestrictRange(size_t v, int min_value, int max_value); ///< x_v in [min_value, max_value].
	bool RestrictDomain(size_t v, const std::vector<int> &allowed); ///< x_v in the given values (others removed from its bitset).
	bool AddAvoid(size_t a, size_t b, int c); ///< Not (x_a <= c <= x_b): an edge from x_a to x_b must not cover column c.
	size_t Mark() const; ///< A trail position for Undo().
	void Undo(size_t mark); ///< Restore lo, up and domains to the state at the mark; constraints added since are removed.
	int Lo(size_t v) const;
	int Up(size_t v) const;
	size_t Size() const;

private:
	struct Difference { size_t a, b; int k; };
	struct Avoid { size_t a, b; int c; };
	struct TrailEntry {
		enum class Kind : uint8_t { Lo, Up, DomainWord, Difference, Avoid } kind;
		size_t index;
		int64_t old;
	};

	bool SetLo(size_t v, int value);
	bool SetUp(size_t v, int value);
	bool Propagate();
	bool PropagateDifference(const Difference &d);
	bool PropagateAvoid(const Avoid &a);
	bool Allowed(size_t v, int value) const;

	int min_value; ///< Smallest position of every domain.
	int max_value; ///< Largest position of every domain.
	std::vector<int> lo, up;
	std::vector<std::vector<uint64_t>> domain; ///< Per variable, a bit per position in [min_value, max_value].
	std::vector<Difference> differences;
	std::vector<Avoid> avoids;
	std::vector<std::vector<size_t>> differences_of; ///< Per variable, the indices into 'differences' touching it.
	std::vector<std::vector<size_t>> avoids_of; ///< Per variable, the indices into 'avoids' touching it.
	std::vector<TrailEntry> trail;
	std::deque<size_t> queue; ///< Variables whose lo or up changed.
};

#endif /* JUNCTION_POSITIONS_H */
