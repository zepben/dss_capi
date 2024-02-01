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
#include <time.h>
#include <unistd.h>

#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/tcp_socket.h>

#include "include/rmqpush.h"
#include "include/utils.h"
#include "proto/zepben/protobuf/hc/opendss/Diagnostics.pb-c.h"
#include "proto/zepben/protobuf/hc/opendss/EnergyMeter.pb-c.h"
#include "proto/zepben/protobuf/hc/opendss/OpenDssReport.pb-c.h"

#define REPORT_BATCH_SIZE 50

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

// Connection parameters
static const char* host = NULL;
static const char* user = NULL;
static const char* pass = NULL;
static const char* exchange = NULL;
static const char* routing_key = NULL;
static int port;
static int heartbeat;

static bool conn_conf_set = false;
static bool connect_called = false;

// message batching
static OpenDssReport* reports[REPORT_BATCH_SIZE];
static OpenDssReportBatch open_dss_report_batch = {
    PROTOBUF_C_MESSAGE_INIT (&open_dss_report_batch__descriptor),
    0,
    reports
};

char* copy_str(const char* str) {
    char* copy = malloc(strlen(str) + 1);
    strcpy(copy, str);
    return copy;
}

void clear_mem() {
    free((void*)exchange);
    exchange = NULL;

    free((void*)routing_key);
    routing_key = NULL;

    free((void*)host);
    host = NULL;

    free((void*)user);
    user = NULL;

    free((void*)pass);
    pass = NULL;
}

void send_opendss_message(OpenDssReport* message) {
    reports[open_dss_report_batch.n_reports] = malloc(sizeof(OpenDssReport));
    *(reports[open_dss_report_batch.n_reports]) = *message;
    if (++open_dss_report_batch.n_reports == REPORT_BATCH_SIZE) send_opendss_message_batch();
}

void send_opendss_message_batch() {
    if (open_dss_report_batch.n_reports == 0) return;

    int len = open_dss_report_batch__get_packed_size(&open_dss_report_batch);
    void* buf = malloc(len);
    open_dss_report_batch__pack(&open_dss_report_batch, buf);

    stream_out_message(buf, len);

    free(buf);

    for (int i = 0; i < open_dss_report_batch.n_reports; ++i) free_opendss_report(reports[i]);
    open_dss_report_batch.n_reports = 0;
}

void free_opendss_report(OpenDssReport* report) {
    switch (report->report_case) {
    OPEN_DSS_REPORT__REPORT_DI:
        for (int i = 0; i < report->di->n_voltbases; ++i) free(report->di->voltbases[i]);
        free(report->di->voltbases);
        break;
    OPEN_DSS_REPORT__REPORT_PHV:
        for (int i = 0; i < report->phv->n_values; ++i) {
            free(report->phv->values[i]->phs1);
            free(report->phv->values[i]->phs2);
            free(report->phv->values[i]->phs3);
            free(report->phv->values[i]);
        }
        free(report->phv->values);
        break;
    OPEN_DSS_REPORT__REPORT_EL:
        for (int i = 0; i < report->el->n_logentry; ++i) {
            free(report->el->logentry[i]);
        }
        free(report->el->logentry);
        break;
    OPEN_DSS_REPORT__REPORT_IBR:
        free(report->ibr->disconnectedbuses);

        for (int i = 0; i < report->ibr->n_isolatedsubareas; i++) {
            free(report->ibr->isolatedsubareas[i]->loads);
            free(report->ibr->isolatedsubareas[i]);
        }
        free(report->ibr->isolatedsubareas);

        for (int i = 0; i < report->ibr->n_isolatedelements; i++) {
            free(report->ibr->isolatedelements[i]->buses);
            free(report->ibr->isolatedelements[i]);
        }
        free(report->ibr->isolatedelements);
        break;
    default:
        break;
    }

    free(report);
}

