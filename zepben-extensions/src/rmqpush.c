/*
 * Copyright 2020 Zeppelin Bend Pty Ltd
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/tcp_socket.h>

#include "proto/hc/opendss/EnergyMeter.pb-c.h"
#include "proto/hc/opendss/Diagnostics.pb-c.h"
#include "proto/hc/opendss/QueueMessage.pb-c.h"
#include "include/utils.h"
#include "include/rmqpush.h"

typedef enum ERabbitMQStatus {
    OK,
    ALREADY_CALLED,
    CONNECTION_FAILED,
    SOCKET_CREATION_FAILED,
    SOCKET_OPEN_FAILED,
    LOGIN_FAILED,
    CHANNEL_FAILED,
    CLEANUP_FAILED
} RabbitMQStatus;

static amqp_connection_state_t conn;
static amqp_basic_properties_t props;

static const char *exchange = NULL;
static const char *routing_key = NULL;

static bool connect_called = false;

const char *copy_str(const char *str) {
    char *copy = malloc(strlen(str) + 1);
    strcpy(copy, str);
    return copy;
}

int connect_rabbitmq(char *hostname, int port, char *username, char *password, char *_routing_key, char *_exchange) {
    if (connect_called)
        return ALREADY_CALLED;
    
    connect_called = true;
    
    printf("connecting OpenDSS to RabbitMQ - '%s@%s:%d'...", username, hostname, port);

    exchange = copy_str(_exchange);
    routing_key = copy_str(_routing_key);

    printf("connection...");

    conn = amqp_new_connection();
    if (conn == NULL)
        return CONNECTION_FAILED;

    printf("socket...");

    amqp_socket_t *socket = amqp_tcp_socket_new(conn);
    if (socket == NULL)
        return SOCKET_CREATION_FAILED;

    if (amqp_socket_open(socket, hostname, port) != AMQP_STATUS_OK)
        return SOCKET_OPEN_FAILED;

    printf("login...");
    
    // Login
    if (has_amqp_error(amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, username, password), "login"))
        return LOGIN_FAILED;

    printf("channel...");

    // Open channel
    amqp_channel_open(conn, 1);
    if (has_amqp_error(amqp_get_rpc_reply(conn), "channel"))
        return CHANNEL_FAILED;

    if (props.delivery_mode != AMQP_DELIVERY_PERSISTENT) {
        props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
        props.content_type = amqp_cstring_bytes("binary/proto");
        props.delivery_mode = AMQP_DELIVERY_PERSISTENT; /* persistent delivery mode */
    }
  
    printf("done.\n");
    return OK;
}

int disconnect_rabbitmq() {
    if (!connect_called)
        return OK;

    free((void*)exchange);
    exchange = NULL;
    
    free((void*)routing_key);
    routing_key = NULL;
    
    // Cleanup the connection if there is one.
    if (conn != NULL) {
        // This will implicitly clean up the channels and sockets.
        if (has_error(amqp_destroy_connection(conn), "cleanup"))
            return CLEANUP_FAILED;
        
        conn = NULL;
    }
    
    return OK;
}

void send_opendss_message(OpenDssReport *report) {
    int len = open_dss_report__get_packed_size(report);
    void *buf = malloc(len);
    open_dss_report__pack(report, buf);

    amqp_bytes_t body;
    body.len = len;
    body.bytes = buf;

    if (has_error(amqp_basic_publish(conn, 1, amqp_cstring_bytes(exchange), amqp_cstring_bytes(routing_key), 1, 0, &props, body), "publishing voltage report"))
      exit(1);

    free(buf);
}

