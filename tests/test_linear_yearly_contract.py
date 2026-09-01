import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def source(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


class LinearYearlyContractTests(unittest.TestCase):
    def test_public_enums_append_value_18(self) -> None:
        self.assertIn("LINEARYEARLYMODE = 18", source("src/Shared/Dynamics.pas"))
        self.assertIn("SolveModes_LinearYearly = 18", source("include/dss_capi.h"))
        self.assertIn("SOLUTION_LINEARYEARLYMODE = 18", source("include/dss_UserModels.h"))
        self.assertIn("LinearYearly = 18", source("include/dss_obj.hpp"))
        self.assertIn("dssLinearYearly = $00000012", source("src/CAPI/CAPI_Constants.pas"))

    def test_public_headers_compile_with_the_new_enum(self) -> None:
        compiler = shutil.which("clang++") or shutil.which("g++")
        if compiler is None:
            self.skipTest("no C++ compiler available")

        translation_unit = """
#include "dss_capi.h"
#include "dss_UserModels.h"
static_assert(SolveModes_LinearYearly == 18, "classic C enum changed");
static_assert(SOLUTION_LINEARYEARLYMODE == 18, "user-model enum changed");
int main() { return 0; }
"""
        with tempfile.TemporaryDirectory() as tmp:
            test_file = Path(tmp) / "linear_yearly_headers.cpp"
            test_file.write_text(translation_unit, encoding="utf-8")
            subprocess.run(
                [compiler, "-std=c++17", "-fsyntax-only", "-I", str(ROOT / "include"), str(test_file)],
                check=True,
                capture_output=True,
                text=True,
            )

    def test_mode_parser_dispatch_and_defaults_are_wired(self) -> None:
        dss_class = source("src/Common/DSSClass.pas")
        solution = source("src/Common/Solution.pas")

        self.assertIn("'LinearYearly'", dss_class)
        self.assertIn("Ord(TSolveMode.LINEARYEARLYMODE)", dss_class)
        self.assertGreaterEqual(solution.count("TSolveMode.LINEARYEARLYMODE"), 4)
        self.assertIn("TSolveMode.LINEARYEARLYMODE:\n                    SolveYearly", solution)
        self.assertIn("TSolveMode.LINEARYEARLYMODE:\n                                    SolveYearly", solution)
        self.assertIn("LoadModel := ADMITTANCE", solution)

    def test_all_yearly_aware_models_recognize_linear_yearly(self) -> None:
        yearly_files = [
            "src/PCElements/Load.pas",
            "src/PCElements/generator.pas",
            "src/PCElements/PVsystem.pas",
            "src/PCElements/Storage.pas",
            "src/PCElements/IndMach012.pas",
            "src/PCElements/VSource.pas",
            "src/PCElements/Isource.pas",
            "src/Controls/StorageController.pas",
        ]
        for path in yearly_files:
            with self.subTest(path=path):
                self.assertIn("TSolveMode.LINEARYEARLYMODE", source(path))

    def test_load_model_validation_is_shared_by_text_and_c_api(self) -> None:
        solution = source("src/Common/Solution.pas")
        exec_options = source("src/Executive/ExecOptions.pas")
        capi_solution = source("src/CAPI/CAPI_Solution.pas")

        self.assertIn("procedure Set_LoadModel", solution)
        self.assertIn("Set_LoadModel(DSS.DefaultLoadModelEnum.StringToOrdinal(Param))", exec_options)
        self.assertIn("Set_LoadModel(Value)", capi_solution)

    def test_direct_solve_consumes_refresh_and_reports_failure(self) -> None:
        solution = source("src/Common/Solution.pas")

        self.assertIn("function SolveDirect(ForcePCRefresh: Boolean = TRUE): Integer", solution)
        self.assertIn("SolveDirect(FALSE)", solution)
        self.assertIn("InvalidateAllPCElements", solution)
        self.assertIn("LoadsNeedUpdating := FALSE", solution)
        self.assertIn("ConvergedFlag := FALSE", solution)
        self.assertIn("DSS.ActiveCircuit.IsSolved := FALSE", solution)
        self.assertIn("if Result <> 1 then", solution)

    def test_repeat_steps_use_existing_incremental_y_infrastructure(self) -> None:
        circuit = source("src/Common/Circuit.pas")
        ckt_element = source("src/Common/CktElement.pas")
        solution = source("src/Common/Solution.pas")
        ymatrix = source("src/Common/Ymatrix.pas")

        self.assertIn("QueueAllPCElementsForIncrementalY", circuit)
        self.assertIn("IncrCktElements.Add(p)", circuit)
        self.assertIn("QueueAllPCElementsForIncrementalY", solution)
        self.assertIn("EffectiveSolverOptions", ymatrix)
        self.assertIn("ReuseSymbolicFactorization", ymatrix)
        self.assertIn("DirectIncremental", ymatrix)
        self.assertIn("IncrementMatrixElement", ymatrix)
        self.assertIn("Mode = TSolveMode.LINEARYEARLYMODE", ckt_element)

    def test_repeat_steps_reuse_solution_arrays_when_nodes_are_stable(self) -> None:
        solution = source("src/Common/Solution.pas")

        self.assertIn("AllocateVI := (NodeV = NIL) or DSS.ActiveCircuit.BusNameRedefined", solution)
        self.assertIn("BuildYMatrix(DSS, WHOLEMATRIX, AllocateVI)", solution)

    def test_failed_direct_yearly_step_is_not_sampled(self) -> None:
        yearly = source("src/Common/SolutionAlgs.pas")
        solve_pos = yearly.index("SolveSnap;")
        monitor_pos = yearly.index("DSS.MonitorClass.SampleAll", solve_pos)
        guard = yearly[solve_pos:monitor_pos]

        self.assertIn("DSS.SolutionAbort", guard)
        self.assertIn("ConvergedFlag", guard)
        self.assertIn("Break", guard)

    def test_indmach_refreshes_nominal_power_before_y_primitive(self) -> None:
        indmach = source("src/PCElements/IndMach012.pas")
        calc_start = indmach.index("procedure TIndMach012Obj.CalcYPrim;")
        calc_end = indmach.index("procedure TIndMach012Obj.DoIndMach012Model", calc_start)
        calc_y_prim = indmach[calc_start:calc_end]

        nominal_pos = calc_y_prim.index("SetNominalPower;")
        matrix_pos = calc_y_prim.index("CalcYPrimMatrix(YPrim_Shunt);")
        self.assertLess(nominal_pos, matrix_pos)

    def test_storage_controller_warns_only_when_redispatch_is_requested(self) -> None:
        controller = source("src/Controls/StorageController.pas")

        self.assertIn("WarnLinearYearlyRedispatch", controller)
        self.assertIn("LinearYearlyUnsupportedWarningIssued", controller)
        self.assertIn("SetFleetChargeRate", controller)
        self.assertIn("SetFleetkWRate", controller)

    def test_yearly_loop_is_not_duplicated(self) -> None:
        self.assertNotIn("SolveLinearYearly", source("src/Common/SolutionAlgs.pas"))


if __name__ == "__main__":
    unittest.main()
