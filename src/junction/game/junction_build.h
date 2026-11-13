/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_build.h Sequencer that builds a junction one step at a time. */

#ifndef JUNCTION_BUILD_H
#define JUNCTION_BUILD_H

#include "../junction_layout.h"

#include "../../economy_type.h"

#include <span>
#include <vector>

/** Progress of building a junction. */
enum class JunctionBuildState : uint8_t {
	Idle,     ///< Nothing to build.
	Building, ///< Posting steps one at a time.
	Failed,   ///< A step failed; waiting for Retry or Cancel.
	Done,     ///< Every step succeeded.
};

/** Posts one build step as a command; tests provide a fake. */
class JunctionStepPoster {
public:
	virtual ~JunctionStepPoster() = default;
	/**
	 * Post a build step.
	 * @return The result of posting; the outcome may also arrive later through JunctionSequencer::OnStepResult.
	 */
	virtual bool Post(const JunctionBuildStep &step) = 0;
};

/** Builds a junction one step at a time, never posting from inside a command callback. */
class JunctionSequencer {
public:
	void Start(std::vector<JunctionBuildStep> steps);
	void Tick(JunctionStepPoster &poster);
	void OnStepResult(bool success);
	void Retry();
	void Cancel();
	JunctionBuildState GetState() const { return this->state; }
	size_t GetDone() const { return this->next; }
	size_t GetTotal() const { return this->steps.size(); }
	std::span<const JunctionBuildStep> GetRemaining() const;

	Money spent = 0; ///< Sum of the costs the executed steps reported.

private:
	std::vector<JunctionBuildStep> steps; ///< Steps in build order.
	size_t next = 0; ///< Index of the next step to post; equals the number of succeeded steps.
	bool awaiting = false; ///< A step has been posted and its result hasn't arrived yet.
	JunctionBuildState state = JunctionBuildState::Idle; ///< Current progress.
};

#endif /* JUNCTION_BUILD_H */