// diVoltBases should be in the di struct, but seems to cause issues, so pass it as a separate parameter.
void send_demand_interval_report(struct TDemandIntervalReport di, struct TVoltBaseRegisters diVoltBases[]) {
    DemandIntervalReport report = DEMAND_INTERVAL_REPORT__INIT;

    report.element = (char*)di.element;
    report.hour = di.hour;
    report.kwh = di.kwh;
    report.kvarh = di.kvarh;
    report.maxkw = di.maxKw;
    report.maxkva = di.maxKva;
    report.zonekwh = di.zoneKwh;
    report.zonekvarh = di.zoneKvarh;
    report.zonemaxkw = di.zoneMaxKw;
    report.zonemaxkva = di.zoneMaxKva;
    report.overloadkwhnormal = di.overloadKwhNormal;
    report.overloadkwhemerg = di.overloadKwhEmerg;
    report.loadeen = di.loadEEN;
    report.loadue = di.loadUE;
    report.zonelosseskwh = di.zoneLossesKwh;
    report.zonelosseskvarh = di.zoneLossesKvarh;
    report.zonemaxkwlosses = di.zoneMaxKwLosses;
    report.zonemaxkvarlosses = di.zoneMaxKvarLosses;
    report.loadlosseskwh = di.loadLossesKwh;
    report.loadlosseskvarh = di.loadLossesKvarh;
    report.noloadlosseskwh = di.noLoadLossesKwh;
    report.noloadlosseskvarh = di.noLoadLossesKvarh;
    report.maxkwloadlosses = di.maxKwLoadLosses;
    report.maxkwnoloadlosses = di.maxKwNoLoadLosses;
    report.linelosses = di.lineLosses;
    report.transformerlosses = di.transformerLosses;
    report.linemodelinelosses = di.lineModeLineLosses;
    report.zeromodelinelosses = di.zeroModeLineLosses;
    report.phaselinelosses3 = di.phaseLineLosses3;
    report.phaselinelosses12 = di.phaseLineLosses12;
    report.genkwh = di.genKwh;
    report.genkvarh = di.genKvarh;
    report.genmaxkw = di.genMaxKw;
    report.genmaxkva = di.genMaxKva;

    report.n_voltbases = di.numVoltBases;
    report.voltbases = (VoltBaseRegisters**)malloc(di.numVoltBases * sizeof(VoltBaseRegisters*));

    for (int i = 0; i < di.numVoltBases; ++i) {
        VoltBaseRegisters voltBase = VOLT_BASE_REGISTERS__INIT;

        voltBase.vbase = diVoltBases[i].vbase;
        voltBase.kvlosses = diVoltBases[i].kvLosses;
        voltBase.kvlineloss = diVoltBases[i].kvLineLoss;
        voltBase.kvloadloss = diVoltBases[i].kvLoadLoss;
        voltBase.kvnoloadloss = diVoltBases[i].kvNoLoadLoss;
        voltBase.kvloadenergy = diVoltBases[i].kvLoadEnergy;

        VoltBaseRegisters* voltBasePtr = (VoltBaseRegisters*)malloc(sizeof(VoltBaseRegisters));
        *voltBasePtr = voltBase;

        report.voltbases[i] = voltBasePtr;
    }

    OpenDssReport queueMsg = OPEN_DSS_REPORT__INIT;
    queueMsg.report_case = OPEN_DSS_REPORT__REPORT_DI;
    queueMsg.di = &report;
    
    send_opendss_message(&queueMsg);

    for (int i = 0; i < di.numVoltBases; ++i)
        free(report.voltbases[i]);
    free(report.voltbases);
}

MaxMinAvg* copyMaxMinAvg(struct TMaxMinAvg *source) {
    MaxMinAvg phs = MAX_MIN_AVG__INIT;
    MaxMinAvg* phsPtr = (MaxMinAvg*)malloc(sizeof(MaxMinAvg));
    
    phs.max = source->max;
    phs.min = source->min;
    phs.avg = source->avg;
    
    *phsPtr = phs;
    
    return phsPtr;
}

