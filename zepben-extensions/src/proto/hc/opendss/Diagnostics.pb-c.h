/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: hc/opendss/Diagnostics.proto */

#ifndef PROTOBUF_C_hc_2fopendss_2fDiagnostics_2eproto__INCLUDED
#define PROTOBUF_C_hc_2fopendss_2fDiagnostics_2eproto__INCLUDED

#include <protobuf-c/protobuf-c.h>

PROTOBUF_C__BEGIN_DECLS

#if PROTOBUF_C_VERSION_NUMBER < 1003000
# error This file was generated by a newer version of protobuf-c which is incompatible with your libprotobuf-c headers. Please update your headers.
#elif 1005001 < PROTOBUF_C_MIN_COMPILER_VERSION
# error This file was generated by an older version of protobuf-c which is incompatible with your libprotobuf-c headers. Please regenerate this file with a newer version of protobuf-c.
#endif


typedef struct SummaryReport SummaryReport;
typedef struct EventLogEntry EventLogEntry;
typedef struct EventLog EventLog;
typedef struct TapsReport TapsReport;
typedef struct LoopReport LoopReport;
typedef struct IsolatedArea IsolatedArea;
typedef struct IsolatedElement IsolatedElement;
typedef struct IsolatedBusesReport IsolatedBusesReport;
typedef struct LossesEntry LossesEntry;
typedef struct LossesTotals LossesTotals;
typedef struct NodeMismatch NodeMismatch;
typedef struct KVBaseMismatch KVBaseMismatch;


/* --- enums --- */


/* --- messages --- */

/*
 **
 * Message for streaming the summary report
 */
struct  SummaryReport
{
  ProtobufCMessage base;
  char *casename;
  protobuf_c_boolean solved;
  char *mode;
  int32_t number;
  double loadmult;
  int32_t numdevices;
  int32_t numbuses;
  int32_t numnodes;
  int32_t iterations;
  char *controlmode;
  int32_t controliterations;
  int32_t mostiterationsdone;
  int32_t year;
  int32_t hour;
  double maxpuvoltage;
  double minpuvoltage;
  double totalmw;
  double totalmvar;
  double mwlosses;
  double pctlosses;
  double mvarlosses;
  double frequency;
};
#define SUMMARY_REPORT__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&summary_report__descriptor) \
    , (char *)protobuf_c_empty_string, 0, (char *)protobuf_c_empty_string, 0, 0, 0, 0, 0, 0, (char *)protobuf_c_empty_string, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }


/*
 **
 * Message for a single Event Log Entry
 */
struct  EventLogEntry
{
  ProtobufCMessage base;
  int32_t hour;
  double sec;
  int32_t controliter;
  int32_t iteration;
  char *element;
  char *action;
  char *event;
};
#define EVENT_LOG_ENTRY__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&event_log_entry__descriptor) \
    , 0, 0, 0, 0, (char *)protobuf_c_empty_string, (char *)protobuf_c_empty_string, (char *)protobuf_c_empty_string }


/*
 **
 * Message for streaming the opendss eventlog 
 */
struct  EventLog
{
  ProtobufCMessage base;
  size_t n_logentry;
  EventLogEntry **logentry;
};
#define EVENT_LOG__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&event_log__descriptor) \
    , 0,NULL }


/*
 **
 * Message for streaming the registry taps report
 */
struct  TapsReport
{
  ProtobufCMessage base;
  char *name;
  double tap;
  double min;
  double max;
  double step;
  int32_t position;
};
#define TAPS_REPORT__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&taps_report__descriptor) \
    , (char *)protobuf_c_empty_string, 0, 0, 0, 0, 0 }


/*
 **
 * Message for streaming the loops report
 */
struct  LoopReport
{
  ProtobufCMessage base;
  char *meter;
  char *linea;
  char *lineb;
  protobuf_c_boolean parallel;
  protobuf_c_boolean looped;
};
#define LOOP_REPORT__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&loop_report__descriptor) \
    , (char *)protobuf_c_empty_string, (char *)protobuf_c_empty_string, (char *)protobuf_c_empty_string, 0, 0 }


