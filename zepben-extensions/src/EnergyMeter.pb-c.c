/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: EnergyMeter.proto */

/* Do not generate deprecated warnings for self */
#ifndef PROTOBUF_C__NO_DEPRECATED
#define PROTOBUF_C__NO_DEPRECATED
#endif

#include "EnergyMeter.pb-c.h"
void   result__init
                     (Result         *message)
{
  static const Result init_value = RESULT__INIT;
  *message = init_value;
}
size_t result__get_packed_size
                     (const Result *message)
{
  assert(message->base.descriptor == &result__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t result__pack
                     (const Result *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &result__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t result__pack_to_buffer
                     (const Result *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &result__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
Result *
       result__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (Result *)
     protobuf_c_message_unpack (&result__descriptor,
                                allocator, len, data);
}
void   result__free_unpacked
                     (Result *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &result__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   energy_meter_result__init
                     (EnergyMeterResult         *message)
{
  static const EnergyMeterResult init_value = ENERGY_METER_RESULT__INIT;
  *message = init_value;
}
size_t energy_meter_result__get_packed_size
                     (const EnergyMeterResult *message)
{
  assert(message->base.descriptor == &energy_meter_result__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t energy_meter_result__pack
                     (const EnergyMeterResult *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &energy_meter_result__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t energy_meter_result__pack_to_buffer
                     (const EnergyMeterResult *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &energy_meter_result__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
EnergyMeterResult *
       energy_meter_result__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (EnergyMeterResult *)
     protobuf_c_message_unpack (&energy_meter_result__descriptor,
                                allocator, len, data);
}
void   energy_meter_result__free_unpacked
                     (EnergyMeterResult *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &energy_meter_result__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
static const ProtobufCFieldDescriptor result__field_descriptors[2] =
{
  {
    "name",
    1,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_STRING,
    0,   /* quantifier_offset */
    offsetof(Result, name),
    NULL,
    &protobuf_c_empty_string,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "value",
    2,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_STRING,
    0,   /* quantifier_offset */
    offsetof(Result, value),
    NULL,
    &protobuf_c_empty_string,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned result__field_indices_by_name[] = {
  0,   /* field[0] = name */
  1,   /* field[1] = value */
};
static const ProtobufCIntRange result__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 2 }
};
const ProtobufCMessageDescriptor result__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "Result",
  "Result",
  "Result",
  "",
  sizeof(Result),
  2,
  result__field_descriptors,
  result__field_indices_by_name,
  1,  result__number_ranges,
  (ProtobufCMessageInit) result__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor energy_meter_result__field_descriptors[4] =
{
  {
    "type",
    1,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_ENUM,
    0,   /* quantifier_offset */
    offsetof(EnergyMeterResult, type),
    &result_type__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "mrid",
    2,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_STRING,
    0,   /* quantifier_offset */
    offsetof(EnergyMeterResult, mrid),
    NULL,
    &protobuf_c_empty_string,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "year",
    3,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_INT32,
    0,   /* quantifier_offset */
    offsetof(EnergyMeterResult, year),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "results",
    4,
    PROTOBUF_C_LABEL_REPEATED,
    PROTOBUF_C_TYPE_MESSAGE,
    offsetof(EnergyMeterResult, n_results),
    offsetof(EnergyMeterResult, results),
    &result__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned energy_meter_result__field_indices_by_name[] = {
  1,   /* field[1] = mrid */
  3,   /* field[3] = results */
  0,   /* field[0] = type */
  2,   /* field[2] = year */
};
static const ProtobufCIntRange energy_meter_result__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 4 }
};
const ProtobufCMessageDescriptor energy_meter_result__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "EnergyMeterResult",
  "EnergyMeterResult",
  "EnergyMeterResult",
  "",
  sizeof(EnergyMeterResult),
  4,
  energy_meter_result__field_descriptors,
  energy_meter_result__field_indices_by_name,
  1,  energy_meter_result__number_ranges,
  (ProtobufCMessageInit) energy_meter_result__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCEnumValue result_type__enum_values_by_number[4] =
{
  { "DI", "RESULT_TYPE__DI", 0 },
  { "PHV_DI", "RESULT_TYPE__PHV_DI", 1 },
  { "DI_OVERLOAD", "RESULT_TYPE__DI_OVERLOAD", 2 },
  { "DI_VOLT_EXCEPTIONS", "RESULT_TYPE__DI_VOLT_EXCEPTIONS", 3 },
};
static const ProtobufCIntRange result_type__value_ranges[] = {
{0, 0},{0, 4}
};
static const ProtobufCEnumValueIndex result_type__enum_values_by_name[4] =
{
  { "DI", 0 },
  { "DI_OVERLOAD", 2 },
  { "DI_VOLT_EXCEPTIONS", 3 },
  { "PHV_DI", 1 },
};
const ProtobufCEnumDescriptor result_type__descriptor =
{
  PROTOBUF_C__ENUM_DESCRIPTOR_MAGIC,
  "ResultType",
  "ResultType",
  "ResultType",
  "",
  4,
  result_type__enum_values_by_number,
  4,
  result_type__enum_values_by_name,
  1,
  result_type__value_ranges,
  NULL,NULL,NULL,NULL   /* reserved[1234] */
};