// phvValues should be in the phv struct, but seems to cause issues, so pass it as a separate parameter.
void send_phase_voltage_report(struct TPhaseVoltageReport phv, struct TPhaseVoltageReportValues phvValues[]) {
    PhaseVoltageReport report = PHASE_VOLTAGE_REPORT__INIT;

    report.element = (char*)phv.element;
    report.hour = phv.hour;
    report.n_values = phv.numValues;
    report.values = (PhaseVoltageReportValues**)malloc(phv.numValues * sizeof(PhaseVoltageReportValues*));

    for (int i = 0; i < phv.numValues; ++i) {
        PhaseVoltageReportValues values = PHASE_VOLTAGE_REPORT_VALUES__INIT;
        
        values.vbase = phvValues[i].vbase;
        values.phs1 = copyMaxMinAvg(&phvValues[i].phs1);
        values.phs2 = copyMaxMinAvg(&phvValues[i].phs2);
        values.phs3 = copyMaxMinAvg(&phvValues[i].phs3);
        
        PhaseVoltageReportValues* valuesPtr = (PhaseVoltageReportValues*)malloc(sizeof(PhaseVoltageReportValues));
        *valuesPtr = values;

        report.values[i] = valuesPtr;
    }

    OpenDssReport queueMsg = OPEN_DSS_REPORT__INIT;
    queueMsg.report_case = OPEN_DSS_REPORT__REPORT_PHV;
    queueMsg.phv = &report;
    
    send_opendss_message(&queueMsg);

    for (int i = 0; i < phv.numValues; ++i) {
        free(report.values[i]->phs1);
        free(report.values[i]->phs2);
        free(report.values[i]->phs3);
        free(report.values[i]);
    }
    free(report.values);
}

void send_overload_report(struct TOverloadReport ov) {
    OverloadReport report = OVERLOAD_REPORT__INIT;

    report.hour = ov.hour;
    report.element = (char*)ov.element;
    report.normalamps = ov.normalAmps;
    report.emergamps = ov.emergAmps;
    report.percentnormal = ov.percentNormal;
    report.percentemerg = ov.percentEmerg;
    report.kvbase = ov.kvBase;
    report.phase1amps = ov.phase1Amps;
    report.phase2amps = ov.phase2Amps;
    report.phase3amps = ov.phase3Amps;

    OpenDssReport queueMsg = OPEN_DSS_REPORT__INIT;
    queueMsg.report_case = OPEN_DSS_REPORT__REPORT_OV;
    queueMsg.ov = &report;
    
    send_opendss_message(&queueMsg);
}

void send_voltage_report(struct TVoltageReport vr) {
    VoltageReport report = VOLTAGE_REPORT__INIT;
    VoltageReportValues hv = VOLTAGE_REPORT_VALUES__INIT;
    VoltageReportValues lv = VOLTAGE_REPORT_VALUES__INIT;

    report.hour = vr.hour;
    report.hv = &hv;
    report.lv = &lv;
    
    hv.undervoltages = vr.hv.under_voltages;
    hv.minvoltage = vr.hv.min_voltage;
    hv.overvoltage = vr.hv.over_voltage;
    hv.maxvoltage = vr.hv.max_voltage;
    hv.minbus = (char*)vr.hv.min_bus;
    hv.maxbus = (char*)vr.hv.max_bus;

    hv.undervoltages = vr.lv.under_voltages;
    hv.minvoltage = vr.lv.min_voltage;
    hv.overvoltage = vr.lv.over_voltage;
    hv.maxvoltage = vr.lv.max_voltage;
    hv.minbus = (char*)vr.lv.min_bus;
    hv.maxbus = (char*)vr.lv.max_bus;

    OpenDssReport queueMsg = OPEN_DSS_REPORT__INIT;
    queueMsg.report_case = OPEN_DSS_REPORT__REPORT_VR;
    queueMsg.vr = &report;
    
    send_opendss_message(&queueMsg);
}

