-- Runtime medians used by the grouped comparison chart.
WITH runtime_medians(model, mode, median_seconds, steps_per_second, build, sample_count) AS (
    VALUES
        ('BAR8B4', 'Original Yearly', 0.226505375, 6357.46502704406, 'optimized branch', 5),
        ('BAR8B4', 'Initial LinearYearly', 0.501231375, 2872.9247046835403, 'initial LinearYearly branch', 5),
        ('BAR8B4', 'Optimized LinearYearly', 0.310182833, 4642.423263959292, 'optimized branch', 5),
        ('ADA12', 'Original Yearly', 0.4537365, 3173.6481415976014, 'optimized branch', 5),
        ('ADA12', 'Initial LinearYearly', 0.889894541, 1618.1692702394048, 'initial LinearYearly branch', 5),
        ('ADA12', 'Optimized LinearYearly', 0.546885667, 2633.091497715189, 'optimized branch', 5)
)
SELECT * FROM runtime_medians;

-- Exact runtime comparison table.
WITH runtime_summary(
    model,
    original_yearly_seconds,
    initial_linear_seconds,
    optimized_linear_seconds,
    initial_linear_over_yearly,
    optimized_linear_over_yearly,
    optimized_runtime_reduction,
    optimized_speedup,
    ordinary_yearly_cross_build_delta
) AS (
    VALUES
        ('BAR8B4', 0.226505375, 0.501231375, 0.310182833, 2.2128895395970183, 1.369428133879825, 0.3811583861844243, 1.6159223582821556, 0.001936367491806923),
        ('ADA12', 0.4537365, 0.889894541, 0.546885667, 1.9612584418489587, 1.2052935282923019, 0.38544890230987494, 1.6272039928960143, 0.005273459232675082)
)
SELECT * FROM runtime_summary;

-- Full-year numerical-equivalence table.
WITH annual_equivalence(
    model,
    steps,
    optimized_step_loop_seconds,
    forced_full_rebuild_step_loop_seconds,
    incremental_runtime_reduction,
    max_voltage_error_volts,
    max_head_power_error_kw_or_kvar,
    all_values_finite,
    public_solver_options_unchanged
) AS (
    VALUES
        ('BAR8B4', 17520, 3.61719679197995, 5.940047957992647, 0.3910492276223424, 7.97426764620468e-08, 1.320411655569842e-07, 1, 1),
        ('ADA12', 17472, 6.60426545800874, 10.673207541985903, 0.3812295477222659, 5.723450158257037e-09, 1.3564317669079173e-08, 1, 1)
)
SELECT * FROM annual_equivalence;
