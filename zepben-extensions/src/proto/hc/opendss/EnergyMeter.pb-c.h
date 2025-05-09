/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: hc/opendss/EnergyMeter.proto */

#ifndef PROTOBUF_C_hc_2fopendss_2fEnergyMeter_2eproto__INCLUDED
#define PROTOBUF_C_hc_2fopendss_2fEnergyMeter_2eproto__INCLUDED

#include <protobuf-c/protobuf-c.h>

PROTOBUF_C__BEGIN_DECLS

#if PROTOBUF_C_VERSION_NUMBER < 1003000
# error This file was generated by a newer version of protobuf-c which is incompatible with your libprotobuf-c headers. Please update your headers.
#elif 1005001 < PROTOBUF_C_MIN_COMPILER_VERSION
# error This file was generated by an older version of protobuf-c which is incompatible with your libprotobuf-c headers. Please regenerate this file with a newer version of protobuf-c.
#endif


typedef struct VoltBaseRegisters VoltBaseRegisters;
typedef struct DemandIntervalReport DemandIntervalReport;
typedef struct MaxMinAvg MaxMinAvg;
typedef struct PhaseVoltageReportValues PhaseVoltageReportValues;
typedef struct PhaseVoltageReport PhaseVoltageReport;
typedef struct OverloadReport OverloadReport;
typedef struct VoltageReportValues VoltageReportValues;
typedef struct VoltageReport VoltageReport;


/* --- enums --- */


/* --- messages --- */

/*
 **
 * Repeated values for the DemandIntervalReport.
 */
struct  VoltBaseRegisters
{
  ProtobufCMessage base;
  double vbase;
  double kvlosses;
  double kvlineloss;
  double kvloadloss;
  double kvnoloadloss;
  double kvloadenergy;
};
#define VOLT_BASE_REGISTERS__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&volt_base_registers__descriptor) \
    , 0, 0, 0, 0, 0, 0 }


/*
 **
 * Message for streaming the records written to DI_MHandle in WriteDemandIntervalData (top).
 */
struct  DemandIntervalReport
{
  ProtobufCMessage base;
  char *element;
  double hour;
  double kwh;
  double kvarh;
  double maxkw;
  double maxkva;
  double zonekwh;
  double zonekvarh;
  double zonemaxkw;
  double zonemaxkva;
  double overloadkwhnormal;
  double overloadkwhemerg;
  double loadeen;
  double loadue;
  double zonelosseskwh;
  double zonelosseskvarh;
  double zonemaxkwlosses;
  double zonemaxkvarlosses;
  double loadlosseskwh;
  double loadlosseskvarh;
  double noloadlosseskwh;
  double noloadlosseskvarh;
  double maxkwloadlosses;
  double maxkwnoloadlosses;
  double linelosses;
  double transformerlosses;
  double linemodelinelosses;
  double zeromodelinelosses;
  double phaselinelosses3;
  double phaselinelosses12;
  double genkwh;
  double genkvarh;
  double genmaxkw;
  double genmaxkva;
  size_t n_voltbases;
  VoltBaseRegisters **voltbases;
};
#define DEMAND_INTERVAL_REPORT__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&demand_interval_report__descriptor) \
    , (char *)protobuf_c_empty_string, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,NULL }


/*
 **
 * Repeated values for the TPhaseVoltageReportValues.
 */
struct  MaxMinAvg
{
  ProtobufCMessage base;
  double max;
  double min;
  double avg;
};
#define MAX_MIN_AVG__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&max_min_avg__descriptor) \
    , 0, 0, 0 }


/*
 **
 * Repeated values for the PhaseVoltageReport.
 */
struct  PhaseVoltageReportValues
{
  ProtobufCMessage base;
  double vbase;
  MaxMinAvg *phs1;
  MaxMinAvg *phs2;
  MaxMinAvg *phs3;
};
#define PHASE_VOLTAGE_REPORT_VALUES__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&phase_voltage_report_values__descriptor) \
    , 0, NULL, NULL, NULL }


/*
 **
 * Message for streaming the records written to PHV_MHandle in WriteDemandIntervalData (bottom).
 */