void send_summary_report(struct TSummaryReport sr) {
    SummaryReport report = SUMMARY_REPORT__INIT;

    report.timestamp = time(NULL);
    report.circuitname = sr.circuitName;
    report.solved = sr.solved;
    report.mode = sr.mode;
    report.number = sr.number;
    report.loadmult = sr.loadMult;
    report.numdevices = sr.numDevices;
    report.numbuses = sr.numBuses;
    report.numnodes = sr.numNodes;
    report.iterations = sr.iterations;
    report.controlmode = sr.controlMode;
    report.controliterations = sr.controlIterations;
    report.mostiterationsdone = sr.mostIterationsDone;
    report.year = sr.year;
    report.hour = sr.hour;
    report.maxpuvoltage = sr.maxPuVoltage;
    report.minpuvoltage = sr.minPuVoltage;
    report.totalmw = sr.totalMW;
    report.totalmvar = sr.totalMvar;
    report.mwlosses = sr.MWLosses;
    report.pctlosses = sr.pctLosses;
    report.mvarlosses = sr.mvarLosses;
    report.frequency = sr.frequency;

    OpenDssReport queueMsg = OPEN_DSS_REPORT__INIT;
    queueMsg.report_case = OPEN_DSS_REPORT__REPORT_VR;
    queueMsg.sr = &report;
    
    send_opendss_message(&queueMsg);
}

void send_tap_report(struct TTapReport tap) {
    TapsReport report = TAPS_REPORT__INIT;

    report.name = tap.name;
    report.tap = tap.tap;
    report.mintap = tap.mintap;
    report.maxtap = tap.maxtap;
    report.step = tap.step;
    report.position = tap.position;

    OpenDssReport queueMsg = OPEN_DSS_REPORT__INIT;
    queueMsg.report_case = OPEN_DSS_REPORT__REPORT_TR;
    queueMsg.tr = &report;

    send_opendss_message(&queueMsg);
}

void send_eventlog(struct TEventLog *el, int numEvents) {
    EventLog log = EVENT_LOG__INIT;

    log.logentry = (EventLogEntry**)malloc(numEvents*sizeof(EventLogEntry*));

    for (int i = 0; i < numEvents; i++) {
        EventLogEntry logEntry = EVENT_LOG_ENTRY__INIT;
        logEntry.hour = el[i].hour;
        logEntry.sec = el[i].sec;
        logEntry.controliter = el[i].controlIter;
        logEntry.iteration = el[i].iteration;
        logEntry.element = el[i].element;
        logEntry.event = el[i].event;
        logEntry.action = el[i].action;

        EventLogEntry* entry = (EventLogEntry*)malloc(sizeof(EventLogEntry));
        *entry = logEntry;
        log.logentry[i] = entry;
    }

    OpenDssReport queueMsg = OPEN_DSS_REPORT__INIT;
    queueMsg.report_case = OPEN_DSS_REPORT__REPORT_EL;
    queueMsg.el = &log;
    send_opendss_message(&queueMsg);

    for (int i = 0; i < numEvents; i++) {
        free(log.logentry[i]);
    }
    free(log.logentry);

}

void send_loop_report(struct TLoopReport loop) {
    LoopReport loopReport = LOOP_REPORT__INIT;

    loopReport.meter = loop.meter;
    loopReport.linea = loop.lineA;
    loopReport.lineb = loop.lineB;
    loopReport.relation = loop.relation;


    OpenDssReport queueMsg = OPEN_DSS_REPORT__INIT;
    queueMsg.report_case = OPEN_DSS_REPORT__REPORT_LR;
    queueMsg.lr = &loopReport;
    send_opendss_message(&queueMsg);
}

void send_losses_entry(struct TLossesEntry le) {
    LossesEntry lossEntry = LOSSES_ENTRY__INIT;

    lossEntry.element = le.element;
    lossEntry.kloss = le.kLoss;
    lossEntry.pctpower = le.pctPower;
    lossEntry.kvarlosses = le.kvarLosses;

    OpenDssReport queueMsg = OPEN_DSS_REPORT__INIT;
    queueMsg.report_case = OPEN_DSS_REPORT__REPORT_LE;
    queueMsg.le = &lossEntry;

    send_opendss_message(&queueMsg);
}

void send_losses_totals(struct TLossesTotals lt) {
    LossesTotals totals = LOSSES_TOTALS__INIT;

    totals.linelosses = lt.lineLosses;
    totals.totalpctlosses = lt.totalPctLosses;
    totals.totalloadpower = lt.totalLoadPower;
    totals.transformerlosses = lt.transformerLosses;

    OpenDssReport queueMsg = OPEN_DSS_REPORT__INIT;
    queueMsg.report_case = OPEN_DSS_REPORT__REPORT_LOSSES;
    queueMsg.losses = &totals;
    send_opendss_message(&queueMsg);
}

