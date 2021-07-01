// projet_benders.cpp�: d�finit le point d'entr�e pour l'application console.
//

#include "glog/logging.h"
#include "gflags/gflags.h"

#include "Worker.h"
#include "Timer.h"
#include "benders_sequential_core/Benders.h"
#include "BendersMPI.h"
#include "launcher.h"
#include "JsonWriter.h"
#include "logger/User.h"

#if defined(WIN32) || defined(_WIN32)
#include <direct.h>
#define GetCurrentDir _getcwd
#else
#include <unistd.h>
#define GetCurrentDir getcwd
#endif

int main(int argc, char** argv)
{
	mpi::environment env(argc, argv);
	mpi::communicator world;

    Logger logger = std::make_shared<xpansion::logger::User>(std::cout);

	// First check usage (options are given)
	if (world.rank() == 0)
	{
		usage(argc);
	}

	// Read options, needed to have options.OUTPUTROOT
	BendersOptions options(build_benders_options(argc, argv));

    gflags::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);
    std::string path_to_log = options.OUTPUTROOT + PATH_SEPARATOR + "bendersmpiLog-rank" + std::to_string(world.rank()) + "-";
    google::SetLogDestination(google::GLOG_INFO, path_to_log.c_str());

    JsonWriter jsonWriter_l;

	if (world.rank() == 0)
	{
	    LOG(INFO) << "starting bendersmpi" << std::endl;
        jsonWriter_l.write_failure();
        jsonWriter_l.dump(options.OUTPUTROOT + PATH_SEPARATOR + options.JSON_NAME + ".json");
	}

	if (world.rank() > options.SLAVE_NUMBER + 1 && options.SLAVE_NUMBER != -1) {
		std::cout << "You need to have at least one slave by thread" << std::endl;
		exit(1);
	}
	if (world.rank() == 0) {
		std::ostringstream oss_l;
		options.print(oss_l);
		LOG(INFO) << oss_l.str() << std::endl;

		jsonWriter_l.write(options);
		jsonWriter_l.updateBeginTime();

	}

	world.barrier();
    if (world.size() == 1) {
        std::cout << "Sequential launch" << std::endl;
        LOG(INFO) << "Size is 1. Launching in sequential mode..." << std::endl;

        sequential_launch(options, logger);
    }
    else {
        Timer timer;
        CouplingMap input = build_input(options);
        world.barrier();

        BendersMpi bendersMpi(env, world, options,logger);
        bendersMpi.load(input, env, world);
        world.barrier();

        bendersMpi.run(env, world);
        world.barrier();

        if (world.rank() == 0) {

            LogData logData = defineLogDataFromBendersDataAndTrace(bendersMpi._data, bendersMpi._trace);
            logData.optimal_gap = options.GAP;

            logger->log_at_ending(logData);

            jsonWriter_l.updateEndTime();
            jsonWriter_l.write(input.size(), bendersMpi._trace, bendersMpi._data,options.GAP);
            jsonWriter_l.dump(options.OUTPUTROOT + PATH_SEPARATOR + options.JSON_NAME + ".json");

            char buff[FILENAME_MAX];
            GetCurrentDir(buff, FILENAME_MAX);

            std::stringstream str;
            str << "Optimization results available in : " << buff << PATH_SEPARATOR
                << options.OUTPUTROOT + PATH_SEPARATOR + options.JSON_NAME + ".json";
            logger->display_message(str.str());
        }
        bendersMpi.free(env, world);
        world.barrier();

        if (world.rank() == 0) {
            logger->log_total_duration(timer.elapsed());
            jsonWriter_l.updateEndTime();
        }
    }
  
	return(0);
}