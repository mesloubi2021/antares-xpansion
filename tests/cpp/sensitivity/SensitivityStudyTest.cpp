#include "SensitivityFileLogger.h"
#include "SensitivityStudy.h"
#include "gtest/gtest.h"
#include "multisolver_interface/SolverFactory.h"

class SensitivityStudyTest : public ::testing::Test {
 public:
  std::string data_test_dir;

  const std::string semibase_name = "semibase";
  const std::string peak_name = "peak";
  const std::string pv_name = "pv";
  const std::string battery_name = "battery";
  const std::string transmission_name = "transmission_line";

  std::vector<std::string> candidate_names;

  SolverAbstract::Ptr math_problem;

  std::shared_ptr<SensitivityILogger> logger;
  std::shared_ptr<SensitivityWriter> writer;

  SensitivityInputData input_data;

 protected:
  void SetUp() override {
#if defined(WIN32) || defined(_WIN32)
    data_test_dir = "../../data_test";
#else
    data_test_dir = "../data_test";
#endif
    data_test_dir += "/sensitivity";

    std::string logger_filename = std::tmpnam(nullptr);
    logger = std::make_shared<SensitivityFileLogger>(logger_filename);

    std::string json_filename = std::tmpnam(nullptr);
    writer = std::make_shared<SensitivityWriter>(json_filename);

    std::string solver_name = "CBC";
    SolverFactory factory;
    math_problem = factory.create_solver(solver_name);
    math_problem->init();
  }

  void prepare_toy_sensitivity_pb() {
    double epsilon = 100;
    double best_ub = 1390;

    candidate_names = {peak_name, semibase_name};

    std::map<std::string, int> name_to_id = {{peak_name, 0},
                                             {semibase_name, 1}};

    std::string last_master_mps_path =
        data_test_dir + "/toy_last_iteration.mps";
    math_problem->read_prob_mps(last_master_mps_path);

    bool capex = true;

    std::map<std::string, std::pair<double, double>> candidates_bounds = {
        {peak_name, {0, 3000}}, {semibase_name, {0, 400}}};

    input_data = {epsilon,           best_ub, name_to_id,     math_problem,
                  candidates_bounds, capex,   candidate_names};
  }

  void prepare_real_sensitivity_pb() {
    double epsilon = 10000;
    double best_ub = 1440683382.5376825;

    candidate_names = {semibase_name, peak_name, pv_name, battery_name,
                       transmission_name};

    std::map<std::string, int> name_to_id = {{semibase_name, 3},
                                             {peak_name, 1},
                                             {pv_name, 2},
                                             {battery_name, 0},
                                             {transmission_name, 4}};

    std::string last_master_mps_path =
        data_test_dir + "/real_last_iteration.mps";
    math_problem->read_prob_mps(last_master_mps_path);

    bool capex = true;

    std::map<std::string, std::pair<double, double>> candidates_bounds = {
        {peak_name, {0, 2000}},
        {semibase_name, {0, 2000}},
        {battery_name, {0, 1000}},
        {pv_name, {0, 1000}},
        {transmission_name, {0, 3200}}};

    input_data = {epsilon,           best_ub, name_to_id,     math_problem,
                  candidates_bounds, capex,   candidate_names};
  }

  bool areEquals(const Point &left, const Point &right) {
    if (left.size() != left.size()) return false;

    for (auto candidate_it = left.begin(),
              expec_candidate_it = right.begin();
         candidate_it != left.end(),
              expec_candidate_it != right.end();
         candidate_it++, expec_candidate_it++) {
      if (candidate_it->first != expec_candidate_it->first) return false;
      if ( fabs(candidate_it->second - expec_candidate_it->second) > 1e-6) return false;
    }
    return true;
  }

  bool areEquals(const SinglePbData& left, const SinglePbData& right) {
    return left.pb_type == right.pb_type &&
    left.str_pb_type == right.str_pb_type &&
    left.candidate_name ==
              right.candidate_name &&
    left.opt_dir == right.opt_dir &&
    fabs(left.objective - right.objective) < 1e-6 &&
    fabs(left.system_cost - right.system_cost) < 1e-6 &&
    left.solver_status == right.solver_status &&
    areEquals(left.candidates, right.candidates);
  }

