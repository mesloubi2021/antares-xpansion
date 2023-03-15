#include "BendersByBatch.h"

#include <algorithm>
#include <functional>
#include <mutex>
#include <numeric>

#include "BatchCollection.h"
#include "RandomBatchShuffler.h"
#include "glog/logging.h"
BendersByBatch::BendersByBatch(BendersBaseOptions const &options, Logger logger,
                               Writer writer, mpi::environment &env,
                               mpi::communicator &world)
    : BendersMpi(options, logger, writer, env, world) {}

void BendersByBatch::InitializeProblems() {
  MatchProblemToId();

  BuildMasterProblem();
  const auto &coupling_map_size = coupling_map.size();
  std::vector<std::string> problem_names;
  for (const auto &[problem_name, _] : coupling_map) {
    problem_names.emplace_back(problem_name);
  }
  auto batch_size =
      Options().BATCH_SIZE == 0 ? coupling_map_size : Options().BATCH_SIZE;
  batch_collection_.SetLogger(_logger);
  batch_collection_.SetBatchSize(batch_size);
  batch_collection_.SetSubProblemNames(problem_names);
  batch_collection_.BuildBatches();
  BroadCast(batch_collection_, rank_0);
  // Dispatch subproblems to process
  auto problem_count = 0;
  for (const auto &batch : batch_collection_.BatchCollections()) {
    for (const auto &problem_name : batch.sub_problem_names) {
      // In case there are more subproblems than process
      if (auto process_to_feed = problem_count % WorldSize();
          process_to_feed ==
          Rank()) {  // Assign  [problemNumber % WorldSize] to processID

        const auto subProblemFilePath = GetSubproblemPath(problem_name);
        AddSubproblem({problem_name, coupling_map[problem_name]});
        AddSubproblemName(problem_name);
      }
      problem_count++;
    }
  }
}
void BendersByBatch::BroadcastSingleSubpbCostsUnderApprox() {
  DblVector single_subpb_costs_under_approx(_data.nsubproblem);
  if (Rank() == rank_0) {
    single_subpb_costs_under_approx = GetAlpha_i();
  }

  BroadCast(single_subpb_costs_under_approx.data(), _data.nsubproblem, rank_0);
  SetAlpha_i(single_subpb_costs_under_approx);
}
void BendersByBatch::Run() {
  PreRunInitialization();
  MasterLoop();
  if (Rank() == rank_0) {
    compute_ub();
    update_best_ub();
    _logger->log_at_iteration_end(bendersDataToLogData(_data));
    UpdateTrace();
    SaveCurrentBendersData();
    _data.stopping_criterion = _data.stop ? StoppingCriterion::timelimit
                                          : StoppingCriterion::absolute_gap;
    CloseCsvFile();
    EndWritingInOutputFile();
    write_basis();
  }
}

