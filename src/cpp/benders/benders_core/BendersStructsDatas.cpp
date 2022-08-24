#include "BendersStructsDatas.h"

/*!
 * \brief Get point
 */
Point WorkerMasterData::get_point() const { return *_x_out; }

Point WorkerMasterData::get_min_invest() const { return *_min_invest; }

Point WorkerMasterData::get_max_invest() const { return *_max_invest; }

LogData defineLogDataFromBendersDataAndTrace(const BendersData& data,
                                             const BendersTrace& trace) {
  LogData result;

  result.it = data.it;
  result.best_it = data.best_it;
  result.lb = data.lb;
  result.best_ub = data.best_ub;
  result.x_out = data.x_in;
  size_t bestItIndex_l = data.best_it - 1;

  if (bestItIndex_l < trace.size()) {
    const WorkerMasterDataPtr& bestItTrace = trace[bestItIndex_l];
    result.subproblem_cost = bestItTrace->_operational_cost;
    result.invest_cost = bestItTrace->_invest_cost;
    result.min_invest = bestItTrace->get_min_invest();
    result.max_invest = bestItTrace->get_max_invest();
  }

  return result;
}
