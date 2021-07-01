
#include <algorithm>
#include <random>

#include "glog/logging.h"

#include "BendersMPI.h"

#define __DEBUG_BENDERS_MPI__ 0

BendersMpi::~BendersMpi() {

}

BendersMpi::BendersMpi(mpi::environment & env, mpi::communicator & world, BendersOptions const & options, Logger &logger):
_options(options),_logger(logger),_exceptionRaised(false) {

}

/*!
*  \brief Method to load each problem in a thread
*
*  The initialization of each problem is done sequentially
*
*  \param problem_list : map linking each problem name to its variables and their id
*
*  \param env : environment variable for mpi communication
*
*  \param world : communicator variable for mpi communication
*/

void BendersMpi::load(CouplingMap const & problem_list, mpi::environment & env, mpi::communicator & world) {
	StrVector names;
	_data.nslaves = -1;
	std::vector<CouplingMap::const_iterator> real_problem_list;
	if (!problem_list.empty()) {
		if (world.rank() == 0) {
			_data.nslaves = _options.SLAVE_NUMBER;
			if (_data.nslaves < 0) {
				_data.nslaves = problem_list.size() - 1;
			}
			std::string const & master_name(_options.MASTER_NAME);
			auto const it_master(problem_list.find(master_name));
			if (it_master == problem_list.end()) {
				std::cout << "UNABLE TO FIND " << master_name << std::endl;
				std::exit(1);
			}
			// real problem list taking into account SLAVE_NUMBER

			real_problem_list.resize(_data.nslaves, problem_list.end());

			CouplingMap::const_iterator it(problem_list.begin());
			for (int i(0); i < _data.nslaves; ++it) {
				if (it != it_master) {
					real_problem_list[i] = it;
					_problem_to_id[it->first] = i;
					++i;
				}
			}
			_master.reset(new WorkerMaster(it_master->second, _options.get_master_path(), _options, _data.nslaves));
			LOG(INFO) << "nrealslaves is " << _data.nslaves << std::endl;
		}
		mpi::broadcast(world, _data.nslaves, 0);
		int current_worker(1);
		for (int islave(0); islave < _data.nslaves; ++islave, ++current_worker) {
			if (current_worker >= world.size()) {
				current_worker = 1;
			}
			if (world.rank() == 0) {
				CouplingMap::value_type kvp(*real_problem_list[islave]);
				world.send(current_worker, islave, kvp);
			}
			else if (world.rank() == current_worker) {
				CouplingMap::value_type kvp;
				world.recv(0, islave, kvp);
				_map_slaves[kvp.first] = WorkerSlavePtr(new WorkerSlave(kvp.second, _options.get_slave_path(kvp.first), _options.slave_weight(_data.nslaves, kvp.first), _options));
				_slaves.push_back(kvp.first);
			}
		}
	}
}

/*!
*  \brief Update the value of options RAND_AGGREGATION according to the number of slaves on each thread
*
*	Update the value of options RAND_AGGREGATION according to the number of slaves on each thread
*
*  \param options : set of Benders options
*
*  \param options : set of Benders data
*
*  \param env : environment variable for mpi communication
*
*  \param world : communicator variable for mpi communication
*/
void BendersMpi::update_random_option(mpi::environment & env, mpi::communicator & world, BendersOptions const & options, BendersData & data) {
	int const n_thread(world.size() - 1);
	int const n_max_slave(_data.nslaves % n_thread);
	int const n_sup_rand(_options.RAND_AGGREGATION % n_thread);
	int const n_slave_by_thread(_data.nslaves / n_thread);
	int const n_rand_slave(_options.RAND_AGGREGATION / n_thread);
	int const n_add_rand(n_thread * !(n_slave_by_thread == n_rand_slave) + n_max_slave* (n_slave_by_thread == n_rand_slave));
	if (_options.RAND_AGGREGATION && world.rank() != 0) {
		_data.nrandom = n_rand_slave;
	}
	if (n_sup_rand) {
		std::set<int> set_rand_slave;
		if (world.rank() == 0) {
			for (int i(0); i < n_sup_rand;) {
				if (set_rand_slave.insert(std::rand() % n_add_rand + 1).second) {
					i++;
				}
			}
		}
		boost::mpi::broadcast(world, set_rand_slave, 0);
		if (set_rand_slave.find(world.rank()) != set_rand_slave.end()) {
			data.nrandom++;
		}
	}
}

