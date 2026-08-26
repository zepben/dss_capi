import ctypes
import os
import unittest
from pathlib import Path


LIBRARY = os.environ.get("DSS_CAPI_LIBRARY")


@unittest.skipUnless(LIBRARY and Path(LIBRARY).is_file(), "set DSS_CAPI_LIBRARY to a built library")
class LinearYearlyRuntimeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.dss = ctypes.CDLL(LIBRARY)
        cls.dss.DSS_Start.argtypes = [ctypes.c_int32]
        cls.dss.DSS_Start.restype = ctypes.c_uint16
        cls.dss.DSS_NewCircuit.argtypes = [ctypes.c_char_p]
        cls.dss.DSS_ClearAll.argtypes = []
        cls.dss.Text_Set_Command.argtypes = [ctypes.c_char_p]
        cls.dss.Error_Get_Number.restype = ctypes.c_int32
        cls.dss.Error_Get_Description.restype = ctypes.c_char_p
        cls.dss.Solution_Set_Mode.argtypes = [ctypes.c_int32]
        cls.dss.Solution_Get_Mode.restype = ctypes.c_int32
        cls.dss.Solution_Get_ModeID.restype = ctypes.c_char_p
        cls.dss.Solution_Get_LoadModel.restype = ctypes.c_int32
        cls.dss.Solution_Set_LoadModel.argtypes = [ctypes.c_int32]
        cls.dss.Solution_Get_Number.restype = ctypes.c_int32
        cls.dss.Solution_Set_Number.argtypes = [ctypes.c_int32]
        cls.dss.Solution_Get_StepSize.restype = ctypes.c_double
        cls.dss.Solution_Set_StepSize.argtypes = [ctypes.c_double]
        cls.dss.Solution_Solve.argtypes = []
        cls.dss.Solution_Get_Hour.restype = ctypes.c_int32
        cls.dss.Solution_Get_Converged.restype = ctypes.c_uint16
        cls.dss.Circuit_Get_AllBusVmag.argtypes = [
            ctypes.POINTER(ctypes.POINTER(ctypes.c_double)),
            ctypes.POINTER(ctypes.c_int32),
        ]
        if not cls.dss.DSS_Start(0):
            raise RuntimeError("DSS engine did not start")

    def setUp(self) -> None:
        self.dss.DSS_ClearAll()
        self.dss.DSS_NewCircuit(b"linear_yearly_test")
        self.command("set mode=snapshot")

    def command(self, value: str) -> None:
        self.dss.Text_Set_Command(value.encode())
        error = self.dss.Error_Get_Number()
        if error:
            description = self.dss.Error_Get_Description().decode(errors="replace")
            self.fail(f"DSS command failed ({error}): {value}\n{description}")

    def bus_magnitudes(self) -> list[float]:
        values = ctypes.POINTER(ctypes.c_double)()
        counts = (ctypes.c_int32 * 4)()
        self.dss.Circuit_Get_AllBusVmag(ctypes.byref(values), counts)
        return [values[index] for index in range(counts[0])]

    def test_mode_defaults_and_load_model_invariant(self) -> None:
        self.dss.Solution_Set_LoadModel(1)
        self.dss.Solution_Set_Mode(18)

        self.assertEqual(18, self.dss.Solution_Get_Mode())
        self.assertEqual(b"LinearYearly", self.dss.Solution_Get_ModeID())
        self.assertEqual(2, self.dss.Solution_Get_LoadModel())
        self.assertEqual(8760, self.dss.Solution_Get_Number())
        self.assertEqual(3600.0, self.dss.Solution_Get_StepSize())

        self.dss.Solution_Set_Mode(2)
        self.assertEqual(1, self.dss.Solution_Get_LoadModel())

        self.dss.Solution_Set_Mode(18)

        self.dss.Solution_Set_LoadModel(1)
        self.assertNotEqual(0, self.dss.Error_Get_Number())
        self.assertEqual(2, self.dss.Solution_Get_LoadModel())

    def test_three_sequential_steps_rebuild_time_dependent_admittance(self) -> None:
        self.command(
            "new line.feeder bus1=sourcebus bus2=loadbus phases=3 "
            "r1=0.5 x1=0.2 r0=0.5 x0=0.2 length=1 units=km"
        )
        self.command("new loadshape.year npts=3 interval=1 mult=(0.5 1.0 1.5)")
        self.command(
            "new load.customer bus1=loadbus.1.2.3 phases=3 conn=wye "
            "kv=12.47 kw=6000 kvar=2000 yearly=year"
        )

        self.dss.Solution_Set_Mode(18)
        self.dss.Solution_Set_Number(1)
        self.dss.Solution_Set_StepSize(3600.0)

        results = []
        for _ in range(3):
            self.dss.Solution_Solve()
            self.assertTrue(self.dss.Solution_Get_Converged())
            results.append(self.bus_magnitudes())

        self.assertEqual(3, self.dss.Solution_Get_Hour())
        self.assertEqual(len(results[0]), len(results[1]))
        self.assertGreater(max(abs(a - b) for a, b in zip(results[0], results[1])), 1e-3)
        self.assertGreater(max(abs(a - b) for a, b in zip(results[1], results[2])), 1e-3)

if __name__ == "__main__":
    unittest.main()
