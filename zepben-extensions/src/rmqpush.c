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

#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/tcp_socket.h>

#include "include/rmqpush.h"
#include "include/utils.h"
#include "proto/hc/opendss/Diagnostics.pb-c.h"
#include "proto/hc/opendss/EnergyMeter.pb-c.h"
#include "proto/hc/opendss/QueueMessage.pb-c.h"

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

void send_opendss_message(QueueMessage *message) {
    int len = queue_message__get_packed_size(message);
    void *buf = malloc(len);
    queue_message__pack(message, buf);

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
    report.maxkw = di.max_kw;
    report.maxkva = di.max_kva;
    report.zonekwh = di.zone_kwh;
    report.zonekvarh = di.zone_kvarh;
    report.zonemaxkw = di.zone_max_kw;
    report.zonemaxkva = di.zone_max_kva;
    report.overloadkwhnormal = di.overload_kwh_normal;
    report.overloadkwhemerg = di.overload_kwh_emerg;
    report.loadeen = di.load_een;
    report.loadue = di.load_ue;
    report.zonelosseskwh = di.zone_losses_kwh;
    report.zonelosseskvarh = di.zone_losses_kvarh;
    report.zonemaxkwlosses = di.zone_max_kw_losses;
    report.zonemaxkvarlosses = di.zone_max_kvar_losses;
    report.loadlosseskwh = di.load_losses_kwh;
    report.loadlosseskvarh = di.load_losses_kvarh;
    report.noloadlosseskwh = di.no_load_losses_kwh;
    report.noloadlosseskvarh = di.no_load_losses_kvarh;
    report.maxkwloadlosses = di.max_kw_load_losses;
    report.maxkwnoloadlosses = di.max_kw_no_load_losses;
    report.linelosses = di.line_losses;
    report.transformerlosses = di.transformer_losses;
    report.linemodelinelosses = di.line_mode_line_losses;
    report.zeromodelinelosses = di.zero_mode_line_losses;
    report.phaselinelosses3 = di.phase_line_losses_3;
    report.phaselinelosses12 = di.phase_line_losses_12;
    report.genkwh = di.gen_kwh;
    report.genkvarh = di.gen_kvarh;
    report.genmaxkw = di.gen_max_kw;
    report.genmaxkva = di.gen_max_kva;

    report.n_voltbases = di.num_volt_bases;
    report.voltbases = (VoltBaseRegisters**)malloc(di.num_volt_bases * sizeof(VoltBaseRegisters*));

    for (int i = 0; i < di.num_volt_bases; ++i) {
        VoltBaseRegisters volt_base = VOLT_BASE_REGISTERS__INIT;

        volt_base.vbase = diVoltBases[i].vbase;
        volt_base.kvlosses = diVoltBases[i].kv_losses;
        volt_base.kvlineloss = diVoltBases[i].kv_line_loss;
        volt_base.kvloadloss = diVoltBases[i].kv_load_loss;
        volt_base.kvnoloadloss = diVoltBases[i].kv_no_load_loss;
        volt_base.kvloadenergy = diVoltBases[i].kv_load_energy;

        VoltBaseRegisters *volt_base_ptr = (VoltBaseRegisters*)malloc(sizeof(VoltBaseRegisters));
        *volt_base_ptr = volt_base;

        report.voltbases[i] = volt_base_ptr;
    }

    QueueMessage queue_message = QUEUE_MESSAGE__INIT;
    queue_message.report_case = QUEUE_MESSAGE__REPORT_DI;
    queue_message.di = &report;

    send_opendss_message(&queue_message);

    for (int i = 0; i < di.num_volt_bases; ++i)
        free(report.voltbases[i]);
    free(report.voltbases);
}

MaxMinAvg *copyMaxMinAvg(struct TMaxMinAvg *source) {
    MaxMinAvg phs = MAX_MIN_AVG__INIT;
    MaxMinAvg *phs_ptr = (MaxMinAvg*)malloc(sizeof(MaxMinAvg));

    phs.max = source->max;
    phs.min = source->min;
    phs.avg = source->avg;

    *phs_ptr = phs;

    return phs_ptr;
}