void send_demand_interval_report(struct TDemandIntervalReport data) {
    DemandIntervalReport di = DEMAND_INTERVAL_REPORT__INIT;

    di.element = (char*)data.element;
    di.hour = data.hour;
    di.kwh = data.kwh;
    di.kvarh = data.kvarh;
    di.maxkw = data.max_kw;
    di.maxkva = data.max_kva;
    di.zonekwh = data.zone_kwh;
    di.zonekvarh = data.zone_kvarh;
    di.zonemaxkw = data.zone_max_kw;
    di.zonemaxkva = data.zone_max_kva;
    di.overloadkwhnormal = data.overload_kwh_normal;
    di.overloadkwhemerg = data.overload_kwh_emerg;
    di.loadeen = data.load_een;
    di.loadue = data.load_ue;
    di.zonelosseskwh = data.zone_losses_kwh;
    di.zonelosseskvarh = data.zone_losses_kvarh;
    di.zonemaxkwlosses = data.zone_max_kw_losses;
    di.zonemaxkvarlosses = data.zone_max_kvar_losses;
    di.loadlosseskwh = data.load_losses_kwh;
    di.loadlosseskvarh = data.load_losses_kvarh;
    di.noloadlosseskwh = data.no_load_losses_kwh;
    di.noloadlosseskvarh = data.no_load_losses_kvarh;
    di.maxkwloadlosses = data.max_kw_load_losses;
    di.maxkwnoloadlosses = data.max_kw_no_load_losses;
    di.linelosses = data.line_losses;
    di.transformerlosses = data.transformer_losses;
    di.linemodelinelosses = data.line_mode_line_losses;
    di.zeromodelinelosses = data.zero_mode_line_losses;
    di.phaselinelosses3 = data.phase_line_losses_3;
    di.phaselinelosses12 = data.phase_line_losses_12;
    di.genkwh = data.gen_kwh;
    di.genkvarh = data.gen_kvarh;
    di.genmaxkw = data.gen_max_kw;
    di.genmaxkva = data.gen_max_kva;

    di.n_voltbases = data.num_volt_bases;
    di.voltbases = (VoltBaseRegisters**)malloc(data.num_volt_bases * sizeof(VoltBaseRegisters*));

    for (int i = 0; i < data.num_volt_bases; ++i) {
        VoltBaseRegisters volt_base = VOLT_BASE_REGISTERS__INIT;

        volt_base.vbase = data.volt_bases[i].vbase;
        volt_base.kvlosses = data.volt_bases[i].kv_losses;
        volt_base.kvlineloss = data.volt_bases[i].kv_line_loss;
        volt_base.kvloadloss = data.volt_bases[i].kv_load_loss;
        volt_base.kvnoloadloss = data.volt_bases[i].kv_no_load_loss;
        volt_base.kvloadenergy = data.volt_bases[i].kv_load_energy;

        VoltBaseRegisters* volt_base_ptr = (VoltBaseRegisters*)malloc(sizeof(VoltBaseRegisters));
        *volt_base_ptr = volt_base;

        di.voltbases[i] = volt_base_ptr;
    }

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_DI;
    report.di = &di;

    send_opendss_message(&report);
}

MaxMinAvg* copyMaxMinAvg(struct TMaxMinAvg* source) {
    MaxMinAvg phs = MAX_MIN_AVG__INIT;
    MaxMinAvg* phs_ptr = (MaxMinAvg*)malloc(sizeof(MaxMinAvg));

    phs.max = source->max;
    phs.min = source->min;
    phs.avg = source->avg;

    *phs_ptr = phs;

    return phs_ptr;
}

void send_phase_voltage_report(struct TPhaseVoltageReport data) {
    PhaseVoltageReport phv = PHASE_VOLTAGE_REPORT__INIT;

    phv.element = (char*)data.element;
    phv.hour = data.hour;
    phv.n_values = data.num_values;
    phv.values = (PhaseVoltageReportValues**)malloc(data.num_values * sizeof(PhaseVoltageReportValues*));

    for (int i = 0; i < data.num_values; ++i) {
        PhaseVoltageReportValues values = PHASE_VOLTAGE_REPORT_VALUES__INIT;

        values.vbase = data.values[i].vbase;
        values.phs1 = copyMaxMinAvg(&data.values[i].phs1);
        values.phs2 = copyMaxMinAvg(&data.values[i].phs2);
        values.phs3 = copyMaxMinAvg(&data.values[i].phs3);

        PhaseVoltageReportValues* values_ptr = (PhaseVoltageReportValues*)malloc(sizeof(PhaseVoltageReportValues));
        *values_ptr = values;

        phv.values[i] = values_ptr;
    }

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_PHV;
    report.phv = &phv;

    send_opendss_message(&report);
}

