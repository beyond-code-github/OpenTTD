/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_positions.cpp Junction node positions implementation. */

#include "stdafx.h"

#include "junction_positions.h"

#include <algorithm>

#include "safeguards.h"

JunctionPositions::JunctionPositions(size_t vars, int min_value, int max_value) : min_value(min_value), max_value(max_value), lo(vars, min_value), up(vars, max_value), lo_cause(vars), up_cause(vars), differences_of(vars), avoids_of(vars)
{
	size_t bits = static_cast<size_t>(max_value - min_value + 1);
	std::vector<uint64_t> full((bits + 63) / 64, ~static_cast<uint64_t>(0));
	if (bits % 64 != 0) full.back() = (static_cast<uint64_t>(1) << (bits % 64)) - 1;
	domain.assign(vars, full);
}

bool JunctionPositions::Allowed(size_t v, int value) const
{
	size_t bit = static_cast<size_t>(value - this->min_value);
	return (this->domain[v][bit / 64] >> (bit % 64) & 1) != 0;
}

/**
 * Raise lo of v to the first allowed value >= value (a no-op when that is not above lo).
 * @return Whether v still has lo <= up.
 */
bool JunctionPositions::SetLo(size_t v, int value, Cause cause)
{
	value = std::max(value, this->lo[v]);
	while (value <= this->max_value && !this->Allowed(v, value)) value++;
	if (value == this->lo[v]) return true;
	this->trail.push_back({TrailEntry::Kind::Lo, v, this->lo[v], this->lo_cause[v]});
	this->lo[v] = value;
	this->lo_cause[v] = cause;
	this->queue.push_back(v);
	if (value <= this->up[v]) return true;
	this->failed = v;
	return false;
}

/**
 * Lower up of v to the last allowed value <= value (a no-op when that is not below up).
 * @return Whether v still has lo <= up.
 */
bool JunctionPositions::SetUp(size_t v, int value, Cause cause)
{
	value = std::min(value, this->up[v]);
	while (value >= this->min_value && !this->Allowed(v, value)) value--;
	if (value == this->up[v]) return true;
	this->trail.push_back({TrailEntry::Kind::Up, v, this->up[v], this->up_cause[v]});
	this->up[v] = value;
	this->up_cause[v] = cause;
	this->queue.push_back(v);
	if (this->lo[v] <= value) return true;
	this->failed = v;
	return false;
}

bool JunctionPositions::PropagateDifference(const Difference &d)
{
	return this->SetLo(d.a, this->lo[d.b] + d.k, {d.reason, d.b}) && this->SetUp(d.b, this->up[d.a] - d.k, {d.reason, d.a});
}

bool JunctionPositions::PropagateAvoid(const Avoid &a)
{
	if (this->lo[a.a] <= a.c_a && a.c_b <= this->lo[a.b] && !this->SetLo(a.a, a.c_a + 1, {a.reason, a.b})) return false;
	if (this->up[a.a] <= a.c_a && a.c_b <= this->up[a.b] && !this->SetUp(a.b, a.c_b - 1, {a.reason, a.a})) return false;
	return true;
}

/** Run the queue to a fixpoint; on failure the queue is emptied. */
bool JunctionPositions::Propagate()
{
	while (!this->queue.empty()) {
		size_t v = this->queue.front();
		this->queue.pop_front();
		for (size_t i = 0; i < this->differences_of[v].size(); i++) {
			if (!this->PropagateDifference(this->differences[this->differences_of[v][i]])) {
				this->queue.clear();
				return false;
			}
		}
		for (size_t i = 0; i < this->avoids_of[v].size(); i++) {
			if (!this->PropagateAvoid(this->avoids[this->avoids_of[v][i]])) {
				this->queue.clear();
				return false;
			}
		}
	}
	return true;
}

bool JunctionPositions::AddDifference(size_t a, size_t b, int k, uint32_t reason)
{
	this->trail.push_back({TrailEntry::Kind::Difference, this->differences.size(), 0, {}});
	this->differences_of[a].push_back(this->differences.size());
	this->differences_of[b].push_back(this->differences.size());
	this->differences.push_back({a, b, k, reason});
	this->queue.push_back(a);
	this->queue.push_back(b);
	return this->Propagate();
}