struct  PhaseVoltageReport
{
  ProtobufCMessage base;
  char *element;
  double hour;
  /*
   * This column is declared in OpenDSS, but it is never populated, so we wont include it. string minBus = 4;
   * This column is declared in OpenDSS, but it is never populated, so we wont include it. string maxBus = 5;
   */
  size_t n_values;
  PhaseVoltageReportValues **values;
};
#define PHASE_VOLTAGE_REPORT__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&phase_voltage_report__descriptor) \
    , (char *)protobuf_c_empty_string, 0, 0,NULL }


/*
 **
 * Message for streaming the records written to OV_MHandle in WriteOverloadReport.
 */
struct  OverloadReport
{
  ProtobufCMessage base;
  double hour;
  char *element;
  double normalamps;
  double emergamps;
  double percentnormal;
  double percentemerg;
  double kvbase;
  double phase1amps;
  double phase2amps;
  double phase3amps;
};
#define OVERLOAD_REPORT__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&overload_report__descriptor) \
    , 0, (char *)protobuf_c_empty_string, 0, 0, 0, 0, 0, 0, 0, 0 }


/*
 **
 * Repeated values for the VoltageReport.
 */
struct  VoltageReportValues
{
  ProtobufCMessage base;
  int32_t undervoltages;
  double minvoltage;
  int32_t overvoltage;
  double maxvoltage;
  char *minbus;
  char *maxbus;
};
#define VOLTAGE_REPORT_VALUES__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&voltage_report_values__descriptor) \
    , 0, 0, 0, 0, (char *)protobuf_c_empty_string, (char *)protobuf_c_empty_string }


/*
 **
 * Message for streaming the records written to VR_MHandle in WriteVoltageReport.
 */
struct  VoltageReport
{
  ProtobufCMessage base;
  double hour;
  VoltageReportValues *hv;
  VoltageReportValues *lv;
};
#define VOLTAGE_REPORT__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&voltage_report__descriptor) \
    , 0, NULL, NULL }


/* VoltBaseRegisters methods */
void   volt_base_registers__init
                     (VoltBaseRegisters         *message);
size_t volt_base_registers__get_packed_size
                     (const VoltBaseRegisters   *message);
size_t volt_base_registers__pack
                     (const VoltBaseRegisters   *message,
                      uint8_t             *out);
size_t volt_base_registers__pack_to_buffer
                     (const VoltBaseRegisters   *message,
                      ProtobufCBuffer     *buffer);
VoltBaseRegisters *
       volt_base_registers__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   volt_base_registers__free_unpacked
                     (VoltBaseRegisters *message,
                      ProtobufCAllocator *allocator);
/* DemandIntervalReport methods */
void   demand_interval_report__init
                     (DemandIntervalReport         *message);
size_t demand_interval_report__get_packed_size
                     (const DemandIntervalReport   *message);
size_t demand_interval_report__pack
                     (const DemandIntervalReport   *message,
                      uint8_t             *out);
size_t demand_interval_report__pack_to_buffer
                     (const DemandIntervalReport   *message,
                      ProtobufCBuffer     *buffer);
DemandIntervalReport *
       demand_interval_report__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   demand_interval_report__free_unpacked
                     (DemandIntervalReport *message,
                      ProtobufCAllocator *allocator);
/* MaxMinAvg methods */
void   max_min_avg__init
                     (MaxMinAvg         *message);
size_t max_min_avg__get_packed_size
                     (const MaxMinAvg   *message);
size_t max_min_avg__pack
                     (const MaxMinAvg   *message,
                      uint8_t             *out);
size_t max_min_avg__pack_to_buffer
                     (const MaxMinAvg   *message,
                      ProtobufCBuffer     *buffer);
MaxMinAvg *
       max_min_avg__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   max_min_avg__free_unpacked
                     (MaxMinAvg *message,
                      ProtobufCAllocator *allocator);
/* PhaseVoltageReportValues methods */
void   phase_voltage_report_values__init
                     (PhaseVoltageReportValues         *message);
size_t phase_voltage_report_values__get_packed_size
                     (const PhaseVoltageReportValues   *message);
size_t phase_voltage_report_values__pack
                     (const PhaseVoltageReportValues   *message,
                      uint8_t             *out);
size_t phase_voltage_report_values__pack_to_buffer
                     (const PhaseVoltageReportValues   *message,
                      ProtobufCBuffer     *buffer);
