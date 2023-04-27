// Zepben

#include <stdbool.h>

#ifndef rmq_push_h
#define rmq_push_h

int connect_rabbitmq(char *hostname, int port, char *username, char *password, char *routingkey, char *exchange);
int disconnect_rabbitmq();

// Repeated values for the DemandIntervalReport.
struct TVoltBaseRegisters {
    double vbase;
    double kvLosses;
    double kvLineLoss;
    double kvLoadLoss;
    double kvNoLoadLoss;
    double kvLoadEnergy;
};

// Record for streaming the records written to DI_MHandle in WriteDemandIntervalData (top).
struct TDemandIntervalReport {
    const char *element;
    double hour;

    double kwh;
    double kvarh;
    double maxKw;
    double maxKva;
    double zoneKwh;
    double zoneKvarh;
    double zoneMaxKw;
    double zoneMaxKva;
    double overloadKwhNormal;
    double overloadKwhEmerg;
    double loadEEN;
    double loadUE;
    double zoneLossesKwh;
    double zoneLossesKvarh;
    double zoneMaxKwLosses;
    double zoneMaxKvarLosses;
    double loadLossesKwh;
    double loadLossesKvarh;
    double noLoadLossesKwh;
    double noLoadLossesKvarh;
    double maxKwLoadLosses;
    double maxKwNoLoadLosses;
    double lineLosses;
    double transformerLosses;

    double lineModeLineLosses;
    double zeroModeLineLosses;

    double phaseLineLosses3;
    double phaseLineLosses12;

    double genKwh;
    double genKvarh;
    double genMaxKw;
    double genMaxKva;
    
    int numVoltBases;
    // voltBases should be here, but seems to cause issues, so pass it as a separate parameter.
    // struct TVoltBaseRegisters voltBases[];
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
    int numValues;
    // values should be here, but seems to cause issues, so pass it as a separate parameter.
    // struct TPhaseVoltageReportValues values[];
};

// Record for streaming the records written to OV_MHandle in WriteOverloadReport.
struct TOverloadReport {
    double hour;
    const char *element;
    double normalAmps;
    double emergAmps;
    double percentNormal;
    double percentEmerg;
    double kvBase;
    double phase1Amps;
    double phase2Amps;
    double phase3Amps;
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
// to keep as a timestamp instead of performing string tranformations
struct TSummaryReport {
    char *circuitName;
    bool solved;
    char *mode;
    int number;
    double loadMult;
    int numDevices;
    int numBuses;
    int numNodes;
    int iterations;
    char *controlMode;
    int controlIterations;
    int mostIterationsDone;
    int year;
    int hour;
    double maxPuVoltage;
    double minPuVoltage;
    double totalMW;
    double totalMvar;
    double MWLosses;
    double pctLosses;
    double mvarLosses;
    double frequency;
};

// Record for streaming the event log from opendss
struct TEventLog {
    int hour;
    double sec;
    int controlIter;
    int iteration;
    char *element;
    char *action;
    char *event;
};

// Record for streaming the Tap report from Common/ExportResults
struct TTapReport {
    char *name; 
    double tap;
    double mintap;
    double maxtap;
    double step;
    int position;
};

// Record for Loop/Parallel lines report
struct TLoopReport {
    char *meter;
    char *lineA;
    char *lineB;
    char *relation;
};

struct TLossesEntry {
    char  *element;
    double kLoss;
    double pctPower;
    double kvarLosses;
};

struct TLossesTotals {
    double lineLosses;
    double transformerLosses;
    double totalLoadPower;
    double totalPctLosses;
};

struct TNodeMismatch {
    char *bus;
    int node;
    double currentSum;
    double pctError;
    double maxCurrent;
};

struct TKVBaseMismatch {
    char *load;
    double kv;
    char *bus;
    double kvBase;
};

struct TIsolatedArea {
    int id;
    char *line;
    int numLoads;
    char **loads;
};

struct TIsolatedElement {
    char *name;
    int numBuses;
    char **buses;
};

struct TIsolatedBusesReport {
    char **isolatedBuses;
    struct TIsolatedArea *isolatedArea;
    struct TIsolatedElement *isolatedElement;
    int numBuses, numAreas, numElements;
};

// diVoltBases should be in the di struct, but seems to cause issues, so pass it as a separate parameter.
void send_demand_interval_report(struct TDemandIntervalReport di, struct TVoltBaseRegisters diVoltBases[]);
// phvValues should be in the phv struct, but seems to cause issues, so pass it as a separate parameter.
void send_phase_voltage_report(struct TPhaseVoltageReport phv, struct TPhaseVoltageReportValues phvValues[]);
void send_overload_report(struct TOverloadReport ov);
void send_voltage_report(struct TVoltageReport vr);

// Diagnostics reports
void send_summary_report(struct TSummaryReport sr);
void send_tap_report(struct TTapReport tap);
void send_eventlog(struct TEventLog *eventlog, int numEvents);
void send_loop_report(struct TLoopReport loop);
void send_losses_entry(struct TLossesEntry lr);
void send_losses_totals(struct TLossesTotals lr);
void send_node_mismatch_report(struct TNodeMismatch nm);
void send_kvbase_mismatch_report(struct TKVBaseMismatch bm);
void send_isolated_elements_report(struct TIsolatedBusesReport ib);

#endif
