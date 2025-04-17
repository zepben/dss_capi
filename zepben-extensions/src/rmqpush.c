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

#define REPORT_BATCH_SIZE 10

// message batching
static OpenDssReport* reports[REPORT_BATCH_SIZE];
static OpenDssReportBatch open_dss_report_batch = {
    PROTOBUF_C_MESSAGE_INIT (&open_dss_report_batch__descriptor),
    0,
    reports
};

char* copy_str(const char* str) {
    if (!str) return NULL;
    char* copy = malloc(strlen(str) + 1);
    if (copy) strcpy(copy, str);
    return copy;
}

void free_opendss_report(OpenDssReport* report) {
    switch (report->report_case) {
    OPEN_DSS_REPORT__REPORT_DI:
        free(report->di->element);
        for (size_t i = 0; i < report->di->n_voltbases; i++) free(report->di->voltbases[i]);
        free(report->di->voltbases);
        free(report->di);
        break;
    OPEN_DSS_REPORT__REPORT_PHV:
        free(report->phv->element);
        for (size_t i = 0; i < report->phv->n_values; i++) {
            free(report->phv->values[i]->phs1);
            free(report->phv->values[i]->phs2);
            free(report->phv->values[i]->phs3);
            free(report->phv->values[i]);
        }
        free(report->phv->values);
        free(report->phv);
        break;
    OPEN_DSS_REPORT__REPORT_OV:
        free(report->ov->element);
        free(report->ov);
        break;
    OPEN_DSS_REPORT__REPORT_VR:
        free(report->vr->hv->minbus);
        free(report->vr->hv->maxbus);
        free(report->vr->hv);
        free(report->vr->lv->minbus);
        free(report->vr->lv->maxbus);
        free(report->vr->lv);
        free(report->vr);
        break;
    OPEN_DSS_REPORT__REPORT_SR:
        free(report->sr->casename);
        free(report->sr->mode);
        free(report->sr->controlmode);
        free(report->sr);
        break;
    OPEN_DSS_REPORT__REPORT_TR:
        free(report->tr->name);
        free(report->tr);
        break;
    OPEN_DSS_REPORT__REPORT_EL:
        for (size_t i = 0; i < report->el->n_logentry; i++) {
            free(report->el->logentry[i]->element);
            free(report->el->logentry[i]->event);
            free(report->el->logentry[i]->action);
            free(report->el->logentry[i]);
        }
        free(report->el->logentry);
        free(report->el);
        break;
    OPEN_DSS_REPORT__REPORT_LR:
        free(report->lr->meter);
        free(report->lr->linea);
        free(report->lr->lineb);
        free(report->lr);
        break;
    OPEN_DSS_REPORT__REPORT_IBR:
        for (size_t i = 0; i < report->ibr->n_disconnectedbuses; i++) {
            free(report->ibr->disconnectedbuses[i]);
        }
        free(report->ibr->disconnectedbuses);

        for (size_t i = 0; i < report->ibr->n_isolatedsubareas; i++) {
            free(report->ibr->isolatedsubareas[i]->element);
            for (size_t j = 0; j < report->ibr->isolatedsubareas[i]->n_loads; j++) {
                free(report->ibr->isolatedsubareas[i]->loads[j]);
            }
            free(report->ibr->isolatedsubareas[i]);
        }
        free(report->ibr->isolatedsubareas);

        for (size_t i = 0; i < report->ibr->n_isolatedelements; i++) {
            free(report->ibr->isolatedelements[i]->name);
            for (size_t j = 0; j < report->ibr->isolatedelements[i]->n_buses; j++) {
                free(report->ibr->isolatedelements[i]->buses[j]);
            }
            free(report->ibr->isolatedelements[i]);
        }
        free(report->ibr->isolatedelements);

        free(report->ibr);
        break;
    OPEN_DSS_REPORT__REPORT_LE:
        free(report->le->element);
        free(report->le);
        break;
    OPEN_DSS_REPORT__REPORT_LOSSES:
        free(report->losses);
        break;
    OPEN_DSS_REPORT__REPORT_NM:
        free(report->nm->bus);
        free(report->nm);
        break;
    OPEN_DSS_REPORT__REPORT_KVM:
        free(report->kvm->load);
        free(report->kvm->bus);
        free(report->kvm);
        break;
    default:
        break;
    }

    free(report);
}

