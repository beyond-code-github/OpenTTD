/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_milp.cpp Junction mixed-integer linear programs implementation. */

#include "../../stdafx.h"

#include "junction_milp.h"

#include <Highs.h>

#include <algorithm>

#include "../../safeguards.h"

/** The model, kept until Solve() builds the HiGHS model from it, and the last solve's results. */
struct JunctionMilp::Impl {
	std::vector<double> lower; ///< Variable lower bounds.
	std::vector<double> upper; ///< Variable upper bounds.
	std::vector<double> cost; ///< Variable objective coefficients.
	std::vector<bool> integer; ///< Whether each variable is integer.
	std::vector<double> row_lower; ///< Row lower bounds.
	std::vector<double> row_upper; ///< Row upper bounds.
	std::vector<std::vector<std::pair<uint32_t, double>>> row_terms; ///< Row terms.
	std::vector<double> values; ///< Variable values of the last Optimal solve.
	double objective = 0; ///< Objective of the last Optimal solve.
	std::vector<uint32_t> infeasible_rows; ///< IIS rows of the last Infeasible solve.

	/**
	 * Build the HiGHS model.
	 * @param integral Whether to keep integrality.
	 * @return The model.
	 */
	HighsLp BuildLp(bool integral) const
	{
		HighsLp lp;
		lp.num_col_ = static_cast<HighsInt>(this->lower.size());
		lp.num_row_ = static_cast<HighsInt>(this->row_lower.size());
		lp.col_cost_ = this->cost;
		lp.col_lower_ = this->lower;
		lp.col_upper_ = this->upper;
		lp.row_lower_ = this->row_lower;
		lp.row_upper_ = this->row_upper;
		lp.a_matrix_.format_ = MatrixFormat::kRowwise;
		lp.a_matrix_.start_.assign(1, 0);
		for (const auto &terms : this->row_terms) {
			for (const auto &[variable, coefficient] : terms) {
				lp.a_matrix_.index_.push_back(static_cast<HighsInt>(variable));
				lp.a_matrix_.value_.push_back(coefficient);
			}
			lp.a_matrix_.start_.push_back(static_cast<HighsInt>(lp.a_matrix_.index_.size()));
		}
		if (integral) {
			lp.integrality_.resize(this->lower.size());
			for (size_t i = 0; i < this->lower.size(); i++) lp.integrality_[i] = this->integer[i] ? HighsVarType::kInteger : HighsVarType::kContinuous;
		}
		return lp;
	}

	/**
	 * Start a quiet, exact and deterministic solver on a model.
	 * @param lp The model.
	 * @param highs The solver.
	 * @return Whether the model was accepted.
	 */
	static bool Setup(Highs &highs, HighsLp lp)
	{
		if (highs.setOptionValue("output_flag", false) != HighsStatus::kOk) return false;
		if (highs.setOptionValue("mip_rel_gap", 0.0) != HighsStatus::kOk) return false;
		if (highs.setOptionValue("random_seed", 0) != HighsStatus::kOk) return false;
		if (highs.setOptionValue("threads", 1) != HighsStatus::kOk) return false;
		return highs.passModel(std::move(lp)) == HighsStatus::kOk;
	}
};

JunctionMilp::JunctionMilp() : impl(std::make_unique<Impl>()) {}

JunctionMilp::~JunctionMilp() = default;

uint32_t JunctionMilp::AddVariable(double lower, double upper, double cost, bool integer)
{
	this->impl->lower.push_back(lower);
	this->impl->upper.push_back(upper);
	this->impl->cost.push_back(cost);
	this->impl->integer.push_back(integer);
	return static_cast<uint32_t>(this->impl->lower.size() - 1);
}

uint32_t JunctionMilp::AddRow(std::span<const std::pair<uint32_t, double>> terms, double lower, double upper)
{
	this->impl->row_terms.emplace_back(terms.begin(), terms.end());
	this->impl->row_lower.push_back(lower);
	this->impl->row_upper.push_back(upper);
	return static_cast<uint32_t>(this->impl->row_lower.size() - 1);
}

JunctionMilp::Status JunctionMilp::Solve()
{
	this->impl->values.clear();
	this->impl->objective = 0;
	this->impl->infeasible_rows.clear();

	Highs highs;
	if (!Impl::Setup(highs, this->impl->BuildLp(true))) return Status::Error;
	if (highs.run() == HighsStatus::kError) return Status::Error;
	switch (highs.getModelStatus()) {
		case HighsModelStatus::kOptimal:
			this->impl->values = highs.getSolution().col_value;
			this->impl->objective = highs.getInfo().objective_function_value;
			return Status::Optimal;

		case HighsModelStatus::kInfeasible: {
			Highs relaxation;
			if (!Impl::Setup(relaxation, this->impl->BuildLp(false))) return Status::Error;
			if (relaxation.setOptionValue("iis_strategy", 2) != HighsStatus::kOk) return Status::Error;
			HighsIis iis;
			if (relaxation.getIis(iis) == HighsStatus::kError) return Status::Error;
			for (HighsInt row : iis.row_index_) this->impl->infeasible_rows.push_back(static_cast<uint32_t>(row));
			std::sort(this->impl->infeasible_rows.begin(), this->impl->infeasible_rows.end());
			return Status::Infeasible;
		}

		default:
			return Status::Error;
	}
}

double JunctionMilp::Value(uint32_t variable) const
{
	return this->impl->values[variable];
}

double JunctionMilp::Objective() const
{
	return this->impl->objective;
}

std::vector<uint32_t> JunctionMilp::InfeasibleRows() const
{
	return this->impl->infeasible_rows;
}
