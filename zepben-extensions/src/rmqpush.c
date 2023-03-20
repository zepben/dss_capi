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

#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/tcp_socket.h>

#include "include/EnergyMeter.pb-c.h"
#include "include/utils.h"

static amqp_socket_t *socket;
static amqp_connection_state_t conn;
static amqp_basic_properties_t props;
static char const *exchange;
static char const *routingkey;
static int status = AMQP_STATUS_UNEXPECTED_STATE; // initial state

EnergyMeterResult dss_result;

char *get_env(char *envname, char *def) {
  char *p_tmp;
  if ((p_tmp = getenv(envname)) != NULL) {
    return p_tmp;
  } else {
    return def;
  }
}

void close_queue() {

  // Only close everything once
  if (conn && status == AMQP_STATUS_OK) {
    die_on_amqp_error(amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS),
                      "Closing channel");
    die_on_amqp_error(amqp_connection_close(conn, AMQP_REPLY_SUCCESS),
                      "Closing connection");
    die_on_error(amqp_destroy_connection(conn), "Ending connection");
    status = AMQP_STATUS_CONNECTION_CLOSED;
  }
}

void init_connection() {
  if (status != AMQP_STATUS_OK) {

    printf("Init connection\n");
    const char *hostname = get_env("RMQ_HOST", "me");
    const int port = atoi(get_env("RMQ_PORT", "5672"));
    const char *username = get_env("RMQ_USERNAME", "hc1");
    const char *password = get_env("RMQ_PASSWORD", "password");

    routingkey = get_env("RMQ_ROUTING_KEY", "test");
    exchange = get_env("RMQ_EXCHANGE", "amq.direct");
    // TODO: maybe
    // ("HC_SCENARIO", "")
    // ("HC_FEEDER_MRID", "")
    // ("HC_YEAR", "")

    printf("Connecting to '%s:%d' with '%s/%s' ", hostname, port, username,
           password);

    if (conn == NULL) {
      conn = amqp_new_connection();
      if (conn == NULL) {
        die_smart(11, "creating connection");
      } else {
        printf(" - Done\n");
      }
    } else {
      printf("Using existing connection\n");
    }

    if (socket == NULL) {
      socket = amqp_tcp_socket_new(conn);
      if (socket == NULL) {
        die_smart(11, "creating TCP socket");
      }

      status = amqp_socket_open(socket, hostname, port);
      if (status) {
        die_smart(11,
                  "opening TCP socket, check that rabbitmq is running on the "
                  "provided host");
      }
    }

    // Login
    die_on_amqp_error(amqp_login(conn, "/", 0, 131072, 0,
                                 AMQP_SASL_METHOD_PLAIN, username, password),
                      "Logging in");
    // Open channel
    amqp_channel_open(conn, 1);
    die_on_amqp_error(amqp_get_rpc_reply(conn), "Opening channel");
    if (props.delivery_mode != AMQP_DELIVERY_PERSISTENT) {
      props._flags =
          AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
      props.content_type = amqp_cstring_bytes("binary/proto");
      props.delivery_mode =
          AMQP_DELIVERY_PERSISTENT; /* persistent delivery mode */
    }
  }
}

amqp_bytes_t message_to_bytes(EnergyMeterResult *message) {
  void *buf;
  int len = energy_meter_result__get_packed_size(message);
  buf = malloc(len);
  energy_meter_result__pack(&dss_result, buf);

  amqp_bytes_t data;
  data.len = len;
  data.bytes = buf;

  return data;
}

void show_message(EnergyMeterResult *result) {
  printf("Constructed message is:\ndata {\n\ttype -> %d,\n\tyear -> %d,\n\t"
         "mrid -> %s",
         result->type, result->year, result->mrid);
  for (int i = 0; i < (int)result->n_results; i++) {

    printf(",\n\t%s -> %s", result->results[i]->name,
           result->results[i]->value);
  }
  printf("\n}\n");
}

bool is_aux_message(char *field) {
  return sizeof(field) >= 3 && field[0] == 'A' && field[1] == 'u' &&
         field[2] == 'x';
}

void build_message(ResultType type, char *name, double year, char **data,
                   char **fieldnames, int len) {

  // Init object if first time
  if (dss_result.base.descriptor != &energy_meter_result__descriptor) {
    energy_meter_result__init(&dss_result);
  }

  dss_result.year = (int)year;
  dss_result.mrid = name;
  dss_result.type = type;

  int elem_size = (int)sizeof(Result);
  int size = len * elem_size;

  int newlen = 0;
  if (size > 0) {

    // We've init-ed dss_result, so results is nil
    // Hence we need to reallocate
    dss_result.results = malloc(size);
    for (int i = 0; i < len; i++) {
      if (is_aux_message(fieldnames[i]))
        continue;

      dss_result.results[newlen] = malloc(elem_size);
      result__init(dss_result.results[newlen]);
      dss_result.results[newlen]->name = fieldnames[i];
      // Done with data, free the allocation
      dss_result.results[newlen]->value = data[i];
      newlen++;
    }
  }

  // Update new optimised length
  dss_result.n_results = newlen;
  // show_message(&dss_result);
}

int send_di_data(ResultType type, char *name, double year, char **data,
                 char **fieldnames, int len) {

  init_connection();
  build_message(type, name, year, data, fieldnames, len);
  die_on_error(amqp_basic_publish(conn, 1, amqp_cstring_bytes(exchange),
                                  amqp_cstring_bytes(routingkey), 1, 0, &props,
                                  message_to_bytes(&dss_result)),
               "Publishing");

  // Release memory for the results pointer.
  // TODO: Should we release all the pointers in the list as well??
  free(dss_result.results);

  return 0;
}

void send_di_as_double(ResultType type, char *name, double year, double *data,
                       char **fieldnames, int len) {

  // Transform doubles into strings
  char *stringdata[len];

  for (int i = 0; i < len; i++) {
    char *d;
    d = malloc(30);
    sprintf(d, "%.20lf", data[i]);
    stringdata[i] = d;
  }

  send_di_data(type, name, year, stringdata, fieldnames, len);
}
