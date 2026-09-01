# LinearYearly solution mode

`LinearYearly` runs the normal Yearly time-series loop while solving every time step as a direct nodal-admittance problem. It is intended for fast annual studies where a constant-admittance approximation is acceptable.

## Usage

From a DSS script:

```text
set mode=LinearYearly
set number=8760
set stepsize=1h
solve
```

From the C API, pass `SolveModes_LinearYearly` (numeric value 18) to `Solution_Set_Mode`. Entering the mode sets the same defaults as Yearly: 8760 solutions, a one-hour step, normal control mode, and monitor and energy-meter sampling. It temporarily forces `SolutionLoadModels_Admittance`; the previously selected default load model is restored when another solution mode is selected. Attempts to select `SolutionLoadModels_PowerFlow` while the mode is active are rejected.

## Solver semantics

Each step advances time and applies the same yearly shapes and growth multipliers as Yearly before rebuilding time-dependent power-conversion admittances and solving the system directly. The `Algorithm` option is therefore ignored. `Solution_Get_Converged` means that the direct numerical solve succeeded; it does not indicate convergence of an iterative power-flow algorithm.

If a direct step fails, `LinearYearly` aborts the loop immediately. That failed step is not sampled by monitors or energy meters and does not run post-solve control actions.

Explicit `Yearly` mode with `LoadModel=Admittance` uses the same refreshed direct-solve mechanics. This avoids stale admittance matrices when time, shapes, or nominal power change between steps.

## Model scope

The v1 implementation refreshes yearly-dependent nominal power and admittance behavior for Load, Generator, PVSystem, Storage, IndMach012, VSource, and Isource elements. `IndMach012.CalcYPrim` refreshes nominal power before constructing its primitive admittance matrix.

Controls still execute through the standard Yearly control loop, but their results are evaluated against approximate linear voltages. In particular:

- InvControl remains available, with decisions based on those approximate voltages.
- StorageController support is limited to controller timing and discrete Storage state transitions.
- Continuous StorageController kW, kvar, or percentage-rate redispatch is not supported in v1. When requested, the controller emits warning 14410; quantitative redispatch results should not be relied upon.

Use the standard iterative Yearly power-flow mode when voltage-dependent behavior, nonlinear convergence, or fully quantitative controller redispatch is required.