void send_node_mismatch_report(struct TNodeMismatch nm) {
    NodeMismatch mismatch = NODE_MISMATCH__INIT;

    mismatch.bus = nm.bus;
    mismatch.node = nm.node;
    mismatch.currentsum = nm.currentSum;
    mismatch.pcterror = nm.pctError;
    mismatch.maxcurrent = nm.maxCurrent;

    OpenDssReport queueMsg = OPEN_DSS_REPORT__INIT;
    queueMsg.report_case = OPEN_DSS_REPORT__REPORT_NM;
    queueMsg.nm = &mismatch;

    send_opendss_message(&queueMsg);
}

void send_kvbase_mismatch_report(struct TKVBaseMismatch kvm) {
    KVBaseMismatch mismatch = KVBASE_MISMATCH__INIT;

    mismatch.load = kvm.load;
    mismatch.kv = kvm.kv;
    mismatch.bus = kvm.bus;
    mismatch.kvbase = kvm.kvBase;

    OpenDssReport queueMsg = OPEN_DSS_REPORT__INIT;
    queueMsg.report_case = OPEN_DSS_REPORT__REPORT_KVM;
    queueMsg.kvm = &mismatch;

    send_opendss_message(&queueMsg);
}

void send_isolated_elements_report(struct TIsolatedBusesReport ib) {
    IsolatedBusesReport report = ISOLATED_BUSES_REPORT__INIT;

    // isolatedBuses first; pick 1024b arbitrarily as I've never seen bus names this long 
    report.disconnectedbuses = malloc(ib.numBuses * 1024);
    for (int i = 0; i < ib.numBuses; i++) {
        report.disconnectedbuses[i] = ib.isolatedBuses[i];
    }


    // IsolatedAreas
    report.isolatedsubareas = malloc(ib.numAreas * sizeof(IsolatedArea));
    for (int i = 0; i < ib.numAreas; i++) {
        IsolatedArea area = ISOLATED_AREA__INIT;
        struct TIsolatedArea ia = ib.isolatedArea[i];

        area.id = ia.id;
        area.line = ia.line;
        area.loads = malloc(ia.numLoads * 1024);
        for (int j = 0; i < ia.numLoads; i++) {
            area.loads[j] = ia.loads[j];
        }

        IsolatedArea *a = (IsolatedArea*)malloc(sizeof(IsolatedArea));
        *a = area;
        report.isolatedsubareas[i] = a;
    }

    // IsolatedElements
    report.isolatedelements = malloc(ib.numElements * sizeof(IsolatedElement));
    for (int i = 0; i < ib.numElements; i++) {
        IsolatedElement element = ISOLATED_ELEMENT__INIT;
        struct TIsolatedElement ie = ib.isolatedElement[i];

        element.name = ie.name;
        element.buses = malloc(ie.numBuses * 1024);
        for (int j = 0; i < ie.numBuses; i++) {
            element.buses[j] = ie.buses[j];
        }

        IsolatedElement *el = (IsolatedElement*)malloc(sizeof(IsolatedElement));
        *el = element;

        report.isolatedelements[i] = el;
    }

    OpenDssReport queueMsg = OPEN_DSS_REPORT__INIT;
    queueMsg.report_case = OPEN_DSS_REPORT__REPORT_IB;
    queueMsg.ib = &report;
    send_opendss_message(&queueMsg);

    // free stuff
    free(report.disconnectedbuses);

    for (int i = 0; i < ib.numAreas; i++) {
        free(report.isolatedsubareas[i]->loads);
        free(report.isolatedsubareas[i]);
    }
    free(report.isolatedsubareas);

    for (int i = 0; i < ib.numElements; i++) {
        free(report.isolatedelements[i]->buses);
        free(report.isolatedelements[i]);
    }
    free(report.isolatedelements);
}

