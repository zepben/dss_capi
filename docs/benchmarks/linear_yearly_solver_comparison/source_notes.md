# Source and reproducibility notes

## Source files

- BAR8B4 initial benchmark: `benchmark_before.json`
- BAR8B4 optimized benchmark: `benchmark_after.json`
- BAR8B4 annual equivalence: `annual_verification.json`
- ADA12 initial benchmark: `ada12/benchmark_before.json`
- ADA12 optimized benchmark: `ada12/benchmark_after.json`
- ADA12 annual equivalence: `ada12/annual_verification.json`

The raw outputs were produced from the local native ARM64 macOS DSS C-API builds and consolidated into `benchmark_data.json` without rounding the stored measurements.
`benchmark_queries.sql` reproduces the bounded chart and table datasets from those reviewed values for the portable report renderer.

## Comparison definition

The report uses the ordinary Yearly timing from the optimized build as the single "Original Yearly" comparison point. The ordinary Yearly solver code path was not changed by the optimization. Its cross-build median moved by only 0.19% on BAR8B4 and 0.53% on ADA12, providing a noise check.

The initial LinearYearly timing comes from the original feature implementation. The optimized LinearYearly timing comes from commit `b21850de`, which reuses existing incremental Y-matrix and stable-node solution-array paths.

Runtime reduction is calculated as:

`1 - optimized LinearYearly median / initial LinearYearly median`

Relative mode cost is calculated as:

`mode median / ordinary Yearly median`

## Chart map

- Section: median runtime comparison
- Question: how do the three solution paths compare on each feeder?
- Family: comparison and ranking
- Type: grouped vertical bar
- Fields: model, mode, median_seconds
- Takeaway: optimized LinearYearly removes about 38% of its initial runtime on both models, but remains slower than ordinary Yearly
- Palette: categorical, with labels and legend providing non-color distinction
- Delivery: native chart embedded in the portable HTML report

## Validation boundary

The annual checks compare optimized incremental LinearYearly against LinearYearly with a forced full Y-matrix rebuild before every step. They verify optimization equivalence and cumulative-drift behavior. They do not measure the approximation error between LinearYearly and nonlinear AC Yearly.