void send_overload_report(struct TOverloadReport data) {
    OverloadReport ov = OVERLOAD_REPORT__INIT;

    ov.hour = data.hour;
    ov.element = (char*)data.element;
    ov.normalamps = data.normal_amps;
    ov.emergamps = data.emerg_amps;
    ov.percentnormal = data.percent_normal;
    ov.percentemerg = data.percent_emerg;
    ov.kvbase = data.kv_base;
    ov.phase1amps = data.phase1_amps;
    ov.phase2amps = data.phase2_amps;
    ov.phase3amps = data.phase3_amps;

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_OV;
    report.ov = &ov;

    send_opendss_message(&report);
}

void send_voltage_report(struct TVoltageReport data) {
    VoltageReport vr = VOLTAGE_REPORT__INIT;
    VoltageReportValues hv = VOLTAGE_REPORT_VALUES__INIT;
    VoltageReportValues lv = VOLTAGE_REPORT_VALUES__INIT;

    vr.hour = data.hour;
    vr.hv = &hv;
    vr.lv = &lv;

    hv.undervoltages = data.hv.under_voltages;
    hv.minvoltage = data.hv.min_voltage;
    hv.overvoltage = data.hv.over_voltage;
    hv.maxvoltage = data.hv.max_voltage;
    hv.minbus = (char*)data.hv.min_bus;
    hv.maxbus = (char*)data.hv.max_bus;

    hv.undervoltages = data.lv.under_voltages;
    hv.minvoltage = data.lv.min_voltage;
    hv.overvoltage = data.lv.over_voltage;
    hv.maxvoltage = data.lv.max_voltage;
    hv.minbus = (char*)data.lv.min_bus;
    hv.maxbus = (char*)data.lv.max_bus;

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_VR;
    report.vr = &vr;

    send_opendss_message(&report);
}

void send_summary_report(struct TSummaryReport data) {
    SummaryReport sr = SUMMARY_REPORT__INIT;

    sr.casename = data.case_name;
    sr.solved = data.solved;
    sr.mode = data.mode;
    sr.number = data.number;
    sr.loadmult = data.load_mult;
    sr.numdevices = data.num_devices;
    sr.numbuses = data.num_buses;
    sr.numnodes = data.num_nodes;
    sr.iterations = data.iterations;
    sr.controlmode = data.control_mode;
    sr.controliterations = data.control_iterations;
    sr.mostiterationsdone = data.most_iterations_done;
    sr.year = data.year;
    sr.hour = data.hour;
    sr.maxpuvoltage = data.max_pu_voltage;
    sr.minpuvoltage = data.min_pu_voltage;
    sr.totalmw = data.total_mw;
    sr.totalmvar = data.total_mvar;
    sr.mwlosses = data.mw_losses;
    sr.pctlosses = data.pct_losses;
    sr.mvarlosses = data.mvar_losses;
    sr.frequency = data.frequency;

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_SR;
    report.sr = &sr;

    send_opendss_message(&report);
}

void send_taps_report(struct TTapsReport data) {
    TapsReport tr = TAPS_REPORT__INIT;

    tr.name = data.name;
    tr.tap = data.tap;
    tr.min = data.min;
    tr.max = data.max;
    tr.step = data.step;
    tr.position = data.position;

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_TR;
    report.tr = &tr;

    send_opendss_message(&report);
}

void send_eventlog(struct TEventLog* data, int num_events) {
    EventLog el = EVENT_LOG__INIT;

    el.logentry = (EventLogEntry**)malloc(num_events * sizeof(EventLogEntry*));
    el.n_logentry = num_events;

    for (int i = 0; i < num_events; i++) {
        EventLogEntry log_entry = EVENT_LOG_ENTRY__INIT;


        log_entry.hour = data[i].hour;
        log_entry.sec = data[i].sec;
        log_entry.controliter = data[i].control_iter;
        log_entry.iteration = data[i].iteration;
        log_entry.element = data[i].element;
        log_entry.event = data[i].event;
        log_entry.action = data[i].action;

        EventLogEntry* entry = (EventLogEntry*)malloc(sizeof(EventLogEntry));
        *entry = log_entry;
        el.logentry[i] = entry;
    }

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_EL;
    report.el = &el;

    send_opendss_message(&report);
}