void send_opendss_report_batch() {
    if (open_dss_report_batch.n_reports == 0) return;

    int len = open_dss_report_batch__get_packed_size(&open_dss_report_batch);
    void* buf = malloc(len);
    open_dss_report_batch__pack(&open_dss_report_batch, buf);

    stream_out_message(buf, len);

    free(buf);

    for (size_t i = 0; i < open_dss_report_batch.n_reports; i++) free_opendss_report(reports[i]);
    open_dss_report_batch.n_reports = 0;
}

// Push a report onto the batch. `report` is assumed to point to a value on the stack, so it is copied to the heap here.
// However, the caller is responsible for allocating heap space for the report's pointers (strings, repeated fields).
void batch_push_opendss_report(OpenDssReport* report) {
    reports[open_dss_report_batch.n_reports] = malloc(sizeof(OpenDssReport));
    *(reports[open_dss_report_batch.n_reports]) = *report;
    if (++open_dss_report_batch.n_reports == REPORT_BATCH_SIZE) send_opendss_report_batch();
}

void batch_push_demand_interval_report(struct TDemandIntervalReport data) {
    DemandIntervalReport* di = malloc(sizeof(DemandIntervalReport));
    *di = (DemandIntervalReport)DEMAND_INTERVAL_REPORT__INIT;

    di->element = copy_str(data.element);
    di->hour = data.hour;
    di->kwh = data.kwh;
    di->kvarh = data.kvarh;
    di->maxkw = data.max_kw;
    di->maxkva = data.max_kva;
    di->zonekwh = data.zone_kwh;
    di->zonekvarh = data.zone_kvarh;
    di->zonemaxkw = data.zone_max_kw;
    di->zonemaxkva = data.zone_max_kva;
    di->overloadkwhnormal = data.overload_kwh_normal;
    di->overloadkwhemerg = data.overload_kwh_emerg;
    di->loadeen = data.load_een;
    di->loadue = data.load_ue;
    di->zonelosseskwh = data.zone_losses_kwh;
    di->zonelosseskvarh = data.zone_losses_kvarh;
    di->zonemaxkwlosses = data.zone_max_kw_losses;
    di->zonemaxkvarlosses = data.zone_max_kvar_losses;
    di->loadlosseskwh = data.load_losses_kwh;
    di->loadlosseskvarh = data.load_losses_kvarh;
    di->noloadlosseskwh = data.no_load_losses_kwh;
    di->noloadlosseskvarh = data.no_load_losses_kvarh;
    di->maxkwloadlosses = data.max_kw_load_losses;
    di->maxkwnoloadlosses = data.max_kw_no_load_losses;
    di->linelosses = data.line_losses;
    di->transformerlosses = data.transformer_losses;
    di->linemodelinelosses = data.line_mode_line_losses;
    di->zeromodelinelosses = data.zero_mode_line_losses;
    di->phaselinelosses3 = data.phase_line_losses_3;
    di->phaselinelosses12 = data.phase_line_losses_12;
    di->genkwh = data.gen_kwh;
    di->genkvarh = data.gen_kvarh;
    di->genmaxkw = data.gen_max_kw;
    di->genmaxkva = data.gen_max_kva;

    di->n_voltbases = data.num_volt_bases;
    di->voltbases = (VoltBaseRegisters**)malloc(data.num_volt_bases * sizeof(VoltBaseRegisters*));

    for (int i = 0; i < data.num_volt_bases; i++) {
        VoltBaseRegisters* volt_base = malloc(sizeof(VoltBaseRegisters));
        *volt_base = (VoltBaseRegisters)VOLT_BASE_REGISTERS__INIT;

        volt_base->vbase = data.volt_bases[i].vbase;
        volt_base->kvlosses = data.volt_bases[i].kv_losses;
        volt_base->kvlineloss = data.volt_bases[i].kv_line_loss;
        volt_base->kvloadloss = data.volt_bases[i].kv_load_loss;
        volt_base->kvnoloadloss = data.volt_bases[i].kv_no_load_loss;
        volt_base->kvloadenergy = data.volt_bases[i].kv_load_energy;

        di->voltbases[i] = volt_base;
    }

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_DI;
    report.di = di;

    batch_push_opendss_report(&report);
}