  void verify_output_data(const SensitivityOutputData &output_data,
                          SensitivityOutputData expec_output_data) {
    EXPECT_DOUBLE_EQ(output_data.epsilon, expec_output_data.epsilon);
    EXPECT_DOUBLE_EQ(output_data.best_benders_cost,
                     expec_output_data.best_benders_cost);
    EXPECT_EQ(output_data.candidates_bounds,
              expec_output_data.candidates_bounds);
    ASSERT_EQ(output_data.pbs_data.size(), expec_output_data.pbs_data.size());

    for (auto leftMatch : output_data.pbs_data) {
      auto rightMatch = std::find_if(expec_output_data.pbs_data.begin(), expec_output_data.pbs_data.end(), [&leftMatch, this](const SinglePbData& data){
        return areEquals(leftMatch, data);
      });
      ASSERT_NE(rightMatch, expec_output_data.pbs_data.end());
      expec_output_data.pbs_data.erase(rightMatch);
    }
    ASSERT_EQ(expec_output_data.pbs_data.size(), 0);
  }

  void verify_single_pb_data(const SinglePbData &single_pb_data,
                             const SinglePbData &expec_single_pb_data) {
    EXPECT_EQ(single_pb_data.pb_type, expec_single_pb_data.pb_type);
    EXPECT_EQ(single_pb_data.str_pb_type, expec_single_pb_data.str_pb_type);
    EXPECT_EQ(single_pb_data.candidate_name,
              expec_single_pb_data.candidate_name);
    EXPECT_EQ(single_pb_data.opt_dir, expec_single_pb_data.opt_dir);
    EXPECT_NEAR(single_pb_data.objective, expec_single_pb_data.objective, 1e-6);
    EXPECT_NEAR(single_pb_data.system_cost, expec_single_pb_data.system_cost,
                1e-6);
    EXPECT_EQ(single_pb_data.solver_status, expec_single_pb_data.solver_status);

    verify_candidates(single_pb_data.candidates,
                      expec_single_pb_data.candidates);
  }

  void verify_candidates(const Point &candidates,
                         const Point &expec_candidates) {
    ASSERT_EQ(candidates.size(), expec_candidates.size());

    for (auto candidate_it = candidates.begin(),
              expec_candidate_it = expec_candidates.begin();
         candidate_it != candidates.end(),
              expec_candidate_it != expec_candidates.end();
         candidate_it++, expec_candidate_it++) {
      EXPECT_EQ(candidate_it->first, expec_candidate_it->first);
      EXPECT_NEAR(candidate_it->second, expec_candidate_it->second, 1e-6);
    }
  }
};

TEST_F(SensitivityStudyTest, OutputDataInit) {
  prepare_toy_sensitivity_pb();

  auto sensitivity_study = SensitivityStudy(input_data, logger, writer);
  auto expec_output_data = SensitivityOutputData(
      input_data.epsilon, input_data.best_ub, input_data.candidates_bounds);
  auto output_data = sensitivity_study.get_output_data();

  verify_output_data(output_data, expec_output_data);
}

TEST_F(SensitivityStudyTest, GetCapexSolutions) {
  prepare_toy_sensitivity_pb();

  input_data.capex = true;
  input_data.projection = {};
  auto sensitivity_study = SensitivityStudy(input_data, logger, writer);
  sensitivity_study.launch();

  auto output_data = sensitivity_study.get_output_data();

  auto capex_min_data = SinglePbData(
      SensitivityPbType::CAPEX, CAPEX_C, "", MIN_C, 1040, 1390,
      {{peak_name, 14}, {semibase_name, 10}}, SOLVER_STATUS::OPTIMAL);

  auto capex_max_data = SinglePbData(
      SensitivityPbType::CAPEX, CAPEX_C, "", MAX_C, 1224.745762711, 1490,
      {{peak_name, 17.22033898}, {semibase_name, 11.69491525}},
      SOLVER_STATUS::OPTIMAL);

  std::vector<SinglePbData> pbs_data = {capex_min_data, capex_max_data};
  auto expec_output_data =
      SensitivityOutputData(input_data.epsilon, input_data.best_ub,
                            input_data.candidates_bounds, pbs_data);

  verify_output_data(output_data, expec_output_data);
}

