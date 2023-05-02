unit ZepbenHC;

{$MACRO ON}
{$DEFINE RMQPUSH_CALL:=cdecl; external}

interface
uses
    Dynamics,
    SysUtils,
    Classes;


function getBooleanEnv(name: String; default: boolean): Boolean; inline; 
procedure debug(msg: String); 

// Repeated values for the DemandIntervalReport.
type
    TVoltBaseRegisters = record
        vbase: Double;
        kvLosses: Double;
        kvLineLoss: Double;
        kvLoadLoss: Double;
        kvNoLoadLoss: Double;
        kvLoadEnergy: Double;
    end;

// Upper bound needs to be >= NumEMVbase (aka MaxVBaseCount) from EnergyMeter.pas.
type
    TVoltBaseRegistersArray = array [0..10] of TVoltBaseRegisters;

// Record for streaming the records written to DI_MHandle in WriteDemandIntervalData (top).
type
    TDemandIntervalReport = record
        element: string;
        hour: Double;

        kwh: Double;
        kvarh: Double;
        maxKw: Double;
        maxKva: Double;
        zoneKwh: Double;
        zoneKvarh: Double;
        zoneMaxKw: Double;
        zoneMaxKva: Double;
        overloadKwhNormal: Double;
        overloadKwhEmerg: Double;
        loadEEN: Double;
        loadUE: Double;
        zoneLossesKwh: Double;
        zoneLossesKvarh: Double;
        zoneMaxKwLosses: Double;
        zoneMaxKvarLosses: Double;
        loadLossesKwh: Double;
        loadLossesKvarh: Double;
        noLoadLossesKwh: Double;
        noLoadLossesKvarh: Double;
        maxKwLoadLosses: Double;
        maxKwNoLoadLosses: Double;
        lineLosses: Double;
        transformerLosses: Double;

        lineModeLineLosses: Double;
        zeroModeLineLosses: Double;

        phaseLineLosses3: Double;
        phaseLineLosses12: Double;

        genKwh: Double;
        genKvarh: Double;
        genMaxKw: Double;
        genMaxKva: Double;
        
        numVoltBases: Integer;
        // voltBases should be here, but seems to cause issues, so pass it as a separate parameter.
        // voltBases: TVoltBaseRegistersArray;
    end;

// Repeated values for the TPhaseVoltageReportValues.
type
    TMaxMinAvg = record
        max: Double;
        min: Double;
        avg: Double;
    end;

// Repeated values for the PhaseVoltageReport.
type
    TPhaseVoltageReportValues = record
        vbase: Double;
        phs1: TMaxMinAvg;
        phs2: TMaxMinAvg;
        phs3: TMaxMinAvg;
    end;

// Upper bound needs to be >= NumEMVbase (aka MaxVBaseCount) from EnergyMeter.pas.
type
    TPhaseVoltageReportValuesArray = array [0..10] of TPhaseVoltageReportValues;

// Record for streaming the records written to PHV_MHandle in WriteDemandIntervalData (bottom).
type
    TPhaseVoltageReport = record
        element: string;
        hour: Double;
        numValues: Integer;
        // values should be here, but seems to cause issues, so pass it as a separate parameter.
        // values: TPhaseVoltageReportValuesArray;
    end;

// Record for streaming the records written to OV_MHandle in WriteOverloadReport.
type
    TOverloadReport = record
        hour: Double;
        element: string;
        normalAmps: Double;
        emergAmps: Double;
        percentNormal: Double;
        percentEmerg: Double;
        kvBase: Double;
        phase1Amps: Double;
        phase2Amps: Double;
        phase3Amps: Double;
    end;

// Repeated values for the VoltageReport.
type
    TVoltageReportValues = record
        UnderVoltages: Integer;
        MinVoltage: Double;
        OverVoltage: Integer;
        MaxVoltage: Double;
        MinBus: AnsiString;
        MaxBus: string;
    end;

// Record for streaming the records written to VR_MHandle in WriteVoltageReport.
type
    TVoltageReport = record
        Hour: Double;
        Hv: TVoltageReportValues;
        Lv: TVoltageReportValues;
    end;

// Record for event log from Common/Utilities
type
    TEventLog = record
        Hour: Integer;
        Sec: Double;
        ControlIter: Integer;
        Iteration: Integer;
        Element: string;
        Action: string;
        Event: string;
    end;