/*!
*  \brief Solve, get and send solution of the Master Problem to every thread
*
*  \param env : environment variable for mpi communication
*
*  \param world : communicator variable for mpi communication
*/
void BendersMpi::step_1_solve_master(mpi::environment & env, mpi::communicator & world) {

    int success = 1;
    try {
        do_solve_master_create_trace_and_update_cuts(world.rank());
    }catch(std::exception& ex){
        success = 0;
        write_exception_message(ex);
    }
    check_if_some_proc_had_a_failure(world, success);
    broadcast_the_master_problem(world);
}

void BendersMpi::do_solve_master_create_trace_and_update_cuts(int rank) {
    if (rank == 0){
        solve_master_and_create_trace();
        if (_options.ACTIVECUTS) {
            update_active_cuts(_master, _active_cuts, _slave_cut_id, _data.it);
        }
    }
}

void BendersMpi::broadcast_the_master_problem(const mpi::communicator &world) {
    if(!_exceptionRaised){
        mpi::broadcast(world, _data.x0, 0);
        if (_options.RAND_AGGREGATION) {
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(_slaves.begin(), _slaves.end(), g);
        }
        world.barrier();
    }
}



void BendersMpi::solve_master_and_create_trace() {
    _logger->log_at_initialization(bendersDataToLogData(_data));
    _logger->display_message("\tSolving master...");
    get_master_value(_master, _data, _options);
    _logger->log_master_solving_duration(_data.timer_master);
    _logger->log_iteration_candidates(bendersDataToLogData(_data));

    _trace.push_back(WorkerMasterDataPtr(new WorkerMasterData));
}

/*!
*  \brief Get cut information from each Slave and add it to the Master problem
*
*	Get cut information of every Slave Problem in each thread and send it to thread 0 to build new Master's cuts
*
*  \param env : environment variable for mpi communication
*
*  \param world : communicator variable for mpi communication
*/
void BendersMpi::step_2_build_cuts(mpi::environment & env, mpi::communicator & world) {
    solve_slaves_and_build_cuts(world);
    broadcast_rand_aggregation(world);
}

void BendersMpi::broadcast_rand_aggregation(const mpi::communicator &world) {
    if(!_exceptionRaised) {
        mpi::broadcast(world, _options.RAND_AGGREGATION, 0);
        world.barrier();
    }
}

void BendersMpi::solve_slaves_and_build_cuts(const mpi::communicator &world) {
    int success = 1;
    SlaveCutPackage slave_cut_package;
    Timer timer_slaves;
    try {
        if (world.rank() != 0) {
            slave_cut_package = get_slave_package();
        }
    }catch(std::exception& ex){
        success = 0;
        write_exception_message(ex);
    }
    check_if_some_proc_had_a_failure(world,success);
    gather_slave_cut_package_and_build_cuts(world, slave_cut_package, timer_slaves);
}

void BendersMpi::gather_slave_cut_package_and_build_cuts(const mpi::communicator &world,const SlaveCutPackage& slave_cut_package ,const Timer& timer_slaves)
{
    if (!_exceptionRaised) {
        if (world.rank() != 0){
            mpi::gather(world, slave_cut_package, 0);
        }else{
            AllCutPackage all_package;
            mpi::gather(world, slave_cut_package, all_package, 0);
            _data.timer_slaves = timer_slaves.elapsed();
            master_build_cuts(all_package);
        }
    }
}

SlaveCutPackage BendersMpi::get_slave_package()  {
    SlaveCutPackage slave_cut_package;
    if (_options.RAND_AGGREGATION) {
        get_random_slave_cut(slave_cut_package, _map_slaves, _slaves, _options, _data);
    }
    else {
        get_slave_cut(slave_cut_package, _map_slaves, _options, _data);
    }
    return slave_cut_package;
}