TEST_F(SensitivityStudyTest, GetCandidatesProjection) {
  prepare_toy_sensitivity_pb();

  input_data.capex = false;
  input_data.projection = candidate_names;
  auto sensitivity_study = SensitivityStudy(input_data, logger, writer);
  sensitivity_study.launch();

  auto output_data = sensitivity_study.get_output_data();

  auto projection_min_peak =
      SinglePbData(SensitivityPbType::PROJECTION, PROJECTION_C, peak_name,
                   MIN_C, 13.83050847, 1490,
                   {{peak_name, 13.83050847}, {semibase_name, 11.694915254}},
                   SOLVER_STATUS::OPTIMAL);

  auto projection_max_peak = SinglePbData(
      SensitivityPbType::PROJECTION, PROJECTION_C, peak_name, MAX_C, 24, 1490,
      {{peak_name, 24}, {semibase_name, 10}}, SOLVER_STATUS::OPTIMAL);

  auto projection_min_semibase = SinglePbData(
      SensitivityPbType::PROJECTION, PROJECTION_C, semibase_name, MIN_C, 10,
      1390, {{peak_name, 14}, {semibase_name, 10}}, SOLVER_STATUS::OPTIMAL);

  auto projection_max_semibase =
      SinglePbData(SensitivityPbType::PROJECTION, PROJECTION_C, semibase_name,
                   MAX_C, 11.694915254, 1490,
                   {{peak_name, 13.83050847}, {semibase_name, 11.694915254}},
                   SOLVER_STATUS::OPTIMAL);

  std::vector<SinglePbData> pbs_data = {
      projection_min_peak, projection_max_peak, projection_min_semibase,
      projection_max_semibase};

  auto expec_output_data =
      SensitivityOutputData(input_data.epsilon, input_data.best_ub,
                            input_data.candidates_bounds, pbs_data);

  verify_output_data(output_data, expec_output_data);
}

