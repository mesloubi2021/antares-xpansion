import pytest
import os, sys
from pathlib import Path
from unittest.mock import patch

from antares_xpansion.benders_driver import BendersDriver
MOCK_SUBPROCESS_RUN = "antares_xpansion.benders_driver.subprocess.run"
MOCK_SYS = "antares_xpansion.benders_driver.sys"
class TestBendersDriver:


    def setup_method(self):
        
        if sys.platform.startswith("win32"):
            self.MPI_LAUNCHER = "mpiexec"
            self.MPI_N = "-n"
        elif sys.platform.startswith("linux"):
            self.MPI_LAUNCHER = "mpirun"
            self.MPI_N = "-np"


    def test_lp_path(self, tmp_path):
        lp_path = tmp_path / "lp" 
        os.mkdir(lp_path)
        benders_driver = BendersDriver("", "", "")
        benders_driver.set_simulation_output_path(tmp_path)
        assert benders_driver.get_lp_path() == lp_path
    
    def test_non_existing_output_path(self, tmp_path):
        benders_driver = BendersDriver("", "", "")
        with pytest.raises(BendersDriver.BendersOutputPathError):
            benders_driver.launch(tmp_path / "i_dont_exist", "test", False, 13)
    
    def test_empty_output_path(self, tmp_path):
        benders_driver = BendersDriver("", "", "")
        with pytest.raises(BendersDriver.BendersLpPathError):
            benders_driver.launch(tmp_path, "")

    
    def test_illegal_method(self, tmp_path):
        lp_path = tmp_path / "lp" 
        os.mkdir(lp_path)
        benders_driver = BendersDriver("", "", "")
        with pytest.raises(BendersDriver.BendersSolverError):
            benders_driver.launch(tmp_path, "test")
            
                        
    def test_benders_cmd_mpibenders(self, tmp_path):
        my_benders_mpi = "something"
        my_install_dir = Path("Dummy/Path/to/")
        my_n_mpi = 13
        exe_path =  os.path.normpath(os.path.join(my_install_dir, my_benders_mpi))
        
        benders_driver = BendersDriver(exe_path, "", "")

        simulation_output_path = tmp_path
        lp_path =  Path(os.path.normpath(os.path.join(simulation_output_path, 'lp')))
        lp_path.mkdir()
        os.chdir(lp_path)
        with patch(MOCK_SUBPROCESS_RUN, autospec = True) as run_function :
            expected_cmd = [self.MPI_LAUNCHER, self.MPI_N, str(my_n_mpi), exe_path, "options.txt" ]
            run_function.return_value.returncode = 0
            benders_driver.launch(simulation_output_path, "mpibenders", True, my_n_mpi)
            args, _ = run_function.call_args_list[0]
            assert args[0] == expected_cmd
                        
    def test_benders_cmd_sequential(self, tmp_path):
        my_sequential = "something"
        my_install_dir = Path("Dummy/Path/to/")
        exe_path =  os.path.normpath(os.path.join(my_install_dir, my_sequential))

        benders_driver = BendersDriver("", exe_path, "")

        simulation_output_path = tmp_path
        lp_path =  Path(os.path.normpath(os.path.join(simulation_output_path, 'lp')))
        lp_path.mkdir()
        os.chdir(lp_path)
        with patch(MOCK_SUBPROCESS_RUN, autospec = True) as run_function :
            expected_cmd = [exe_path, "options.txt" ]
            run_function.return_value.returncode = 0
            benders_driver.launch(simulation_output_path, "sequential", True)
            args, _ = run_function.call_args_list[0]
            assert args[0] == expected_cmd
                        
    def test_benders_cmd_merge_mps(self, tmp_path):
        my_merges_mps = "something"
        my_install_dir = Path("Dummy/Path/to/")
        exe_path =  os.path.normpath(os.path.join(my_install_dir, my_merges_mps))

        
        benders_driver = BendersDriver("", "", exe_path)

        simulation_output_path = tmp_path
        lp_path =  Path(os.path.normpath(os.path.join(simulation_output_path, 'lp')))
        lp_path.mkdir()
        os.chdir(lp_path)
        with patch(MOCK_SUBPROCESS_RUN, autospec = True) as run_function :
            expected_cmd = [exe_path, "options.txt" ]
            run_function.return_value.returncode = 0
            benders_driver.launch(simulation_output_path, "mergeMPS", True)
            args, _ = run_function.call_args_list[0]
            assert args[0] == expected_cmd


    def test_raise_execution_error(self, tmp_path):

        my_benders_mpi = "something"
        my_install_dir = Path("Dummy/Path/to/")
        my_n_mpi = 13
        exe_path =  os.path.normpath(os.path.join(my_install_dir, my_benders_mpi))
        
        benders_driver = BendersDriver(exe_path, "", "")

        simulation_output_path = tmp_path
        lp_path =  Path(os.path.normpath(os.path.join(simulation_output_path, 'lp')))
        lp_path.mkdir()
        os.chdir(lp_path)
        with patch(MOCK_SUBPROCESS_RUN, autospec = True) :
            with pytest.raises(BendersDriver.BendersExecutionError) :
                benders_driver.launch(simulation_output_path, "mpibenders", True, my_n_mpi)

    
    def test_clean_solver_log_file(self, tmp_path):

        my_benders_mpi = "something"
        my_install_dir = Path("Dummy/Path/to/")
        my_n_mpi = 13
        exe_path =  os.path.normpath(os.path.join(my_install_dir, my_benders_mpi))
        benders_driver = BendersDriver(exe_path, "", "")


        simulation_output_path = tmp_path
        lp_path =  Path(os.path.normpath(os.path.join(simulation_output_path, 'lp')))
        lp_path.mkdir()
        os.chdir(lp_path)

        solver_log_fname_1 = my_benders_mpi +"Log"
        solver_log_fname_2 = my_benders_mpi +".log"
        solver_log_file_1 = lp_path / solver_log_fname_1
        solver_log_file_2 = lp_path / solver_log_fname_2
        solver_log_file_1.write_text("")
        solver_log_file_2.write_text("")
        assert solver_log_file_1.exists()
        assert solver_log_file_2.exists()

        with patch(MOCK_SUBPROCESS_RUN, autospec = True) as run_function:
            run_function.return_value.returncode = 0
            benders_driver.launch(simulation_output_path, "mpibenders", True, my_n_mpi)


        assert not solver_log_file_1.exists()
        assert not solver_log_file_2.exists()
        
    def test_unsupported_platform(self, tmp_path):

        
        with patch(MOCK_SYS, autospec=True) as sys_ :
            sys_.platform = "exotic_platform"
            with pytest.raises(BendersDriver.BendersUnsupportedPlatform):
                BendersDriver("", "", "")
        
    def test_clean_benders_step_if_not_keep_mps(self, tmp_path):
        my_benders_mpi = "something"
        my_install_dir = Path("Dummy/Path/to/")
        exe_path =  os.path.normpath(os.path.join(my_install_dir, my_benders_mpi))
        keep_mps = False
        
        benders_driver = BendersDriver(exe_path, "", "")

        simulation_output_path = tmp_path
        lp_path =  Path(os.path.normpath(os.path.join(simulation_output_path, 'lp')))
        lp_path.mkdir()
        os.chdir(lp_path)

        mps_fnames = ["master.mps", "1.mps","2.mps"]
        mps_files = [self._create_empty_file(lp_path, mps_fname) for mps_fname in mps_fnames ]

        lp_fnames = ["master.lp", "1.lp","2.lp"]
        lp_files = [self._create_empty_file(lp_path, lp_fname) for lp_fname in lp_fnames ]

        benders_files = mps_files+lp_files
        # check that files are created
        for file in benders_files:
            assert file.exists()

        with patch(MOCK_SUBPROCESS_RUN, autospec = True) as run_function :
            run_function.return_value.returncode = 0
            benders_driver.launch(simulation_output_path, "mpibenders", keep_mps)

        
        # check that files are deleted
        for file in benders_files:
            assert not file.exists()
        

    def _create_empty_file(self, tmp_path : Path , fname: str):
        file = tmp_path / fname
        file.write_text("") 
        return file