bool JunctionPositions::RestrictRange(size_t v, int min_value, int max_value, uint32_t reason)
{
	if (!this->SetLo(v, min_value, {reason, SIZE_MAX}) || !this->SetUp(v, max_value, {reason, SIZE_MAX})) {
		this->queue.clear();
		return false;
	}
	return this->Propagate();
}

bool JunctionPositions::RestrictDomain(size_t v, const std::vector<int> &allowed, uint32_t reason)
{
	std::vector<uint64_t> keep(this->domain[v].size(), 0);
	for (int value : allowed) {
		if (value < this->min_value || value > this->max_value) continue;
		size_t bit = static_cast<size_t>(value - this->min_value);
		keep[bit / 64] |= static_cast<uint64_t>(1) << (bit % 64);
	}
	for (size_t w = 0; w < keep.size(); w++) {
		uint64_t word = this->domain[v][w] & keep[w];
		if (word == this->domain[v][w]) continue;
		this->trail.push_back({TrailEntry::Kind::DomainWord, v * keep.size() + w, static_cast<int64_t>(this->domain[v][w]), {}});
		this->domain[v][w] = word;
	}
	if (!this->SetLo(v, this->lo[v], {reason, SIZE_MAX}) || !this->SetUp(v, this->up[v], {reason, SIZE_MAX})) {
		this->queue.clear();
		return false;
	}
	return this->Propagate();
}

bool JunctionPositions::AddAvoid(size_t a, size_t b, int c, uint32_t reason)
{
	return this->AddAvoid(a, b, c, c, reason);
}

bool JunctionPositions::AddAvoid(size_t a, size_t b, int c_a, int c_b, uint32_t reason)
{
	this->trail.push_back({TrailEntry::Kind::Avoid, this->avoids.size(), 0, {}});
	this->avoids_of[a].push_back(this->avoids.size());
	this->avoids_of[b].push_back(this->avoids.size());
	this->avoids.push_back({a, b, c_a, c_b, reason});
	this->queue.push_back(a);
	this->queue.push_back(b);
	return this->Propagate();
}

size_t JunctionPositions::Mark() const { return this->trail.size(); }

void JunctionPositions::Undo(size_t mark)
{
	this->queue.clear();
	while (this->trail.size() > mark) {
		const TrailEntry &e = this->trail.back();
		switch (e.kind) {
			case TrailEntry::Kind::Lo:
				this->lo[e.index] = static_cast<int>(e.old);
				this->lo_cause[e.index] = e.old_cause;
				break;
			case TrailEntry::Kind::Up:
				this->up[e.index] = static_cast<int>(e.old);
				this->up_cause[e.index] = e.old_cause;
				break;
			case TrailEntry::Kind::DomainWord: {
				size_t words = this->domain[0].size();
				this->domain[e.index / words][e.index % words] = static_cast<uint64_t>(e.old);
				break;
			}
			case TrailEntry::Kind::Difference:
				this->differences_of[this->differences.back().a].pop_back();
				this->differences_of[this->differences.back().b].pop_back();
				this->differences.pop_back();
				break;
			case TrailEntry::Kind::Avoid:
				this->avoids_of[this->avoids.back().a].pop_back();
				this->avoids_of[this->avoids.back().b].pop_back();
				this->avoids.pop_back();
				break;
		}
		this->trail.pop_back();
	}
}

int JunctionPositions::Lo(size_t v) const { return this->lo[v]; }
int JunctionPositions::Up(size_t v) const { return this->up[v]; }
size_t JunctionPositions::Size() const { return this->lo.size(); }

std::vector<uint32_t> JunctionPositions::Explain(size_t v, bool lo) const
{
	std::vector<uint32_t> reasons;
	std::vector<bool> seen(this->lo.size(), false);
	for (size_t cur = v; cur != SIZE_MAX && !seen[cur];) {
		seen[cur] = true;
		const Cause &cause = lo ? this->lo_cause[cur] : this->up_cause[cur];
		if (cause.reason != NO_REASON) reasons.push_back(cause.reason);
		cur = cause.from;
	}
	return reasons;
}

std::vector<uint32_t> JunctionPositions::Explain() const
{
	if (this->failed == SIZE_MAX) return {};
	std::vector<uint32_t> reasons = this->Explain(this->failed, true);
	for (uint32_t reason : this->Explain(this->failed, false)) reasons.push_back(reason);
	return reasons;
}
