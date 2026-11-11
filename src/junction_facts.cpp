/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_facts.cpp Per-tile terrain facts of a junction snapshot implementation. */

#include "stdafx.h"

#include "junction_facts.h"
#include "landscape.h"
#include "rail.h"
#include "slope_func.h"
#include "track_func.h"

#include <climits>

#include "safeguards.h"

JunctionFacts::JunctionFacts(const JunctionTerrain &terrain) : origin_x(terrain.origin.x), origin_y(terrain.origin.y), width(terrain.width), height(terrain.height), build_on_slopes(terrain.build_on_slopes)
{
	const size_t cells = static_cast<size_t>(this->width) * this->height;
	this->slope.assign(cells, SLOPE_FLAT);
	this->z_min.assign(cells, 0);
	this->buildable.assign(cells, false);
	this->sw.assign(cells, false);
	this->half.assign(cells, false);
	for (size_t a = 0; a < 2; a++) {
		this->entry[a].assign(cells, 0);
		this->exit[a].assign(cells, 0);
		this->straight[a].assign(cells, false);
		this->run_id[a].assign(cells, 0);
	}

	for (int gy = 0; gy < this->height; gy++) {
		for (int gx = 0; gx < this->width; gx++) {
			const size_t i = static_cast<size_t>(gy) * this->width + gx;
			const int x = gx + this->origin_x;
			const int y = gy + this->origin_y;
			const JunctionTerrainTile *tile = terrain.At(x, y);
			assert((tile->slope == SLOPE_FLAT) == (tile->z_min == tile->z_max));
			assert(tile->incline == DiagDirection::Invalid || InclinedSlope(tile->incline) == tile->slope);
			this->slope[i] = tile->slope;
			this->z_min[i] = tile->z_min;
			this->buildable[i] = tile->cls == JunctionTileClass::Buildable;
			if (tile->cls != JunctionTileClass::Buildable) continue;
			if (!tile->steep) {
				const bool level = terrain.UsableAt(x, y, tile->z_max);
				this->sw[i] = level;
				this->half[i] = level;
			}
			/* A lone axis track is what the game builds: its fit and edge heights come from the game's foundation (D194). */
			for (size_t a = 0; a < 2; a++) {
				const Track track = a == 0 ? Track::X : Track::Y;
				const DiagDirection out = TrackdirToExitdir(a == 0 ? Trackdir::X_SW : Trackdir::Y_SE);
				if (!this->Fits(x, y, TrackBits{track})) continue;
				this->straight[a][i] = true;
				this->entry[a][i] = static_cast<int16_t>(this->EdgeZ(x, y, TrackBits{track}, ReverseDiagDir(out)));
				this->exit[a][i] = static_cast<int16_t>(this->EdgeZ(x, y, TrackBits{track}, out));
			}
		}
	}

	/* Walk every line of each axis once, filling run ids and the bad-position arrays. */
	for (size_t k = 0; k < 6; k++) {
		this->next_bad[k].assign(cells, INT_MAX);
		this->prev_bad[k].assign(cells, INT_MIN);
	}
	int next_id = 0;
	for (size_t a = 0; a < 2; a++) {
		const Axis axis = a == 0 ? Axis::X : Axis::Y;
		const int lines = axis == Axis::X ? this->height : this->width;
		const int length = axis == Axis::X ? this->width : this->height;
		const int line_origin = axis == Axis::X ? this->origin_y : this->origin_x;
		const int pos_origin = axis == Axis::X ? this->origin_x : this->origin_y;
		for (int line = 0; line < lines; line++) {
			std::array<int, 3> next{INT_MAX, INT_MAX, INT_MAX};
			std::array<int, 3> prev{INT_MIN, INT_MIN, INT_MIN};
			size_t before = 0;
			for (int p = 0; p < length; p++) {
				size_t i;
				this->Cell(axis, line + line_origin, p + pos_origin, i);
				const std::array<bool, 3> bad{!this->straight[a][i], !this->sw[i], !this->half[i]};
				for (size_t kind = 0; kind < 3; kind++) {
					if (bad[kind]) prev[kind] = p + pos_origin;
					this->prev_bad[kind * 2 + a][i] = prev[kind];
				}
				const bool connects = p > 0 && this->straight[a][i] && this->straight[a][before] && this->exit[a][before] == this->entry[a][i];
				this->run_id[a][i] = connects ? this->run_id[a][before] : next_id++;
				before = i;
			}
			for (int p = length - 1; p >= 0; p--) {
				size_t i;
				this->Cell(axis, line + line_origin, p + pos_origin, i);
				const std::array<bool, 3> bad{!this->straight[a][i], !this->sw[i], !this->half[i]};
				for (size_t kind = 0; kind < 3; kind++) {
					if (bad[kind]) next[kind] = p + pos_origin;
					this->next_bad[kind * 2 + a][i] = next[kind];
				}
			}
		}
	}
}