TEST_F(SensitivityStudyTest, FullSensitivityTest) {
  prepare_real_sensitivity_pb();

  input_data.capex = true;
  input_data.projection = candidate_names;
  SensitivityStudy sensitivity_study =
      SensitivityStudy(input_data, logger, writer);
  sensitivity_study.launch();

  auto output_data = sensitivity_study.get_output_data();

  auto capex_min_data =
      SinglePbData(SensitivityPbType::CAPEX, CAPEX_C, "", MIN_C,
                   299860860.09420657, 1440693382.5376828,
                   {{battery_name, 511.01433490373716},
                    {peak_name, 1500},
                    {pv_name, 0},
                    {semibase_name, 1200},
                    {transmission_name, 2800}},
                   SOLVER_STATUS::OPTIMAL);

  auto capex_max_data =
      SinglePbData(SensitivityPbType::CAPEX, CAPEX_C, "", MAX_C,
                   300394980.47320199, 1440693382.5376825,
                   {{battery_name, 519.91634122019002},
                    {peak_name, 1500},
                    {pv_name, 0},
                    {semibase_name, 1200},
                    {transmission_name, 2800}},
                   SOLVER_STATUS::OPTIMAL);

  auto projection_min_semibase =
      SinglePbData(SensitivityPbType::PROJECTION, PROJECTION_C, semibase_name,
                   MIN_C, 1200, 1440693382.5376825,
                   {{battery_name, 519.91634122019059},
                    {peak_name, 1500},
                    {pv_name, 0},
                    {semibase_name, 1200},
                    {transmission_name, 2800}},
                   SOLVER_STATUS::OPTIMAL);

  auto projection_max_semibase =
      SinglePbData(SensitivityPbType::PROJECTION, PROJECTION_C, semibase_name,
                   MAX_C, 1200, 1440693382.5376825,
                   {{battery_name, 519.91634122015228},
                    {peak_name, 1500},
                    {pv_name, 0},
                    {semibase_name, 1200},
                    {transmission_name, 2800}},
                   SOLVER_STATUS::OPTIMAL);

  auto projection_min_peak =
      SinglePbData(SensitivityPbType::PROJECTION, PROJECTION_C, peak_name,
                   MIN_C, 1500, 1440693382.5376825,
                   {{battery_name, 511.01433490344891},
                    {peak_name, 1500},
                    {pv_name, 0},
                    {semibase_name, 1200},
                    {transmission_name, 2800}},
                   SOLVER_STATUS::OPTIMAL);

  auto projection_max_peak =
      SinglePbData(SensitivityPbType::PROJECTION, PROJECTION_C, peak_name,
                   MAX_C, 1500, 1440693382.5376825,
                   {{battery_name, 519.91634122015932},
                    {peak_name, 1500},
                    {pv_name, 0},
                    {semibase_name, 1200},
                    {transmission_name, 2800}},
                   SOLVER_STATUS::OPTIMAL);

  auto projection_min_pv =
      SinglePbData(SensitivityPbType::PROJECTION, PROJECTION_C, pv_name, MIN_C,
                   0, 1440693382.5376825,
                   {{battery_name, 519.9163412201425},
                    {peak_name, 1500},
                    {pv_name, 0},
                    {semibase_name, 1200},
                    {transmission_name, 2800}},
                   SOLVER_STATUS::OPTIMAL);

  auto projection_max_pv =
      SinglePbData(SensitivityPbType::PROJECTION, PROJECTION_C, pv_name, MAX_C,
                   1.2427901705828661, 1440693382.5376825,
                   {{battery_name, 517.99999999999875},
                    {peak_name, 1500},
                    {pv_name, 1.2427901706122959},
                    {semibase_name, 1200},
                    {transmission_name, 2800}},
                   SOLVER_STATUS::OPTIMAL);

  auto projection_min_battery =
      SinglePbData(SensitivityPbType::PROJECTION, PROJECTION_C, battery_name,
                   MIN_C, 511.01433490356368, 1440693382.5376825,
                   {{battery_name, 511.01433490356368},
                    {peak_name, 1500},
                    {pv_name, 0},
                    {semibase_name, 1200},
                    {transmission_name, 2800}},
                   SOLVER_STATUS::OPTIMAL);

  auto projection_max_battery =
      SinglePbData(SensitivityPbType::PROJECTION, PROJECTION_C, battery_name,
                   MAX_C, 519.91634122009395, 1440693382.5376825,
                   {{battery_name, 519.91634122009395},
                    {peak_name, 1500},
                    {pv_name, 0},
                    {semibase_name, 1200},
                    {transmission_name, 2800}},
                   SOLVER_STATUS::OPTIMAL);

  auto projection_min_transmission =
      SinglePbData(SensitivityPbType::PROJECTION, PROJECTION_C,
                   transmission_name, MIN_C, 2800, 1440693382.5376825,
                   {{battery_name, 519.91634122015114},
                    {peak_name, 1500},
                    {pv_name, 0},
                    {semibase_name, 1200},
                    {transmission_name, 2800}},
                   SOLVER_STATUS::OPTIMAL);

  auto projection_max_transmission =
      SinglePbData(SensitivityPbType::PROJECTION, PROJECTION_C,
                   transmission_name, MAX_C, 2800, 1440683382.5376825,
                   {{battery_name, 517.99999999999739},
                    {peak_name, 1500},
                    {pv_name, 0},
                    {semibase_name, 1200},
                    {transmission_name, 2800}},
                   SOLVER_STATUS::OPTIMAL);

  std::vector<SinglePbData> pbs_data = {capex_min_data,
                                        capex_max_data,
                                        projection_min_semibase,
                                        projection_max_semibase,
                                        projection_min_peak,
                                        projection_max_peak,
                                        projection_min_pv,
                                        projection_max_pv,
                                        projection_min_battery,
                                        projection_max_battery,
                                        projection_min_transmission,
                                        projection_max_transmission};
  auto expec_output_data =
      SensitivityOutputData(input_data.epsilon, input_data.best_ub,
                            input_data.candidates_bounds, pbs_data);

  verify_output_data(output_data, expec_output_data);
}