void BendersMpi::master_build_cuts(AllCutPackage all_package) {

    _data.slave_cost = 0;
    for (auto const& pack : all_package) {
        for (auto& dataVal : pack) {
            _data.slave_cost += dataVal.second.first.second[SLAVE_COST];
        }
    }
    all_package.erase(all_package.begin());

    _logger->display_message("\tSolving subproblems...");

    build_cut_full(_master, all_package, _problem_to_id, _trace, _slave_cut_id, _all_cuts_storage,
                   _dynamic_aggregate_cuts, _data, _options);
    _logger->log_subproblems_solving_duration(_data.timer_slaves);
}

/*!
*  \brief Gather, store and sort all slaves basis in a set
*
*  \param env : environment variable for mpi communication
*
*  \param world : communicator variable for mpi communication
*/

void BendersMpi::check_if_some_proc_had_a_failure(const mpi::communicator &world, int success) {
    int global_success;
    mpi::all_reduce(world, success, global_success, mpi::bitwise_and<int>());
    if (global_success == 0){
        _exceptionRaised = true;
    }
}

void BendersMpi::write_exception_message(const std::exception &ex) {
    std::string error = "Exception raised : " + std::string(ex.what());
    LOG(WARNING) << error << std::endl;
    if(_logger){
        _logger->display_message(error);
    }
}

void BendersMpi::step_3_gather_slaves_basis(mpi::environment & env, mpi::communicator & world) {

	SimplexBasisPackage slave_basis_package;
	if (world.rank() == 0) {
		AllBasisPackage all_basis_package;
		mpi::gather(world, slave_basis_package, all_basis_package, 0);
		all_basis_package.erase(all_basis_package.begin());
		sort_basis(all_basis_package, _problem_to_id, _basis, _data);
	}
	else {
		get_slave_basis(slave_basis_package, _map_slaves);
		mpi::gather(world, slave_basis_package, 0);
	}
	world.barrier();
}


void BendersMpi::step_4_update_best_solution(int rank, const Timer& timer_master){
    if (rank == 0) {
        update_best_ub(_data.best_ub, _data.ub, _data.bestx, _data.x0, _data.best_it, _data.it);
        _logger->log_at_iteration_end(bendersDataToLogData(_data));

        update_trace(_trace, _data);

        _data.timer_master = timer_master.elapsed();
        _data.stop = stopping_criterion(_data,_options);
    }
}

/*!
*  \brief Method to free the memory used by each problem
*
*  \param env : environment variable for mpi communication
*
*  \param world : communicator variable for mpi communication
*/
void BendersMpi::free(mpi::environment & env, mpi::communicator & world) {
	if (world.rank() == 0)
		_master->free();
	else {
		for (auto & ptr : _map_slaves)
			ptr.second->free();
	}
	world.barrier();
}

/*!
*  \brief Run Benders algorithm in parallel
*
*  Method to run Benders algorithm in parallel
*
*  \param env : environment variable for mpi communication
*
*  \param world : communicator variable for mpi communication
*/
void BendersMpi::run(mpi::environment & env, mpi::communicator & world) {
	if (world.rank() == 0) {
		for (auto const & kvp : _problem_to_id) {
			_all_cuts_storage[kvp.first] = SlaveCutStorage();
		}
	}
	init(_data);
	world.barrier();

	while (!_data.stop) {
		Timer timer_master;
		update_random_option(env, world, _options, _data);
		++_data.it;
		_data.deletedcut = 0;

		/*Solve Master problem, get optimal value and cost and send it to Slaves*/
		step_1_solve_master(env, world);

		/*Gather cut from each slave in master thread and add them to Master problem*/
		if (!_exceptionRaised) {
            step_2_build_cuts(env, world);
        }

		if (_options.BASIS && !_exceptionRaised) {
            step_3_gather_slaves_basis(env, world);
		}

        if (!_exceptionRaised) {
            step_4_update_best_solution(world.rank(), timer_master);
        }
        _data.stop |= _exceptionRaised;

		broadcast(world, _data.stop, 0);
		world.barrier();
	}

	if (world.rank() == 0) {
		if (_options.TRACE) {
			print_csv(_trace,_problem_to_id,_data,_options);
		}
		if (_options.ACTIVECUTS) {
			print_active_cut(_active_cuts, _options);
		}
	}
}