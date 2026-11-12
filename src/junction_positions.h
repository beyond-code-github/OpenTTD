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
	static constexpr uint32_t NO_REASON = UINT32_MAX;
	bool AddDifference(size_t a, size_t b, int k, uint32_t reason = NO_REASON); ///< x_a - x_b >= k.
	bool RestrictRange(size_t v, int min_value, int max_value, uint32_t reason = NO_REASON); ///< x_v in [min_value, max_value].
	bool RestrictDomain(size_t v, const std::vector<int> &allowed); ///< x_v in the given values (others removed from its bitset).
	bool AddAvoid(size_t a, size_t b, int c); ///< Not (x_a <= c <= x_b): an edge from x_a to x_b must not cover column c.
	size_t Mark() const; ///< A trail position for Undo().
	void Undo(size_t mark); ///< Restore lo, up and domains to the state at the mark; constraints added since are removed.
	int Lo(size_t v) const;
	int Up(size_t v) const;
	size_t Size() const;
	std::vector<uint32_t> Explain(size_t v, bool lo) const; ///< The reasons that set v's lo (or up), from v back to a root (a range, a domain, the initial bound); stops at a variable met twice.
	std::vector<uint32_t> Explain() const; ///< After a call returned false: Explain(failed, true) then Explain(failed, false).

private:
	struct Cause {
		uint32_t reason = NO_REASON;
		size_t from = SIZE_MAX;
	};
	struct Difference { size_t a, b; int k; uint32_t reason; };
	struct Avoid { size_t a, b; int c; };
	struct TrailEntry {
		enum class Kind : uint8_t { Lo, Up, DomainWord, Difference, Avoid } kind;
		size_t index;
		int64_t old;
		Cause old_cause;
	};

	bool SetLo(size_t v, int value, Cause cause);
	bool SetUp(size_t v, int value, Cause cause);
	bool Propagate();
	bool PropagateDifference(const Difference &d);
	bool PropagateAvoid(const Avoid &a);
	bool Allowed(size_t v, int value) const;

	int min_value; ///< Smallest position of every domain.
	int max_value; ///< Largest position of every domain.
	std::vector<int> lo, up;
	std::vector<Cause> lo_cause, up_cause; ///< Per variable, what last set its lo and its up.
	size_t failed = SIZE_MAX; ///< The variable whose lo passed its up in the last failing call.
	std::vector<std::vector<uint64_t>> domain; ///< Per variable, a bit per position in [min_value, max_value].
	std::vector<Difference> differences;
	std::vector<Avoid> avoids;
	std::vector<std::vector<size_t>> differences_of; ///< Per variable, the indices into 'differences' touching it.
	std::vector<std::vector<size_t>> avoids_of; ///< Per variable, the indices into 'avoids' touching it.
	std::vector<TrailEntry> trail;
	std::deque<size_t> queue; ///< Variables whose lo or up changed.
};

#endif /* JUNCTION_POSITIONS_H */