PhaseVoltageReportValues *
       phase_voltage_report_values__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   phase_voltage_report_values__free_unpacked
                     (PhaseVoltageReportValues *message,
                      ProtobufCAllocator *allocator);
/* PhaseVoltageReport methods */
void   phase_voltage_report__init
                     (PhaseVoltageReport         *message);
size_t phase_voltage_report__get_packed_size
                     (const PhaseVoltageReport   *message);
size_t phase_voltage_report__pack
                     (const PhaseVoltageReport   *message,
                      uint8_t             *out);
size_t phase_voltage_report__pack_to_buffer
                     (const PhaseVoltageReport   *message,
                      ProtobufCBuffer     *buffer);
PhaseVoltageReport *
       phase_voltage_report__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   phase_voltage_report__free_unpacked
                     (PhaseVoltageReport *message,
                      ProtobufCAllocator *allocator);
/* OverloadReport methods */
void   overload_report__init
                     (OverloadReport         *message);
size_t overload_report__get_packed_size
                     (const OverloadReport   *message);
size_t overload_report__pack
                     (const OverloadReport   *message,
                      uint8_t             *out);
size_t overload_report__pack_to_buffer
                     (const OverloadReport   *message,
                      ProtobufCBuffer     *buffer);
OverloadReport *
       overload_report__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   overload_report__free_unpacked
                     (OverloadReport *message,
                      ProtobufCAllocator *allocator);
/* VoltageReportValues methods */
void   voltage_report_values__init
                     (VoltageReportValues         *message);
size_t voltage_report_values__get_packed_size
                     (const VoltageReportValues   *message);
size_t voltage_report_values__pack
                     (const VoltageReportValues   *message,
                      uint8_t             *out);
size_t voltage_report_values__pack_to_buffer
                     (const VoltageReportValues   *message,
                      ProtobufCBuffer     *buffer);
VoltageReportValues *
       voltage_report_values__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   voltage_report_values__free_unpacked
                     (VoltageReportValues *message,
                      ProtobufCAllocator *allocator);
/* VoltageReport methods */
void   voltage_report__init
                     (VoltageReport         *message);
size_t voltage_report__get_packed_size
                     (const VoltageReport   *message);
size_t voltage_report__pack
                     (const VoltageReport   *message,
                      uint8_t             *out);
size_t voltage_report__pack_to_buffer
                     (const VoltageReport   *message,
                      ProtobufCBuffer     *buffer);
VoltageReport *
       voltage_report__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   voltage_report__free_unpacked
                     (VoltageReport *message,
                      ProtobufCAllocator *allocator);
/* --- per-message closures --- */

typedef void (*VoltBaseRegisters_Closure)
                 (const VoltBaseRegisters *message,
                  void *closure_data);
typedef void (*DemandIntervalReport_Closure)
                 (const DemandIntervalReport *message,
                  void *closure_data);
typedef void (*MaxMinAvg_Closure)
                 (const MaxMinAvg *message,
                  void *closure_data);
typedef void (*PhaseVoltageReportValues_Closure)
                 (const PhaseVoltageReportValues *message,
                  void *closure_data);
typedef void (*PhaseVoltageReport_Closure)
                 (const PhaseVoltageReport *message,
                  void *closure_data);
typedef void (*OverloadReport_Closure)
                 (const OverloadReport *message,
                  void *closure_data);
typedef void (*VoltageReportValues_Closure)
                 (const VoltageReportValues *message,
                  void *closure_data);
typedef void (*VoltageReport_Closure)
                 (const VoltageReport *message,
                  void *closure_data);

/* --- services --- */


/* --- descriptors --- */

extern const ProtobufCMessageDescriptor volt_base_registers__descriptor;
extern const ProtobufCMessageDescriptor demand_interval_report__descriptor;
extern const ProtobufCMessageDescriptor max_min_avg__descriptor;
extern const ProtobufCMessageDescriptor phase_voltage_report_values__descriptor;
extern const ProtobufCMessageDescriptor phase_voltage_report__descriptor;
extern const ProtobufCMessageDescriptor overload_report__descriptor;
extern const ProtobufCMessageDescriptor voltage_report_values__descriptor;
extern const ProtobufCMessageDescriptor voltage_report__descriptor;

PROTOBUF_C__END_DECLS


#endif  /* PROTOBUF_C_hc_2fopendss_2fEnergyMeter_2eproto__INCLUDED */