/*
 **
 * Message for one specific isolated area report (a level/id, line, and the related loads)
 */
struct  IsolatedArea
{
  ProtobufCMessage base;
  int32_t level;
  char *element;
  size_t n_loads;
  char **loads;
};
#define ISOLATED_AREA__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&isolated_area__descriptor) \
    , 0, (char *)protobuf_c_empty_string, 0,NULL }


/*
 **
 * Message for one specific isolated element report (a name of an element, and the related buses)
 */
struct  IsolatedElement
{
  ProtobufCMessage base;
  char *name;
  size_t n_buses;
  char **buses;
};
#define ISOLATED_ELEMENT__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&isolated_element__descriptor) \
    , (char *)protobuf_c_empty_string, 0,NULL }


/*
 **
 * Messages for streaming the isolated elements report (disconnected buses, areas and elements)
 */
struct  IsolatedBusesReport
{
  ProtobufCMessage base;
  size_t n_disconnectedbuses;
  char **disconnectedbuses;
  size_t n_isolatedsubareas;
  IsolatedArea **isolatedsubareas;
  size_t n_isolatedelements;
  IsolatedElement **isolatedelements;
};
#define ISOLATED_BUSES_REPORT__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&isolated_buses_report__descriptor) \
    , 0,NULL, 0,NULL, 0,NULL }


/*
 **
 * Message for streaming one specific Losses entry
 */
struct  LossesEntry
{
  ProtobufCMessage base;
  char *element;
  double kwlosses;
  double pctpower;
  double kvarlosses;
};
#define LOSSES_ENTRY__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&losses_entry__descriptor) \
    , (char *)protobuf_c_empty_string, 0, 0, 0 }


/*
 **
 * Messages for streaming the losses report
 */
struct  LossesTotals
{
  ProtobufCMessage base;
  double linelosses;
  double transformerlosses;
  double totallosses;
  double totalloadpower;
  double totalpctlosses;
};
#define LOSSES_TOTALS__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&losses_totals__descriptor) \
    , 0, 0, 0, 0, 0 }


/*
 **
 * Message for streaming the Node Mismatch report
 */
struct  NodeMismatch
{
  ProtobufCMessage base;
  char *bus;
  int32_t node;
  double currentsum;
  double pcterror;
  double maxcurrent;
};
#define NODE_MISMATCH__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&node_mismatch__descriptor) \
    , (char *)protobuf_c_empty_string, 0, 0, 0, 0 }


/*
 **
 * Message for streaming the kv Base Mismatch report
 */
struct  KVBaseMismatch
{
  ProtobufCMessage base;
  char *load;
  double kv;
  char *bus;
  double kvbase;
};
#define KVBASE_MISMATCH__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&kvbase_mismatch__descriptor) \
    , (char *)protobuf_c_empty_string, 0, (char *)protobuf_c_empty_string, 0 }


/* SummaryReport methods */
void   summary_report__init
                     (SummaryReport         *message);
size_t summary_report__get_packed_size
                     (const SummaryReport   *message);
size_t summary_report__pack
                     (const SummaryReport   *message,
                      uint8_t             *out);
size_t summary_report__pack_to_buffer
                     (const SummaryReport   *message,
                      ProtobufCBuffer     *buffer);
SummaryReport *
       summary_report__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   summary_report__free_unpacked
                     (SummaryReport *message,
                      ProtobufCAllocator *allocator);
/* EventLogEntry methods */
void   event_log_entry__init
                     (EventLogEntry         *message);
size_t event_log_entry__get_packed_size
                     (const EventLogEntry   *message);
size_t event_log_entry__pack
                     (const EventLogEntry   *message,
                      uint8_t             *out);
size_t event_log_entry__pack_to_buffer
                     (const EventLogEntry   *message,
                      ProtobufCBuffer     *buffer);
EventLogEntry *
       event_log_entry__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   event_log_entry__free_unpacked
                     (EventLogEntry *message,
                      ProtobufCAllocator *allocator);
