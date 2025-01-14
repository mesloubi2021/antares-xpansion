//
// Created by marechaljas on 18/11/22.
//

#pragma once

#include <antares/solver/lps/LpsFromAntares.h>

#include "../model/Problem.h"
#include "IXpansionProblemsProvider.h"

class XpansionProblemsFromAntaresProvider : public IXpansionProblemsProvider {
 public:
  explicit XpansionProblemsFromAntaresProvider(const Antares::Solver::LpsFromAntares& antares);
  [[nodiscard]] std::vector<std::shared_ptr<Problem>> provideProblems(
      const std::string& solver_name,
      SolverLogManager& solver_log_manager) const override;
  const Antares::Solver::LpsFromAntares& antares_hebdo_problems;
};
