/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_compact.cpp Junction layout compactness measure. */

#include "stdafx.h"

#include "junction_compact.h"

#include <queue>
#include <utility>

#include "safeguards.h"

/**
 * Measure a layout's compactness (D153).
 * @param layout The layout to measure.
 * @return The occupied, sliver and enclosed tile counts.
 */
JunctionCompactness MeasureJunctionCompactness(const JunctionLayout &layout)
{
	const int w = layout.size.x;
	const int h = layout.size.y;
	const int stride = w + 2;
	const int rows = h + 2;

	/* Grid over the layout's size padded by one free tile on every side. */
	std::vector<bool> blocked(static_cast<size_t>(stride) * rows, false);
	auto mark = [&](int x, int y) {
		if (x >= 0 && x < w && y >= 0 && y < h) blocked[static_cast<size_t>(y + 1) * stride + x + 1] = true;
	};

	for (const JunctionTrack &track : layout.tracks) mark(track.offset.x, track.offset.y);
	for (const JunctionBridge &bridge : layout.bridges) {
		mark(bridge.start.x, bridge.start.y);
		mark(bridge.end.x, bridge.end.y);
	}
	for (const JunctionStationBlock &block : layout.stations) {
		const int bw = block.axis == Axis::X ? block.length : block.platforms;
		const int bh = block.axis == Axis::X ? block.platforms : block.length;
		for (int y = 0; y < bh; y++) {
			for (int x = 0; x < bw; x++) mark(block.origin.x + x, block.origin.y + y);
		}
	}

	JunctionCompactness result;
	for (bool b : blocked) result.occupied += b ? 1 : 0;

	/* Flood fill the free tiles from the padded corner. */
	std::vector<bool> seen(blocked.size(), false);
	std::queue<std::pair<int, int>> queue;
	seen[0] = true;
	queue.push({0, 0});
	while (!queue.empty()) {
		const auto [x, y] = queue.front();
		queue.pop();
		static constexpr std::pair<int, int> step[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
		for (const auto &[dx, dy] : step) {
			const int nx = x + dx;
			const int ny = y + dy;
			if (nx < 0 || nx >= stride || ny < 0 || ny >= rows) continue;
			const size_t n = static_cast<size_t>(ny) * stride + nx;
			if (seen[n] || blocked[n]) continue;
			seen[n] = true;
			queue.push({nx, ny});
		}
	}

	/* A free tile inside the layout's size that lies in no 2x2 square of free tiles inside the size is a sliver. */
	auto free_at = [&](int x, int y) {
		return x >= 1 && x <= w && y >= 1 && y <= h && !blocked[static_cast<size_t>(y) * stride + x];
	};
	for (int y = 1; y <= h; y++) {
		for (int x = 1; x <= w; x++) {
			if (blocked[static_cast<size_t>(y) * stride + x]) continue;
			if (!seen[static_cast<size_t>(y) * stride + x]) {
				result.enclosed++;
				continue;
			}
			bool in_square = false;
			for (int oy = 0; oy < 2 && !in_square; oy++) {
				for (int ox = 0; ox < 2 && !in_square; ox++) {
					in_square = free_at(x - ox, y - oy) && free_at(x - ox + 1, y - oy) && free_at(x - ox, y - oy + 1) && free_at(x - ox + 1, y - oy + 1);
				}
			}
			if (!in_square) result.slivers++;
		}
	}
	return result;
}
