/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_chain.cpp Junction curve speed as pairwise gap constraints implementation. */

#include "stdafx.h"

#include "junction_chain.h"

#include "junction_generator.h"

#include <algorithm>

#include "safeguards.h"

std::vector<JunctionCurveGap> JunctionCurveGaps(const std::vector<JunctionCurveHand> &hands, uint8_t tl, uint8_t curve_speed, uint16_t reference_speed)
{
	const int n = 2 * tl;
	const int cl = static_cast<int>(DeriveCurveSpacing(reference_speed, curve_speed, tl));

	std::vector<JunctionCurveGap> gaps;
	for (size_t i = 0; i < hands.size(); i++) {
		for (size_t j = i + 1; j < hands.size(); j++) {
			int gap;
			if (j == i + 1) {
				gap = hands[i] != hands[j] ? 2 : std::min(n - 1, std::max(2, cl));
			} else {
				gap = std::min(n - 1, static_cast<int>(j - i) * cl);
			}
			gaps.push_back({static_cast<uint16_t>(i), static_cast<uint16_t>(j), static_cast<int16_t>(gap)});
		}
	}
	return gaps;
}

bool JunctionCurveGapsHold(const std::vector<JunctionCurveGap> &gaps, const std::vector<int> &pos)
{
	for (const JunctionCurveGap &g : gaps) {
		if (pos[g.j] - pos[g.i] < g.min_gap) return false;
	}
	return true;
}