/* EventLog methods */
void   event_log__init
                     (EventLog         *message);
size_t event_log__get_packed_size
                     (const EventLog   *message);
size_t event_log__pack
                     (const EventLog   *message,
                      uint8_t             *out);
size_t event_log__pack_to_buffer
                     (const EventLog   *message,
                      ProtobufCBuffer     *buffer);
EventLog *
       event_log__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   event_log__free_unpacked
                     (EventLog *message,
                      ProtobufCAllocator *allocator);
/* TapsReport methods */
void   taps_report__init
                     (TapsReport         *message);
size_t taps_report__get_packed_size
                     (const TapsReport   *message);
size_t taps_report__pack
                     (const TapsReport   *message,
                      uint8_t             *out);
size_t taps_report__pack_to_buffer
                     (const TapsReport   *message,
                      ProtobufCBuffer     *buffer);
TapsReport *
       taps_report__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   taps_report__free_unpacked
                     (TapsReport *message,
                      ProtobufCAllocator *allocator);
/* LoopReport methods */
void   loop_report__init
                     (LoopReport         *message);
size_t loop_report__get_packed_size
                     (const LoopReport   *message);
size_t loop_report__pack
                     (const LoopReport   *message,
                      uint8_t             *out);
size_t loop_report__pack_to_buffer
                     (const LoopReport   *message,
                      ProtobufCBuffer     *buffer);
LoopReport *
       loop_report__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   loop_report__free_unpacked
                     (LoopReport *message,
                      ProtobufCAllocator *allocator);
/* IsolatedArea methods */
void   isolated_area__init
                     (IsolatedArea         *message);
size_t isolated_area__get_packed_size
                     (const IsolatedArea   *message);
size_t isolated_area__pack
                     (const IsolatedArea   *message,
                      uint8_t             *out);
size_t isolated_area__pack_to_buffer
                     (const IsolatedArea   *message,
                      ProtobufCBuffer     *buffer);
IsolatedArea *
       isolated_area__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   isolated_area__free_unpacked
                     (IsolatedArea *message,
                      ProtobufCAllocator *allocator);
/* IsolatedElement methods */
void   isolated_element__init
                     (IsolatedElement         *message);
size_t isolated_element__get_packed_size
                     (const IsolatedElement   *message);
size_t isolated_element__pack
                     (const IsolatedElement   *message,
                      uint8_t             *out);
size_t isolated_element__pack_to_buffer
                     (const IsolatedElement   *message,
                      ProtobufCBuffer     *buffer);
IsolatedElement *
       isolated_element__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   isolated_element__free_unpacked
                     (IsolatedElement *message,
                      ProtobufCAllocator *allocator);
/* IsolatedBusesReport methods */
void   isolated_buses_report__init
                     (IsolatedBusesReport         *message);
size_t isolated_buses_report__get_packed_size
                     (const IsolatedBusesReport   *message);
size_t isolated_buses_report__pack
                     (const IsolatedBusesReport   *message,
                      uint8_t             *out);
size_t isolated_buses_report__pack_to_buffer
                     (const IsolatedBusesReport   *message,
                      ProtobufCBuffer     *buffer);
IsolatedBusesReport *
       isolated_buses_report__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   isolated_buses_report__free_unpacked
                     (IsolatedBusesReport *message,
                      ProtobufCAllocator *allocator);
/* LossesEntry methods */
void   losses_entry__init
                     (LossesEntry         *message);
size_t losses_entry__get_packed_size
                     (const LossesEntry   *message);
size_t losses_entry__pack
                     (const LossesEntry   *message,
                      uint8_t             *out);
size_t losses_entry__pack_to_buffer
                     (const LossesEntry   *message,
                      ProtobufCBuffer     *buffer);
LossesEntry *
       losses_entry__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   losses_entry__free_unpacked
                     (LossesEntry *message,
                      ProtobufCAllocator *allocator);
/* LossesTotals methods */
void   losses_totals__init
                     (LossesTotals         *message);
size_t losses_totals__get_packed_size
                     (const LossesTotals   *message);
