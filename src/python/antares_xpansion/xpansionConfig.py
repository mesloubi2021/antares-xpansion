"""
    parameters for an AntaresXpansion session
"""

from pathlib import Path
import os
import sys
import yaml
import argparse


class XpansionConfig():
    """
        Class defininf the parameters for an AntaresXpansion session
    """

    # pylint: disable=too-many-instance-attributes
    # pylint: disable=too-few-public-methods

    def __init__(self, configfile: Path):

        self.ANTARES: str = ""
        self.MERGE_MPS: str = ""
        self.BENDERS_MPI: str = ""
        self.BENDERS_SEQUENTIAL: str = ""
        self.LP_NAMER: str = ""
        self.STUDY_UPDATER: str = ""
        self.MPI_LAUNCHER: str = ""
        self.MPI_N: str = ""
        self.AVAILABLE_SOLVER: list[str] = {}

        self._initialize_values_from_config_file(configfile)

        self._initialise_system_specific_mpi_vars()

        self._get_parameters_from_arguments()

        self._initialize_default_values()

    def _get_parameters_from_arguments(self):
        parser = argparse.ArgumentParser()
        parser.add_argument("--step",
                            dest="step",
                            choices=["full", "antares", "getnames", "lp", "optim", "update"],
                            help='Step to execute ("full", "antares", "getnames", "lp", "optim", "update")',
                            default="full")
        parser.add_argument("--simulationName",
                            dest="simulationName",
                            help="Name of the antares simulation to use. Must be present in the output directory")
        parser.add_argument("-i", "--dataDir",
                            dest="dataDir",
                            help="Antares study data directory",
                            required=True)
        parser.add_argument("--installDir",
                            dest="installDir",
                            help="The directory where all binaries are located",
                            default=None)
        parser.add_argument("-m", "--method",
                            dest="method",
                            type=str,
                            choices=["mpibenders", "mergeMPS", "sequential"],
                            help="Choose the optimization method",
                            default="sequential")
        parser.add_argument("-n", "--np",
                            dest="n_mpi",
                            default=4,
                            type=lambda x: (int(x) > 1) and int(x) or sys.exit("Minimum of MPI processes is 1"),
                            help='Number of MPI processes')

        parser.add_argument("--keepMps",
                            dest="keep_mps",
                            default=False,
                            action='store_true',
                            help='Keep .mps from lp_namer and benders steps')

        args = parser.parse_args()
        self.step = args.step
        self.simulationName = args.simulationName
        self.dataDir = str(Path(args.dataDir).absolute())
        self.installDir = self._get_install_dir(args.installDir)
        self.method = args.method
        self.n_mpi = args.n_mpi
        self.keep_mps = args.keep_mps

    def _get_install_dir(self, install_dir):
        if install_dir is None:
            return self._initialize_install_dir_with_default_value()
        else:
            return self._initialize_install_dir_with_absolute_path(install_dir)

    @staticmethod
    def _initialize_install_dir_with_absolute_path(install_dir):
        if not Path.is_absolute(Path(install_dir)):
            return os.path.join(Path.cwd(), Path(install_dir))
        else:
            return install_dir

    @staticmethod
    def _initialize_install_dir_with_default_value():
        install_dir_inside_package = Path(os.path.abspath(__file__)).parent.parent / "bin"
        install_dir_from_absolute_path = Path.cwd() / "bin"
        if Path.is_dir(install_dir_inside_package):
            return install_dir_inside_package
        else:
            return install_dir_from_absolute_path

    def _initialize_default_values(self):
        self._set_constants()
        self._set_default_options()
        self._set_default_settings()

    def _set_constants(self):
        self.SETTINGS = 'settings'
        self.USER = 'user'
        self.EXPANSION = 'expansion'
        self.CAPADIR = 'capa'
        self.GENERAL_DATA_INI = 'generaldata.ini'
        self.NB_YEARS = 'nbyears'
        self.SETTINGS_INI = 'settings.ini'
        self.CANDIDATES_INI = 'candidates.ini'
        self.UC_TYPE = 'uc_type'
        self.EXPANSION_ACCURATE = 'expansion_accurate'
        self.EXPANSION_FAST = 'expansion_fast'
        self.OPTIMIZATION = 'optimization'
        self.EXPORT_STRUCTURE = 'include-exportstructure'
        self.EXPORT_MPS = 'include-exportmps'
        self.TRACE = 'include-trace'
        self.USE_XPRS = 'include-usexprs'
        self.INBASIS = 'include-inbasis'
        self.OUTBASIS = 'include-outbasis'
        self.OUTPUT = 'output'
        self.OPTIONS_TXT = 'options.txt'
        self.MPS_TXT = "mps.txt"

    def _set_default_settings(self):
        self.settings_default = {'method': 'benders_decomposition',
                                 'uc_type': 'expansion_fast',
                                 'master': 'integer',
                                 'optimality_gap': '0',
                                 'cut_type': 'yearly',
                                 'week_selection': 'false',
                                 'max_iteration': '+infini',
                                 'relaxed_optimality_gap': '0.01',
                                 'solver': 'Cbc',
                                 'timelimit': '+infini',
                                 'additional-constraints': "",
                                 'yearly-weights': ""}

    def _set_default_options(self):
        self.options_default = {
            'LOG_LEVEL': '0',
            'MAX_ITERATIONS': '-1',
            'GAP': '1e-06',
            'AGGREGATION': '0',
            'OUTPUTROOT': '.',
            'TRACE': '1',
            'DELETE_CUT': '0',
            'SLAVE_WEIGHT': 'CONSTANT',
            'SLAVE_WEIGHT_VALUE': '1',
            'MASTER_NAME': 'master',
            'SLAVE_NUMBER': '-1',
            'STRUCTURE_FILE': 'structure.txt',
            'INPUTROOT': '.',
            'BASIS': '1',
            'ACTIVECUTS': '0',
            'THRESHOLD_AGGREGATION': '0',
            'THRESHOLD_ITERATION': '0',
            'RAND_AGGREGATION': '0',
            'CSV_NAME': 'benders_output_trace',
            'JSON_NAME': 'out',
            'BOUND_ALPHA': '1',
        }

    def _initialise_system_specific_mpi_vars(self):
        if sys.platform.startswith("win32"):
            self.MPI_LAUNCHER = "mpiexec"
            self.MPI_N = "-n"
        elif sys.platform.startswith("linux"):
            self.MPI_LAUNCHER = "mpirun"
            self.MPI_N = "-np"
        else:
            print("WARN: No mpi launcher was defined!")

    def _initialize_values_from_config_file(self, configfile):
        with open(configfile) as file:
            content = yaml.full_load(file)
            if content is not None:
                self.ANTARES = content.get('ANTARES', "antares-solver")
                self.MERGE_MPS = content.get('MERGE_MPS', "merge_mps")
                self.BENDERS_MPI = content.get('BENDERS_MPI', "bender_mpi")
                self.BENDERS_SEQUENTIAL = content.get('BENDERS_SEQUENTIAL', "benders_sequential")
                self.LP_NAMER = content.get('LP_NAMER', "lp_namer")
                self.STUDY_UPDATER = content.get('STUDY_UPDATER', "study_updater")
                self.AVAILABLE_SOLVER = content.get('AVAILABLE_SOLVER')
            else:
                raise RuntimeError("Please check file config.yaml, content is empty")