MaxMinAvg* copyMaxMinAvg(struct TMaxMinAvg* source) {
    MaxMinAvg* phs = malloc(sizeof(MaxMinAvg));
    *phs = (MaxMinAvg)MAX_MIN_AVG__INIT;

    phs->max = source->max;
    phs->min = source->min;
    phs->avg = source->avg;

    return phs;
}

void batch_push_phase_voltage_report(struct TPhaseVoltageReport data) {
    PhaseVoltageReport* phv = malloc(sizeof(PhaseVoltageReport));
    *phv = (PhaseVoltageReport)PHASE_VOLTAGE_REPORT__INIT;

    phv->element = copy_str(data.element);
    phv->hour = data.hour;
    phv->n_values = data.num_values;
    phv->values = (PhaseVoltageReportValues**)malloc(data.num_values * sizeof(PhaseVoltageReportValues*));

    for (int i = 0; i < data.num_values; i++) {
        PhaseVoltageReportValues* values = malloc(sizeof(PhaseVoltageReportValues));
        *values = (PhaseVoltageReportValues)PHASE_VOLTAGE_REPORT_VALUES__INIT;

        values->vbase = data.values[i].vbase;
        values->phs1 = copyMaxMinAvg(&data.values[i].phs1);
        values->phs2 = copyMaxMinAvg(&data.values[i].phs2);
        values->phs3 = copyMaxMinAvg(&data.values[i].phs3);

        phv->values[i] = values;
    }

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_PHV;
    report.phv = phv;

    batch_push_opendss_report(&report);
}

void batch_push_overload_report(struct TOverloadReport data) {
    OverloadReport* ov = malloc(sizeof(OverloadReport));
    *ov = (OverloadReport)OVERLOAD_REPORT__INIT;

    ov->hour = data.hour;
    ov->element = copy_str(data.element);
    ov->normalamps = data.normal_amps;
    ov->emergamps = data.emerg_amps;
    ov->percentnormal = data.percent_normal;
    ov->percentemerg = data.percent_emerg;
    ov->kvbase = data.kv_base;
    ov->phase1amps = data.phase1_amps;
    ov->phase2amps = data.phase2_amps;
    ov->phase3amps = data.phase3_amps;

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_OV;
    report.ov = ov;

    batch_push_opendss_report(&report);
}

void batch_push_voltage_report(struct TVoltageReport data) {
    VoltageReport* vr = malloc(sizeof(VoltageReport));
    VoltageReportValues* hv = malloc(sizeof(VoltageReportValues));
    VoltageReportValues* lv = malloc(sizeof(VoltageReportValues));
    *vr = (VoltageReport)VOLTAGE_REPORT__INIT;
    *hv = (VoltageReportValues)VOLTAGE_REPORT_VALUES__INIT;
    *lv = (VoltageReportValues)VOLTAGE_REPORT_VALUES__INIT;

    vr->hour = data.hour;
    vr->hv = hv;
    vr->lv = lv;

    hv->undervoltages = data.hv.under_voltages;
    hv->minvoltage = data.hv.min_voltage;
    hv->overvoltage = data.hv.over_voltage;
    hv->maxvoltage = data.hv.max_voltage;
    hv->minbus = copy_str(data.hv.min_bus);
    hv->maxbus = copy_str(data.hv.max_bus);

    lv->undervoltages = data.lv.under_voltages;
    lv->minvoltage = data.lv.min_voltage;
    lv->overvoltage = data.lv.over_voltage;
    lv->maxvoltage = data.lv.max_voltage;
    lv->minbus = copy_str(data.lv.min_bus);
    lv->maxbus = copy_str(data.lv.max_bus);

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_VR;
    report.vr = vr;

    batch_push_opendss_report(&report);
}