void BendersByBatch::MasterLoop() {
  number_of_batch_ = batch_collection_.NumberOfBatch();
  random_batch_permutation_.resize(number_of_batch_);
  batch_counter_ = 0;
  current_batch_id_ = 0;

  number_of_sub_problem_resolved_ = 0;
  cumulative_subproblems_timer_per_iter_ = 0;
  first_unsolved_batch_ = 0;
  while (batch_counter_ < number_of_batch_ || _data.stop) {
    _data.ub = 0;
    SetSubproblemCost(0);
    remaining_epsilon_ = AbsoluteGap();

    if (Rank() == rank_0) {
      if (SwitchToIntegerMaster(_data.is_in_initial_relaxation)) {
        _logger->LogAtSwitchToInteger();
        ActivateIntegrityConstraints();
        ResetDataPostRelaxation();
      }
      _logger->display_message(
          " _______________________________________________________________"
          "_"
          "________\n/\n");

      _logger->display_message("\tSolving master...");
      get_master_value();
      _logger->log_master_solving_duration(get_timer_master());

      random_batch_permutation_ = RandomBatchShuffler(number_of_batch_)
                                      .GetCyclicBatchOrder(current_batch_id_);
    }
    BroadcastXOut();
    BroadcastSingleSubpbCostsUnderApprox();
    BroadCast(random_batch_permutation_.data(),
              random_batch_permutation_.size(), rank_0);
    SeparationLoop();
    if (Rank() == rank_0) {
      _data.elapsed_time = GetBendersTime();
      _data.stop = (_data.elapsed_time > Options().TIME_LIMIT);
    }
    BroadCast(_data.stop, rank_0);
    BroadCast(batch_counter_, rank_0);
    SetSubproblemsCumulativeCpuTime(cumulative_subproblems_timer_per_iter_);
    _logger->number_of_sub_problem_resolved(number_of_sub_problem_resolved_);
    _logger->LogSubproblemsSolvingCumulativeCpuTime(
        GetSubproblemsCumulativeCpuTime());
    _logger->LogSubproblemsSolvingWalltime(GetSubproblemsWalltime());
    _logger->display_message(
        "\\________________________________________________________________"
        "________\n");
  }
}
void BendersByBatch::SeparationLoop() {
  misprice_ = true;
  first_unsolved_batch_ = 0;
  batch_counter_ = 0;
  while (misprice_ && batch_counter_ < number_of_batch_) {
    _data.it++;

    _logger->log_at_initialization(_data.it + GetNumIterationsBeforeRestart());
    ComputeXCut();
    _logger->log_iteration_candidates(bendersDataToLogData(_data));
    BroadcastXCut();
    UpdateRemainingEpsilon();
    SolveBatches();
  }
}
void BendersByBatch::ComputeXCut() {
  if (_data.it == 1) {
    _data.x_in = _data.x_out;
    _data.x_cut = _data.x_out;
  } else {
    _data.x_in = _data.x_cut;
    for (const auto &[name, value] : _data.x_out) {
      _data.x_cut[name] = Options().SEPARATION_PARAM * _data.x_out[name] +
                          (1 - Options().SEPARATION_PARAM) * _data.x_in[name];
    }
  }
}
void BendersByBatch::UpdateRemainingEpsilon() {
  if (Rank() == rank_0) {
    auto master_ptr = get_master();
    int ncols = master_ptr->_solver->get_ncols();
    std::vector<double> obj(ncols);
    master_ptr->_solver->get_obj(obj.data(), 0, ncols - 1);
    for (const auto &[candidate_name, x_cut_candidate_value] : _data.x_cut) {
      int col_id = master_ptr->_name_to_id[candidate_name];
      remaining_epsilon_ =
          AbsoluteGap() -
          obj[col_id] * (x_cut_candidate_value - _data.x_out[candidate_name]);
    }
  }
}
void BendersByBatch::SolveBatches() {
  batch_counter_ = 0;
  cumulative_subproblems_timer_per_iter_ = 0;
  while (batch_counter_ < number_of_batch_) {
    first_unsolved_batch_ = first_unsolved_batch_ % number_of_batch_;
    current_batch_id_ = random_batch_permutation_[first_unsolved_batch_];
    first_unsolved_batch_++;
    const auto &batch = batch_collection_.GetBatchFromId(current_batch_id_);
    current_batch_id_++;
    const auto &batch_sub_problems = batch.sub_problem_names;
    double batch_subproblems_costs_contribution_in_gap_per_proc = 0;
    double batch_subproblems_costs_contribution_in_gap = 0;
    BuildCut(batch_sub_problems,
             &batch_subproblems_costs_contribution_in_gap_per_proc);
    Reduce(batch_subproblems_costs_contribution_in_gap_per_proc,
           batch_subproblems_costs_contribution_in_gap, std::plus<double>(),
           rank_0);
    Reduce(GetSubproblemsCpuTime(), cumulative_subproblems_timer_per_iter_,
           std::plus<double>(), rank_0);
    if (Rank() == rank_0) {
      number_of_sub_problem_resolved_ += batch_sub_problems.size();
      remaining_epsilon_ -= batch_subproblems_costs_contribution_in_gap;
    }

    BroadCast(remaining_epsilon_, rank_0);
    if (remaining_epsilon_ > 0) {
      batch_counter_++;
    } else
      break;
  }
}
/*!
 * \brief Build subproblem cut
 * Method to build subproblem cuts
 * and add them to the Master problem
 */