void send_loop_report(struct TLoopReport data) {
    LoopReport lr = LOOP_REPORT__INIT;

    lr.meter = data.meter;
    lr.linea = data.line_a;
    lr.lineb = data.line_b;
    lr.parallel = data.parallel;
    lr.looped = data.looped;

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_LR;
    report.lr = &lr;

    send_opendss_message(&report);
}

void send_losses_entry(struct TLossesEntry data) {
    LossesEntry le = LOSSES_ENTRY__INIT;

    le.element = data.element;
    le.kwlosses = data.kw_losses;
    le.pctpower = data.pct_power;
    le.kvarlosses = data.kvar_losses;

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_LE;
    report.le = &le;

    send_opendss_message(&report);
}

void send_losses_totals(struct TLossesTotals data) {
    LossesTotals losses = LOSSES_TOTALS__INIT;

    losses.totalpctlosses = data.total_pct_losses;
    losses.totalloadpower = data.total_load_power;
    losses.transformerlosses = data.transformer_losses;
    losses.linelosses = data.line_losses;
    losses.totallosses = data.total_losses;

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_LOSSES;
    report.losses = &losses;

    send_opendss_message(&report);
}

void send_node_mismatch_report(struct TNodeMismatch data) {
    NodeMismatch nm = NODE_MISMATCH__INIT;

    nm.bus = data.bus;
    nm.node = data.node;
    nm.currentsum = data.current_sum;
    nm.pcterror = data.pct_error;
    nm.maxcurrent = data.max_current;

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_NM;
    report.nm = &nm;

    send_opendss_message(&report);
}

void send_kvbase_mismatch_report(struct TKVBaseMismatch data) {
    KVBaseMismatch kvm = KVBASE_MISMATCH__INIT;

    kvm.load = data.load;
    kvm.kv = data.kv;
    kvm.bus = data.bus;
    kvm.kvbase = data.kv_base;

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_KVM;
    report.kvm = &kvm;

    send_opendss_message(&report);
}

void send_isolated_elements_report(struct TIsolatedBusesReport data) {
    IsolatedBusesReport ibr = ISOLATED_BUSES_REPORT__INIT;

    // isolatedBuses first; pick 1024b arbitrarily as I've never seen bus names this long
    ibr.disconnectedbuses = malloc(data.num_buses * 1024);
    for (int i = 0; i < data.num_buses; i++) {
        ibr.disconnectedbuses[i] = data.isolated_buses[i];
    }

    // IsolatedAreas
    ibr.isolatedsubareas = malloc(data.num_areas * sizeof(IsolatedArea));
    ibr.n_isolatedsubareas = data.num_areas;
    for (int i = 0; i < data.num_areas; i++) {
        IsolatedArea area = ISOLATED_AREA__INIT;
        struct TIsolatedArea ia = data.isolated_area[i];

        area.level = ia.level;
        area.element = ia.element;
        area.loads = malloc(ia.num_loads * 1024);
        for (int j = 0; i < ia.num_loads; i++) {
            area.loads[j] = ia.loads[j];
        }

        IsolatedArea* a = (IsolatedArea*)malloc(sizeof(IsolatedArea));
        *a = area;
        ibr.isolatedsubareas[i] = a;
    }

    // IsolatedElements
    ibr.isolatedelements = malloc(data.num_elements * sizeof(IsolatedElement));
    ibr.n_isolatedelements = data.num_elements;
    for (int i = 0; i < data.num_elements; i++) {
        IsolatedElement element = ISOLATED_ELEMENT__INIT;
        struct TIsolatedElement ie = data.isolated_element[i];

        element.name = ie.name;
        element.buses = malloc(ie.num_buses * 1024);
        for (int j = 0; i < ie.num_buses; i++) {
            element.buses[j] = ie.buses[j];
        }

        IsolatedElement* el = (IsolatedElement*)malloc(sizeof(IsolatedElement));
        *el = element;

        ibr.isolatedelements[i] = el;
    }

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_IBR;
    report.ibr = &ibr;

    send_opendss_message(&report);
}

void send_final_opendss_report() {
    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    send_opendss_message(&report);
}