void batch_push_summary_report(struct TSummaryReport data) {
    SummaryReport* sr = malloc(sizeof(SummaryReport));
    *sr = (SummaryReport)SUMMARY_REPORT__INIT;

    sr->casename = copy_str(data.case_name);
    sr->solved = data.solved;
    sr->mode = copy_str(data.mode);
    sr->number = data.number;
    sr->loadmult = data.load_mult;
    sr->numdevices = data.num_devices;
    sr->numbuses = data.num_buses;
    sr->numnodes = data.num_nodes;
    sr->iterations = data.iterations;
    sr->controlmode = copy_str(data.control_mode);
    sr->controliterations = data.control_iterations;
    sr->mostiterationsdone = data.most_iterations_done;
    sr->year = data.year;
    sr->hour = data.hour;
    sr->maxpuvoltage = data.max_pu_voltage;
    sr->minpuvoltage = data.min_pu_voltage;
    sr->totalmw = data.total_mw;
    sr->totalmvar = data.total_mvar;
    sr->mwlosses = data.mw_losses;
    sr->pctlosses = data.pct_losses;
    sr->mvarlosses = data.mvar_losses;
    sr->frequency = data.frequency;

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_SR;
    report.sr = sr;

    batch_push_opendss_report(&report);
}

void batch_push_taps_report(struct TTapsReport data) {
    TapsReport* tr = malloc(sizeof(TapsReport));
    *tr = (TapsReport)TAPS_REPORT__INIT;

    tr->name = copy_str(data.name);
    tr->tap = data.tap;
    tr->min = data.min;
    tr->max = data.max;
    tr->step = data.step;
    tr->position = data.position;

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_TR;
    report.tr = tr;

    batch_push_opendss_report(&report);
}

void batch_push_eventlog(struct TEventLog* data, int num_events) {
    EventLog* el = malloc(sizeof(EventLog));
    *el = (EventLog)EVENT_LOG__INIT;

    el->logentry = (EventLogEntry**)malloc(num_events * sizeof(EventLogEntry*));
    el->n_logentry = num_events;

    for (int i = 0; i < num_events; i++) {
        EventLogEntry* log_entry = malloc(sizeof(EventLogEntry));
        *log_entry = (EventLogEntry)EVENT_LOG_ENTRY__INIT;

        log_entry->hour = data[i].hour;
        log_entry->sec = data[i].sec;
        log_entry->controliter = data[i].control_iter;
        log_entry->iteration = data[i].iteration;
        log_entry->element = copy_str(data[i].element);
        log_entry->event = copy_str(data[i].event);
        log_entry->action = copy_str(data[i].action);

        el->logentry[i] = log_entry;
    }

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_EL;
    report.el = el;

    batch_push_opendss_report(&report);
}

void batch_push_loop_report(struct TLoopReport data) {
    LoopReport* lr = malloc(sizeof(LoopReport));
    *lr = (LoopReport)LOOP_REPORT__INIT;

    lr->meter = copy_str(data.meter);
    lr->linea = copy_str(data.line_a);
    lr->lineb = copy_str(data.line_b);
    lr->parallel = data.parallel;
    lr->looped = data.looped;

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_LR;
    report.lr = lr;

    batch_push_opendss_report(&report);
}