void BendersByBatch::BuildCut(
    const std::vector<std::string> &batch_sub_problems,
    double *batch_subproblems_costs_contribution_in_gap_per_proc) {
  SubProblemDataMap subproblem_data_map;
  Timer subproblems_timer_per_proc;
  GetSubproblemCut(subproblem_data_map, batch_sub_problems,
                   batch_subproblems_costs_contribution_in_gap_per_proc);

  SetSubproblemsCpuTime(subproblems_timer_per_proc.elapsed());
  std::vector<SubProblemDataMap> gathered_subproblem_map;
  bool global_misprice = misprice_;
  AllReduce(misprice_, global_misprice, std::logical_and<bool>());
  misprice_ = global_misprice;
  Gather(subproblem_data_map, gathered_subproblem_map, rank_0);
  SetSubproblemsWalltime(subproblems_timer_per_proc.elapsed());
  for (const auto &subproblem_map : gathered_subproblem_map) {
    for (auto &&[_, subproblem_data] : subproblem_map) {
      SetSubproblemCost(GetSubproblemCost() + subproblem_data.subproblem_cost);
    }
  }
  for (const auto &subproblem_map : gathered_subproblem_map) {
    BuildCutFull(subproblem_map);
  }
}

/*!
 *  \brief Solve and store optimal variables of all Subproblem Problems
 *
 *  Method to solve and store optimal variables of all Subproblem Problems
 * after fixing trial values
 *
 *  \param subproblem_data_map : map storing for each subproblem its cut
 */
void BendersByBatch::GetSubproblemCut(
    SubProblemDataMap &subproblem_data_map,
    const std::vector<std::string> &batch_sub_problems,
    double *batch_subproblems_costs_contribution_in_gap_per_proc) {
  *batch_subproblems_costs_contribution_in_gap_per_proc = 0;
  const auto &sub_pblm_map = GetSubProblemMap();

  for (const auto &[name, worker] : sub_pblm_map) {
    if (std::find(batch_sub_problems.cbegin(), batch_sub_problems.cend(),
                  name) != batch_sub_problems.cend()) {
      Timer subproblem_timer;
      SubProblemData subproblem_data;
      worker->fix_to(_data.x_cut);
      worker->solve(subproblem_data.lpstatus, Options().OUTPUTROOT,
                    Options().LAST_MASTER_MPS + MPS_SUFFIX);
      worker->get_value(subproblem_data.subproblem_cost);  // solution phi(x,s)
      worker->get_subgradient(
          subproblem_data.var_name_and_subgradient);  // dual pi_s
      auto subpb_cost_under_approx = GetAlpha_i()[ProblemToId(name)];
      *batch_subproblems_costs_contribution_in_gap_per_proc +=
          subproblem_data.subproblem_cost - subpb_cost_under_approx;
      double cut_value_at_x_cut = subproblem_data.subproblem_cost;
      for (const auto &[candidate_name, x_cut_candidate_value] : _data.x_cut) {
        auto subgradient_at_name =
            subproblem_data.var_name_and_subgradient[candidate_name];
        cut_value_at_x_cut +=
            subgradient_at_name *
            (_data.x_out[candidate_name] - x_cut_candidate_value);
      }

      if (subpb_cost_under_approx < cut_value_at_x_cut) {
        misprice_ = false;
      }
      worker->get_splex_num_of_ite_last(subproblem_data.simplex_iter);
      subproblem_data.subproblem_timer = subproblem_timer.elapsed();
      subproblem_data_map[name] = subproblem_data;
    }
  }
}
void BendersByBatch::BroadcastXOut() {
  Point x_out = get_x_out();
  BroadCast(x_out, rank_0);
  set_x_out(x_out);
}
