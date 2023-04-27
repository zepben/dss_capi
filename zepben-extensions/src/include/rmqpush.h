// Zepben

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
// diVoltBases should be in the di struct, but seems to cause issues, so pass it as a separate parameter.
void send_demand_interval_report(struct TDemandIntervalReport di, struct TVoltBaseRegisters diVoltBases[]);
// phvValues should be in the phv struct, but seems to cause issues, so pass it as a separate parameter.
void send_phase_voltage_report(struct TPhaseVoltageReport phv, struct TPhaseVoltageReportValues phvValues[]);
void send_overload_report(struct TOverloadReport ov);
void send_voltage_report(struct TVoltageReport vr);

// Diagnostics reports
void send_eventlog(struct TEventLog *eventlog, int numEvents);
#endif
