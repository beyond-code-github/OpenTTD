/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_build.cpp Sequencer that builds a junction one step at a time. */

#include "../../stdafx.h"

#include "junction_build.h"

#include "../../safeguards.h"

/**
 * Start building.
 * @param steps The steps to build, in order.
 */
void JunctionSequencer::Start(std::vector<JunctionBuildStep> steps)
{
	this->steps = std::move(steps);
	this->next = 0;
	this->spent = 0;
	this->awaiting = false;
	this->state = this->steps.empty() ? JunctionBuildState::Done : JunctionBuildState::Building;
}

/**
 * Post the next step, if the state is Building and no result is pending.
 * @param poster Posts the step as a command.
 */
void JunctionSequencer::Tick(JunctionStepPoster &poster)
{
	if (this->state != JunctionBuildState::Building || this->awaiting) return;

	this->awaiting = true;
	bool ok = poster.Post(this->steps[this->next]);

	/* The poster refused without calling back, so report the failure here. */
	if (this->awaiting && !ok) this->OnStepResult(false);
}

/**
 * Report the result of the step that was posted last.
 * @param success Whether the step succeeded.
 */
void JunctionSequencer::OnStepResult(bool success)
{
	if (this->state != JunctionBuildState::Building || !this->awaiting) return;
	this->awaiting = false;

	if (!success) {
		this->state = JunctionBuildState::Failed;
		return;
	}

	++this->next;
	if (this->next == this->steps.size()) this->state = JunctionBuildState::Done;
}

/** Continue from the failed step. */
void JunctionSequencer::Retry()
{
	if (this->state != JunctionBuildState::Failed) return;
	this->awaiting = false;
	this->state = JunctionBuildState::Building;
}

/** Forget all steps and return to Idle. */
void JunctionSequencer::Cancel()
{
	this->steps.clear();
	this->next = 0;
	this->awaiting = false;
	this->state = JunctionBuildState::Idle;
}

/**
 * Get the steps that haven't succeeded yet.
 * @return The steps from the next one to the end.
 */
std::span<const JunctionBuildStep> JunctionSequencer::GetRemaining() const
{
	return std::span<const JunctionBuildStep>(this->steps).subspan(this->next);
}
