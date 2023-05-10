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
        Vbase: Double;
        KvLosses: Double;
        KvLineLoss: Double;
        KvLoadLoss: Double;
        KvNoLoadLoss: Double;
        KvLoadEnergy: Double;
    end;

// Record for streaming the records written to DI_MHandle in WriteDemandIntervalData (top).
type
    TDemandIntervalReport = record
        Element: string;
        Hour: Double;

        Kwh: Double;
        Kvarh: Double;
        MaxKw: Double;
        MaxKva: Double;
        ZoneKwh: Double;
        ZoneKvarh: Double;
        ZoneMaxKw: Double;
        ZoneMaxKva: Double;
        OverloadKwhNormal: Double;
        OverloadKwhEmerg: Double;
        LoadEEN: Double;
        LoadUE: Double;
        ZoneLossesKwh: Double;
        ZoneLossesKvarh: Double;
        ZoneMaxKwLosses: Double;
        ZoneMaxKvarLosses: Double;
        LoadLossesKwh: Double;
        LoadLossesKvarh: Double;
        NoLoadLossesKwh: Double;
        NoLoadLossesKvarh: Double;
        MaxKwLoadLosses: Double;
        MaxKwNoLoadLosses: Double;
        LineLosses: Double;
        TransformerLosses: Double;

        LineModeLineLosses: Double;
        ZeroModeLineLosses: Double;

        PhaseLineLosses3: Double;
        PhaseLineLosses12: Double;

        GenKwh: Double;
        GenKvarh: Double;
        GenMaxKw: Double;
        GenMaxKva: Double;

        VoltBases: array of TVoltBaseRegisters;
        NumVoltBases: Integer;
    end;

// Repeated values for the TPhaseVoltageReportValues.
type
    TMaxMinAvg = record
        Max: Double;
        Min: Double;
        Avg: Double;
    end;

// Repeated values for the PhaseVoltageReport.
type
    TPhaseVoltageReportValues = record
        Vbase: Double;
        Phs1: TMaxMinAvg;
        Phs2: TMaxMinAvg;
        Phs3: TMaxMinAvg;
    end;

// Record for streaming the records written to PHV_MHandle in WriteDemandIntervalData (bottom).
type
    TPhaseVoltageReport = record
        Element: string;
        Hour: Double;
        Values: array of TPhaseVoltageReportValues;
        NumValues: Integer;
    end;

// Record for streaming the records written to OV_MHandle in WriteOverloadReport.
type
    TOverloadReport = record
        Hour: Double;
        Element: string;
        NormalAmps: Double;
        EmergAmps: Double;
        PercentNormal: Double;
        PercentEmerg: Double;
        KvBase: Double;
        Phase1Amps: Double;
        Phase2Amps: Double;
        Phase3Amps: Double;
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
        CaseName: string;
        Solved: Boolean;
        Mode: string;
        Number: Integer;
        LoadMult: Double;
        NumDevices: Integer;
        NumBuses: Integer;
        NumNodes: Integer;
        Iterations: Integer;
        ControlMode: string;
        ControlIterations: Integer;
        MostIterationsDone: Integer;
        Year: Integer;
        Hour: Integer;
        MaxPuVoltage: Double;
        MinPuVoltage: Double;
        TotalMW: Double;
        TotalMvar: Double;
        MwLosses: Double;
        PctLosses: Double;
        MvarLosses: Double;
        Frequency: Double;
    end;

// Record for streaming the Tap report from Common/ExportResults
type 
    TTapsReport = record
        Name: string; 
        Tap: Double;
        Mintap: Double;
        Maxtap: Double;
        Step: Double;
        Position: Integer;
    end;

// Record for streaming the Loops/Parallel lines in Energy Meter Zone
type
    TLoopReport = record
        MeterName: string;
        LineA: string;
        LineB: string;
        Parallel: Boolean;
        Looped: Boolean;
    end;

// Records for streaming the Isolated elements
// Repeated values for the isolatedAreas.
type
    TIsolatedArea = record
        Level: Integer;
        Element: string;
        NumLoads: Integer;
        Loads: array of string;
    end;

// Repeated values for the isolatedElements.
type
    TIsolatedElement = record
        Name: string;
        NumBuses: Integer;
        Buses: array of string;
    end;

// The complete record of the Isolated Buses report 
type
    TIsolatedBusesReport = record
        DisconnectedBuses: array of string;
        NumBuses: Integer;
        IsolatedSubAreas: array of TIsolatedArea;
        NumAreas: Integer;
        IsolatedElements: array of TIsolatedElement;
        NumElements: Integer;
    end;

// Repeated record for a single losses entry
type
    TLossesEntry = record
        Element: string;
        KwLosses: Double;
        PctPower: Double;
        KvarLosses: Double;
    end;

// Record for streaming the total losses report
type
    TLossesTotals = record
        LineLosses: Double;
        TransformerLosses: Double;
        TotalLosses: Double;
        TotalLoadPower: Double;
        TotalPctLosses: Double;
    end;

// Record for a node mismatch report
type
    TNodeMismatch = record
        Bus: string;
        Node: Integer;
        CurrentSum: Double;
        PctError: Double;
        MaxCurrent: Double;
    end;

// Record for a kvbase settings mismatch report
type
    TKVBaseMismatch = record
        Load: string;
        Kv: double;
        Bus: string;
        KvBase: Double;
    end;

// Energy meter reports
procedure send_demand_interval_report(data: TDemandIntervalReport); RMQPUSH_CALL;
procedure send_phase_voltage_report(data: TPhaseVoltageReport); RMQPUSH_CALL;
procedure send_overload_report(data: TOverloadReport); RMQPUSH_CALL;
procedure send_voltage_report(data: TVoltageReport); RMQPUSH_CALL;

// Diagnotic reports 
procedure send_summary_report(data: TSummaryReport); RMQPUSH_CALL;
procedure send_taps_report(data: TTapsReport); RMQPUSH_CALL;
procedure send_eventlog(data: array of TEventLog; num_events: Integer); RMQPUSH_CALL;
procedure send_loop_report(data: TLoopReport); RMQPUSH_CALL;
procedure send_isolated_elements_report(data: TIsolatedBusesReport); RMQPUSH_CALL;
procedure send_losses_entry(data: TLossesEntry); RMQPUSH_CALL;
procedure send_losses_totals(data: TLossesTotals); RMQPUSH_CALL;
procedure send_node_mismatch_report(data: TNodeMismatch); RMQPUSH_CALL;
procedure send_kvbase_mismatch_report(data: TKVBaseMismatch); RMQPUSH_CALL;

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