/** Map a line and a position along it to the grid cell index; false when outside the grid. */
bool JunctionFacts::Cell(Axis axis, int line, int pos, size_t &index) const
{
	const int x = (axis == Axis::X ? pos : line) - this->origin_x;
	const int y = (axis == Axis::X ? line : pos) - this->origin_y;
	if (x < 0 || y < 0 || x >= this->width || y >= this->height) return false;
	index = static_cast<size_t>(y) * this->width + x;
	return true;
}

bool JunctionFacts::Straight(int x, int y, Axis axis, int &entry, int &exit) const
{
	size_t i;
	if (!this->Cell(Axis::X, y, x, i) || !this->straight[to_underlying(axis)][i]) return false;
	entry = this->entry[to_underlying(axis)][i];
	exit = this->exit[to_underlying(axis)][i];
	return true;
}

bool JunctionFacts::Fits(int x, int y, TrackBits bits) const
{
	size_t i;
	if (!this->Cell(Axis::X, y, x, i) || !this->buildable[i]) return false;
	const Foundation foundation = GetRailFoundation(this->slope[i], bits);
	if (foundation == Foundation::Invalid) return false;
	return foundation == Foundation::None || this->build_on_slopes;
}

int JunctionFacts::EdgeZ(int x, int y, TrackBits bits, DiagDirection edge) const
{
	if (!this->Fits(x, y, bits)) return INT_MIN;
	size_t i;
	this->Cell(Axis::X, y, x, i);
	static constexpr int MID = TILE_SIZE / 2;
	static constexpr int LAST = TILE_SIZE - 1;
	static constexpr std::array<std::pair<int, int>, 4> MIDPOINT = {{{0, MID}, {MID, LAST}, {LAST, MID}, {MID, 0}}}; // Indexed by DiagDirection: NE, SE, SW, NW.
	Slope s = this->slope[i];
	const int lift = ApplyPixelFoundationToSlope(GetRailFoundation(s, bits), s);
	const auto [mx, my] = MIDPOINT[to_underlying(edge)];
	return this->z_min[i] * static_cast<int>(TILE_HEIGHT) + lift + static_cast<int>(GetPartialPixelZ(mx, my, s));
}

bool JunctionFacts::Switch(int x, int y) const
{
	size_t i;
	return this->Cell(Axis::X, y, x, i) && this->sw[i];
}

bool JunctionFacts::Half(int x, int y) const
{
	size_t i;
	return this->Cell(Axis::X, y, x, i) && this->half[i];
}

bool JunctionFacts::StraightRun(Axis axis, int line, int a, int b) const
{
	size_t ia, ib;
	if (!this->Cell(axis, line, a, ia) || !this->Cell(axis, line, b, ib)) return false;
	const size_t ax = to_underlying(axis);
	return this->straight[ax][ia] && this->straight[ax][ib] && this->run_id[ax][ia] == this->run_id[ax][ib];
}

int JunctionFacts::NextBad(JunctionFactKind kind, Axis axis, int line, int from) const
{
	size_t i;
	if (!this->Cell(axis, line, from, i)) return from;
	return this->next_bad[to_underlying(kind) * 2 + to_underlying(axis)][i];
}

int JunctionFacts::PrevBad(JunctionFactKind kind, Axis axis, int line, int from) const
{
	size_t i;
	if (!this->Cell(axis, line, from, i)) return from;
	return this->prev_bad[to_underlying(kind) * 2 + to_underlying(axis)][i];
}