size_t losses_totals__pack
                     (const LossesTotals   *message,
                      uint8_t             *out);
size_t losses_totals__pack_to_buffer
                     (const LossesTotals   *message,
                      ProtobufCBuffer     *buffer);
LossesTotals *
       losses_totals__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   losses_totals__free_unpacked
                     (LossesTotals *message,
                      ProtobufCAllocator *allocator);
/* NodeMismatch methods */
void   node_mismatch__init
                     (NodeMismatch         *message);
size_t node_mismatch__get_packed_size
                     (const NodeMismatch   *message);
size_t node_mismatch__pack
                     (const NodeMismatch   *message,
                      uint8_t             *out);
size_t node_mismatch__pack_to_buffer
                     (const NodeMismatch   *message,
                      ProtobufCBuffer     *buffer);
NodeMismatch *
       node_mismatch__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   node_mismatch__free_unpacked
                     (NodeMismatch *message,
                      ProtobufCAllocator *allocator);
/* KVBaseMismatch methods */
void   kvbase_mismatch__init
                     (KVBaseMismatch         *message);
size_t kvbase_mismatch__get_packed_size
                     (const KVBaseMismatch   *message);
size_t kvbase_mismatch__pack
                     (const KVBaseMismatch   *message,
                      uint8_t             *out);
size_t kvbase_mismatch__pack_to_buffer
                     (const KVBaseMismatch   *message,
                      ProtobufCBuffer     *buffer);
KVBaseMismatch *
       kvbase_mismatch__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   kvbase_mismatch__free_unpacked
                     (KVBaseMismatch *message,
                      ProtobufCAllocator *allocator);
/* --- per-message closures --- */

typedef void (*SummaryReport_Closure)
                 (const SummaryReport *message,
                  void *closure_data);
typedef void (*EventLogEntry_Closure)
                 (const EventLogEntry *message,
                  void *closure_data);
typedef void (*EventLog_Closure)
                 (const EventLog *message,
                  void *closure_data);
typedef void (*TapsReport_Closure)
                 (const TapsReport *message,
                  void *closure_data);
typedef void (*LoopReport_Closure)
                 (const LoopReport *message,
                  void *closure_data);
typedef void (*IsolatedArea_Closure)
                 (const IsolatedArea *message,
                  void *closure_data);
typedef void (*IsolatedElement_Closure)
                 (const IsolatedElement *message,
                  void *closure_data);
typedef void (*IsolatedBusesReport_Closure)
                 (const IsolatedBusesReport *message,
                  void *closure_data);
typedef void (*LossesEntry_Closure)
                 (const LossesEntry *message,
                  void *closure_data);
typedef void (*LossesTotals_Closure)
                 (const LossesTotals *message,
                  void *closure_data);
typedef void (*NodeMismatch_Closure)
                 (const NodeMismatch *message,
                  void *closure_data);
typedef void (*KVBaseMismatch_Closure)
                 (const KVBaseMismatch *message,
                  void *closure_data);

/* --- services --- */


/* --- descriptors --- */

extern const ProtobufCMessageDescriptor summary_report__descriptor;
extern const ProtobufCMessageDescriptor event_log_entry__descriptor;
extern const ProtobufCMessageDescriptor event_log__descriptor;
extern const ProtobufCMessageDescriptor taps_report__descriptor;
extern const ProtobufCMessageDescriptor loop_report__descriptor;
extern const ProtobufCMessageDescriptor isolated_area__descriptor;
extern const ProtobufCMessageDescriptor isolated_element__descriptor;
extern const ProtobufCMessageDescriptor isolated_buses_report__descriptor;
extern const ProtobufCMessageDescriptor losses_entry__descriptor;
extern const ProtobufCMessageDescriptor losses_totals__descriptor;
extern const ProtobufCMessageDescriptor node_mismatch__descriptor;
extern const ProtobufCMessageDescriptor kvbase_mismatch__descriptor;

PROTOBUF_C__END_DECLS


#endif  /* PROTOBUF_C_hc_2fopendss_2fDiagnostics_2eproto__INCLUDED */