// phvValues should be in the phv struct, but seems to cause issues, so pass it as a separate parameter.
void send_phase_voltage_report(struct TPhaseVoltageReport phv, struct TPhaseVoltageReportValues phvValues[]) {
    PhaseVoltageReport report = PHASE_VOLTAGE_REPORT__INIT;

    report.element = (char*)phv.element;
    report.hour = phv.hour;
    report.n_values = phv.num_values;
    report.values = (PhaseVoltageReportValues**)malloc(phv.num_values * sizeof(PhaseVoltageReportValues*));

    for (int i = 0; i < phv.num_values; ++i) {
        PhaseVoltageReportValues values = PHASE_VOLTAGE_REPORT_VALUES__INIT;

        values.vbase = phvValues[i].vbase;
        values.phs1 = copyMaxMinAvg(&phvValues[i].phs1);
        values.phs2 = copyMaxMinAvg(&phvValues[i].phs2);
        values.phs3 = copyMaxMinAvg(&phvValues[i].phs3);

        PhaseVoltageReportValues *values_ptr = (PhaseVoltageReportValues*)malloc(sizeof(PhaseVoltageReportValues));
        *values_ptr = values;

        report.values[i] = values_ptr;
    }

    QueueMessage queue_message = QUEUE_MESSAGE__INIT;
    queue_message.report_case = QUEUE_MESSAGE__REPORT_PHV;
    queue_message.phv = &report;

    send_opendss_message(&queue_message);

    for (int i = 0; i < phv.num_values; ++i) {
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
    report.normalamps = ov.normal_amps;
    report.emergamps = ov.emerg_amps;
    report.percentnormal = ov.percent_normal;
    report.percentemerg = ov.percent_emerg;
    report.kvbase = ov.kv_base;
    report.phase1amps = ov.phase1_amps;
    report.phase2amps = ov.phase2_amps;
    report.phase3amps = ov.phase3_amps;

    QueueMessage queue_message = QUEUE_MESSAGE__INIT;
    queue_message.report_case = QUEUE_MESSAGE__REPORT_OV;
    queue_message.ov = &report;

    send_opendss_message(&queue_message);
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

    QueueMessage queue_message = QUEUE_MESSAGE__INIT;
    queue_message.report_case = QUEUE_MESSAGE__REPORT_VR;
    queue_message.vr = &report;

    send_opendss_message(&queue_message);
}

void send_summary_report(struct TSummaryReport sr) {
    SummaryReport report = SUMMARY_REPORT__INIT;

    report.timestamp = time(NULL);
    report.circuitname = sr.circuit_name;
    report.solved = sr.solved;
    report.mode = sr.mode;
    report.number = sr.number;
    report.loadmult = sr.load_mult;
    report.numdevices = sr.num_devices;
    report.numbuses = sr.num_buses;
    report.numnodes = sr.num_nodes;
    report.iterations = sr.iterations;
    report.controlmode = sr.control_mode;
    report.controliterations = sr.control_iterations;
    report.mostiterationsdone = sr.most_iterations_done;
    report.year = sr.year;
    report.hour = sr.hour;
    report.maxpuvoltage = sr.max_pu_voltage;
    report.minpuvoltage = sr.min_pu_voltage;
    report.totalmw = sr.total_mw;
    report.totalmvar = sr.total_mvar;
    report.mwlosses = sr.mw_losses;
    report.pctlosses = sr.pct_losses;
    report.mvarlosses = sr.mvar_losses;
    report.frequency = sr.frequency;

    QueueMessage queue_message = QUEUE_MESSAGE__INIT;
    queue_message.report_case = QUEUE_MESSAGE__REPORT_SR;
    queue_message.sr = &report;

    send_opendss_message(&queue_message);
}

void send_taps_report(struct TTapReport tp) {
    TapsReport report = TAPS_REPORT__INIT;

    report.name = tp.name;
    report.tap = tp.tap;
    report.min = tp.min;
    report.max = tp.max;
    report.step = tp.step;
    report.position = tp.position;

    QueueMessage queue_message = QUEUE_MESSAGE__INIT;
    queue_message.report_case = QUEUE_MESSAGE__REPORT_TR;
    queue_message.tr = &report;

    send_opendss_message(&queue_message);
}

void send_eventlog(struct TEventLog *el, int num_events) {
    EventLog log = EVENT_LOG__INIT;

    log.logentry = (EventLogEntry**)malloc(num_events * sizeof(EventLogEntry*));

    for (int i = 0; i < num_events; i++) {
        EventLogEntry log_entry = EVENT_LOG_ENTRY__INIT;
        log_entry.hour = el[i].hour;
        log_entry.sec = el[i].sec;
        log_entry.controliter = el[i].control_iter;
        log_entry.iteration = el[i].iteration;
        log_entry.element = el[i].element;
        log_entry.event = el[i].event;
        log_entry.action = el[i].action;

        EventLogEntry *entry = (EventLogEntry*)malloc(sizeof(EventLogEntry));
        *entry = log_entry;
        log.logentry[i] = entry;
    }

    QueueMessage queue_message = QUEUE_MESSAGE__INIT;
    queue_message.report_case = QUEUE_MESSAGE__REPORT_EL;
    queue_message.el = &log;

    send_opendss_message(&queue_message);

    for (int i = 0; i < num_events; i++) {
        free(log.logentry[i]);
    }
    free(log.logentry);
}

void send_loop_report(struct TLoopReport lr) {
    LoopReport loop_report = LOOP_REPORT__INIT;

    loop_report.meter = lr.meter;
    loop_report.linea = lr.lineA;
    loop_report.lineb = lr.lineB;
    loop_report.parallel = lr.parallel;
    loop_report.looped = lr.looped;

    QueueMessage queue_message = QUEUE_MESSAGE__INIT;
    queue_message.report_case = QUEUE_MESSAGE__REPORT_LR;
    queue_message.lr = &loop_report;

    send_opendss_message(&queue_message);
}

void send_losses_entry(struct TLossesEntry le) {
    LossesEntry loss_entry = LOSSES_ENTRY__INIT;

    loss_entry.element = le.element;
    loss_entry.kwlosses = le.kw_losses;
    loss_entry.pctpower = le.pct_power;
    loss_entry.kvarlosses = le.kvar_losses;

    QueueMessage queue_message = QUEUE_MESSAGE__INIT;
    queue_message.report_case = QUEUE_MESSAGE__REPORT_LE;
    queue_message.le = &loss_entry;

    send_opendss_message(&queue_message);
}

void send_losses_totals(struct TLossesTotals lt) {
    LossesTotals totals = LOSSES_TOTALS__INIT;

    totals.totalpctlosses = lt.total_pct_losses;
    totals.totalloadpower = lt.total_load_power;
    totals.transformerlosses = lt.transformer_losses;
    totals.linelosses = lt.line_losses;
    totals.totallosses = lt.total_losses;

    QueueMessage queue_message = QUEUE_MESSAGE__INIT;
    queue_message.report_case = QUEUE_MESSAGE__REPORT_LOSSES;
    queue_message.losses = &totals;

    send_opendss_message(&queue_message);
}

void send_node_mismatch_report(struct TNodeMismatch nm) {
    NodeMismatch mismatch = NODE_MISMATCH__INIT;

    mismatch.bus = nm.bus;
    mismatch.node = nm.node;
    mismatch.currentsum = nm.current_sum;
    mismatch.pcterror = nm.pct_error;
    mismatch.maxcurrent = nm.max_current;

    QueueMessage queue_message = QUEUE_MESSAGE__INIT;
    queue_message.report_case = QUEUE_MESSAGE__REPORT_NM;
    queue_message.nm = &mismatch;

    send_opendss_message(&queue_message);
}

void send_kvbase_mismatch_report(struct TKVBaseMismatch kvm) {
    KVBaseMismatch mismatch = KVBASE_MISMATCH__INIT;

    mismatch.load = kvm.load;
    mismatch.kv = kvm.kv;
    mismatch.bus = kvm.bus;
    mismatch.kvbase = kvm.kv_base;

    QueueMessage queue_message = QUEUE_MESSAGE__INIT;
    queue_message.report_case = QUEUE_MESSAGE__REPORT_KVM;
    queue_message.kvm = &mismatch;

    send_opendss_message(&queue_message);
}

void send_isolated_elements_report(struct TIsolatedBusesReport ib) {
    IsolatedBusesReport report = ISOLATED_BUSES_REPORT__INIT;

    // isolatedBuses first; pick 1024b arbitrarily as I've never seen bus names this long
    report.disconnectedbuses = malloc(ib.num_buses * 1024);
    for (int i = 0; i < ib.num_buses; i++) {
        report.disconnectedbuses[i] = ib.isolated_buses[i];
    }

    // IsolatedAreas
    report.isolatedsubareas = malloc(ib.num_areas * sizeof(IsolatedArea));
    for (int i = 0; i < ib.num_areas; i++) {
        IsolatedArea area = ISOLATED_AREA__INIT;
        struct TIsolatedArea ia = ib.isolated_area[i];

        area.level = ia.level;
        area.element = ia.element;
        area.loads = malloc(ia.num_loads * 1024);
        for (int j = 0; i < ia.num_loads; i++) {
            area.loads[j] = ia.loads[j];
        }

        IsolatedArea *a = (IsolatedArea *)malloc(sizeof(IsolatedArea));
        *a = area;
        report.isolatedsubareas[i] = a;
    }

    // IsolatedElements
    report.isolatedelements = malloc(ib.num_elements * sizeof(IsolatedElement));
    for (int i = 0; i < ib.num_elements; i++) {
        IsolatedElement element = ISOLATED_ELEMENT__INIT;
        struct TIsolatedElement ie = ib.isolated_element[i];

        element.name = ie.name;
        element.buses = malloc(ie.num_buses * 1024);
        for (int j = 0; i < ie.num_buses; i++) {
            element.buses[j] = ie.buses[j];
        }

        IsolatedElement *el = (IsolatedElement*)malloc(sizeof(IsolatedElement));
        *el = element;

        report.isolatedelements[i] = el;
    }

    QueueMessage queue_message = QUEUE_MESSAGE__INIT;
    queue_message.report_case = QUEUE_MESSAGE__REPORT_IBR;
    queue_message.ibr = &report;

    send_opendss_message(&queue_message);

    // free stuff
    free(report.disconnectedbuses);

    for (int i = 0; i < ib.num_areas; i++) {
        free(report.isolatedsubareas[i]->loads);
        free(report.isolatedsubareas[i]);
    }
    free(report.isolatedsubareas);

    for (int i = 0; i < ib.num_elements; i++) {
        free(report.isolatedelements[i]->buses);
        free(report.isolatedelements[i]);
    }
    free(report.isolatedelements);
}
