// Zepben

#include <stdbool.h>

#ifndef rmq_push_h
#define rmq_push_h

// Use functions from rmqstream
extern void connect_to_stream(char const *hostname, int const port, char const *username, char const *password, char const *stream, int const heartbeat);
extern void disconnect_from_stream();
extern void stream_out_message(void const *msg_ptr, size_t msg_size, bool confirm);

// Repeated values for the DemandIntervalReport.
struct TVoltBaseRegisters {
    double vbase;
    double kv_losses;
    double kv_line_loss;
    double kv_load_loss;
    double kv_no_load_loss;
    double kv_load_energy;
};

// Record for streaming the records written to DI_MHandle in WriteDemandIntervalData (top).
struct TDemandIntervalReport {
    const char *element;
    double hour;

    double kwh;
    double kvarh;
    double max_kw;
    double max_kva;
    double zone_kwh;
    double zone_kvarh;
    double zone_max_kw;
    double zone_max_kva;
    double overload_kwh_normal;
    double overload_kwh_emerg;
    double load_een;
    double load_ue;
    double zone_losses_kwh;
    double zone_losses_kvarh;
    double zone_max_kw_losses;
    double zone_max_kvar_losses;
    double load_losses_kwh;
    double load_losses_kvarh;
    double no_load_losses_kwh;
    double no_load_losses_kvarh;
    double max_kw_load_losses;
    double max_kw_no_load_losses;
    double line_losses;
    double transformer_losses;

    double line_mode_line_losses;
    double zero_mode_line_losses;

    double phase_line_losses_3;
    double phase_line_losses_12;

    double gen_kwh;
    double gen_kvarh;
    double gen_max_kw;
    double gen_max_kva;

    struct TVoltBaseRegisters* volt_bases;
    int num_volt_bases;
};

// Repeated values for the TPhaseVoltageReportValues.
struct TMaxMinAvg {
    double max;
    double min;
    double avg;
};

// Repeated values for the PhaseVoltageReport.
struct TPhaseVoltageReportValues {
    double vbase;
    struct TMaxMinAvg phs1;
    struct TMaxMinAvg phs2;
    struct TMaxMinAvg phs3;
};

// Record for streaming the records written to PHV_MHandle in WriteDemandIntervalData (bottom).
struct TPhaseVoltageReport {
    const char *element;
    double hour;
    struct TPhaseVoltageReportValues* values;
    int num_values;
};

// Record for streaming the records written to OV_MHandle in WriteOverloadReport.
struct TOverloadReport {
    double hour;
    const char *element;
    double normal_amps;
    double emerg_amps;
    double percent_normal;
    double percent_emerg;
    double kv_base;
    double phase1_amps;
    double phase2_amps;
    double phase3_amps;
};

// Repeated values for the VoltageReport.
struct TVoltageReportValues {
    int under_voltages;
    double min_voltage;
    int over_voltage;
    double max_voltage;
    const char *min_bus;
    const char *max_bus;
};

// Record for streaming the records written to VR_MHandle in WriteVoltageReport.
struct TVoltageReport {
    double hour;
    struct TVoltageReportValues hv;
    struct TVoltageReportValues lv;
};

// Record for streaming the summary from Common/ExportResults
// We should calculate the time on the C side, as this is a one-off and will be simpler
// to keep as a timestamp instead of performing string transformations
struct TSummaryReport {
    char *case_name;
    bool solved;
    char *mode;
    int number;
    double load_mult;
    int num_devices;
    int num_buses;
    int num_nodes;
    int iterations;
    char *control_mode;
    int control_iterations;
    int most_iterations_done;
    int year;
    int hour;
    double max_pu_voltage;
    double min_pu_voltage;
    double total_mw;
    double total_mvar;
    double mw_losses;
    double pct_losses;
    double mvar_losses;
    double frequency;
};

// Record for streaming the event log from opendss
struct TEventLog {
    int hour;
    double sec;
    int control_iter;
    int iteration;
    char *element;
    char *action;
    char *event;
};

// Record for streaming the Tap report from Common/ExportResults
struct TTapsReport {
    char *name;
    double tap;
    double min;
    double max;
    double step;
    int position;
};

// Record for Loop/Parallel lines report
struct TLoopReport {
    char *meter;
    char *line_a;
    char *line_b;
    bool parallel;
    bool looped;
};

// Record for a single losses entry
struct TLossesEntry {
    char *element;
    double kw_losses;
    double pct_power;
    double kvar_losses;
};

// Record for streaming the total losses report
struct TLossesTotals {
    double line_losses;
    double transformer_losses;
    double total_losses;
    double total_load_power;
    double total_pct_losses;
};

// Record for a node mismatch report
struct TNodeMismatch {
    char *bus;
    int node;
    double current_sum;
    double pct_error;
    double max_current;
};

// Record for a kv/base settings mismatch report
struct TKVBaseMismatch {
    char *load;
    double kv;
    char *bus;
    double kv_base;
};

// Record for the isolatedAreas.
struct TIsolatedArea {
    int level;
    char *element;
    char **loads;
    int num_loads;
};

// Record for the isolatedElements.
struct TIsolatedElement {
    char *name;
    char **buses;
    int num_buses;
};

// The complete record of the Isolated Buses report 
struct TIsolatedBusesReport {
    char **isolated_buses;
    int num_buses;
    struct TIsolatedArea *isolated_area;
    int num_areas;
    struct TIsolatedElement *isolated_element;
    int num_elements;
};

// Energy meter reports
void batch_push_demand_interval_report(struct TDemandIntervalReport data);
void batch_push_phase_voltage_report(struct TPhaseVoltageReport data);
void batch_push_overload_report(struct TOverloadReport data);
void batch_push_voltage_report(struct TVoltageReport data);

// Diagnostic reports
void batch_push_summary_report(struct TSummaryReport data);
void batch_push_taps_report(struct TTapsReport data);
void batch_push_eventlog(struct TEventLog *data, int num_events);
void batch_push_loop_report(struct TLoopReport data);
void batch_push_isolated_elements_report(struct TIsolatedBusesReport data);
void batch_push_losses_entry(struct TLossesEntry data);
void batch_push_losses_totals(struct TLossesTotals data);
void batch_push_node_mismatch_report(struct TNodeMismatch data);
void batch_push_kvbase_mismatch_report(struct TKVBaseMismatch data);

// Final report (with failure flag). This also sends any other reports in the current batch.
void send_final_opendss_report(bool failure);

#endif