// Record for streaming the summary from Common/ExportResults
// We should calculate the time on the C side, as this is a one-off and will be simpler
// to keep as a timestamp instead of performing string tranformations
type 
    TSummaryReport = record
        circuitName: string;
        solved: Boolean;
        mode: string;
        number: Integer;
        loadMult: Double;
        numDevices: Integer;
        numBuses: Integer;
        numNodes: Integer;
        iterations: Integer;
        controlMode: string;
        controlIterations: Integer;
        mostIterationsDone: Integer;
        year: Integer;
        hour: Integer;
        maxPuVoltage: Double;
        minPuVoltage: Double;
        totalMW: Double;
        totalMvar: Double;
        mWLosses: Double;
        pctLosses: Double;
        mvarLosses: Double;
        frequency: Double;
    end;

// Record for streaming the Tap report from Common/ExportResults
type 
    TTapReport = record
        name: string; 
        tap: Double;
        mintap: Double;
        maxtap: Double;
        step: Double;
        position: Integer;
    end;

// Record for streaming the Loops/Parallel lines in Energy Meter Zone
type
    TLoopReport = record
        meterName: string;
        lineA: string;
        lineB: string;
        parallel: Boolean;
        looped: Boolean;
    end;

// Records for streaming the Isolated elements
// Repeated values for the isolatedAreas.
type
    TIsolatedArea = record
        level: Integer;
        element: string;
        numLoads: Integer;
        loads: array of string;
    end;

// Repeated values for the isolatedElements.
type
    TIsolatedElement = record
        name: string;
        numBuses: Integer;
        buses: array of string;
    end;

// The complete record of the Isolated Buses report 
type
    TIsolatedBusesReport = record
        disconnectedBuses: array of string;
        isolatedSubAreas: array of TIsolatedArea;
        isolatedElements: array of TIsolatedElement;
        numBuses, numAreas, numElements: Integer;
    end;

// Repeated record for a single losses entry
type
    TLossesEntry = record
        element: string;
        kwLosses: Double;
        pctPower: Double;
        kvarLosses: Double;
    end;

// Record for streaming the total losses report
type
    TLossesTotals = record
        lineLosses: Double;
        transformerLosses: Double;
        totalLosses: Double;
        totalLoadPower: Double;
        totalPctLosses: Double;
    end;

// Record for a node mismatch report
type
    TNodeMismatch = record
        bus: string;
        node: Integer;
        currentSum: Double;
        pctError: Double;
        maxCurrent: Double;
    end;

// Record for a kvbase settings mismatch report
type
    TKVBaseMismatch = record
        load: string;
        kv: double;
        bus: string;
        kvBase: Double;
    end;

// diVoltBases should be in the di record, but seems to cause issues, so pass it as a separate parameter.
procedure send_demand_interval_report(di: TDemandIntervalReport; diVoltBases: TVoltBaseRegistersArray); RMQPUSH_CALL;
// phvValues should be in the phv record, but seems to cause issues, so pass it as a separate parameter.
procedure send_phase_voltage_report(phv: TPhaseVoltageReport; phvValues: TPhaseVoltageReportValuesArray); RMQPUSH_CALL;
procedure send_overload_report(ov: TOverloadReport); RMQPUSH_CALL;
procedure send_voltage_report(vr: TVoltageReport); RMQPUSH_CALL;

// Diagnotics 
procedure send_summary_report(sr: TSummaryReport); RMQPUSH_CALL;
procedure send_taps_report(tp: TTapReport); RMQPUSH_CALL;
procedure send_eventlog(el: array of TEventLog; numEvents: Integer); RMQPUSH_CALL;
procedure send_loop_report(lr: TLoopReport); RMQPUSH_CALL;
procedure send_isolated_elements_report(ib: TIsolatedBusesReport); RMQPUSH_CALL;
procedure send_losses_entry(le: TLossesEntry); RMQPUSH_CALL;
procedure send_losses_totals(lt: TLossesTotals); RMQPUSH_CALL;
procedure send_node_mismatch_report(nm: TNodeMismatch); RMQPUSH_CALL;
procedure send_kvbase_mismatch_report(kvm: TKVBaseMismatch); RMQPUSH_CALL;

implementation

function getBooleanEnv(name: String; default: boolean): Boolean; inline; 
begin
    Result := Length(SysUtils.GetEnvironmentVariable(name)) > 0;
end;

procedure debug(msg: String); 
begin
   if (Length(SysUtils.GetEnvironmentVariable('HC_DEBUG')) > 0) then
       writeln(msg);
end;

end.