void batch_push_isolated_elements_report(struct TIsolatedBusesReport data) {
    IsolatedBusesReport* ibr = malloc(sizeof(IsolatedBusesReport));
    *ibr = (IsolatedBusesReport)ISOLATED_BUSES_REPORT__INIT;

    ibr->disconnectedbuses = malloc(data.num_buses * sizeof(char*));
    ibr->n_disconnectedbuses = data.num_buses;
    for (int i = 0; i < data.num_buses; i++) {
        ibr->disconnectedbuses[i] = copy_str(data.isolated_buses[i]);
    }

    // IsolatedAreas
    ibr->isolatedsubareas = malloc(data.num_areas * sizeof(IsolatedArea));
    ibr->n_isolatedsubareas = data.num_areas;
    for (int i = 0; i < data.num_areas; i++) {
        IsolatedArea* area = malloc(sizeof(IsolatedArea));
        *area = (IsolatedArea)ISOLATED_AREA__INIT;
        struct TIsolatedArea ia = data.isolated_area[i];

        area->level = ia.level;
        area->element = copy_str(ia.element);
        area->loads = malloc(ia.num_loads * sizeof(char*));
        area->n_loads = ia.num_loads;
        for (int j = 0; i < ia.num_loads; i++) {
            area->loads[j] = copy_str(ia.loads[j]);
        }

        ibr->isolatedsubareas[i] = area;
    }

    // IsolatedElements
    ibr->isolatedelements = malloc(data.num_elements * sizeof(IsolatedElement));
    ibr->n_isolatedelements = data.num_elements;
    for (int i = 0; i < data.num_elements; i++) {
        IsolatedElement* element = malloc(sizeof(IsolatedElement));
        *element = (IsolatedElement)ISOLATED_ELEMENT__INIT;
        struct TIsolatedElement ie = data.isolated_element[i];

        element->name = copy_str(ie.name);
        element->buses = malloc(ie.num_buses * sizeof(char*));
        element->n_buses = ie.num_buses;
        for (int j = 0; i < ie.num_buses; i++) {
            element->buses[j] = copy_str(ie.buses[j]);
        }

        ibr->isolatedelements[i] = element;
    }

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_IBR;
    report.ibr = ibr;

    batch_push_opendss_report(&report);
}

void batch_push_losses_entry(struct TLossesEntry data) {
    LossesEntry* le = malloc(sizeof(LossesEntry));
    *le = (LossesEntry)LOSSES_ENTRY__INIT;

    le->element = copy_str(data.element);
    le->kwlosses = data.kw_losses;
    le->pctpower = data.pct_power;
    le->kvarlosses = data.kvar_losses;

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_LE;
    report.le = le;

    batch_push_opendss_report(&report);
}

void batch_push_losses_totals(struct TLossesTotals data) {
    LossesTotals* losses = malloc(sizeof(LossesTotals));
    *losses = (LossesTotals)LOSSES_TOTALS__INIT;

    losses->totalpctlosses = data.total_pct_losses;
    losses->totalloadpower = data.total_load_power;
    losses->transformerlosses = data.transformer_losses;
    losses->linelosses = data.line_losses;
    losses->totallosses = data.total_losses;

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_LOSSES;
    report.losses = losses;

    batch_push_opendss_report(&report);
}

void batch_push_node_mismatch_report(struct TNodeMismatch data) {
    NodeMismatch* nm = malloc(sizeof(NodeMismatch));
    *nm = (NodeMismatch)NODE_MISMATCH__INIT;

    nm->bus = copy_str(data.bus);
    nm->node = data.node;
    nm->currentsum = data.current_sum;
    nm->pcterror = data.pct_error;
    nm->maxcurrent = data.max_current;

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_NM;
    report.nm = nm;

    batch_push_opendss_report(&report);
}

void batch_push_kvbase_mismatch_report(struct TKVBaseMismatch data) {
    KVBaseMismatch* kvm = malloc(sizeof(KVBaseMismatch));
    *kvm = (KVBaseMismatch)KVBASE_MISMATCH__INIT;

    kvm->load = copy_str(data.load);
    kvm->kv = data.kv;
    kvm->bus = copy_str(data.bus);
    kvm->kvbase = data.kv_base;

    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_KVM;
    report.kvm = kvm;

    batch_push_opendss_report(&report);
}

void send_final_opendss_report(bool failure) {
    OpenDssReport report = OPEN_DSS_REPORT__INIT;
    report.report_case = OPEN_DSS_REPORT__REPORT_FAILURE;
    report.failure = failure;
    
    batch_push_opendss_report(&report);
    send_opendss_report_batch();
}
