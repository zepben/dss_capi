// Copyright 2007 - 2021, Alan Antonuk and the rabbitmq-c contributors.
// SPDX-License-Identifier: mit

#ifndef amqp_utils_h
#define amqp_utils_h

void die_smart(int error, const char *fmt, ...);
void die(const char *fmt, ...);

extern bool has_error(int x, char const *context);
extern bool has_amqp_error(amqp_rpc_reply_t x, char const *context);

extern void amqp_dump(void const *buffer, size_t len);

extern uint64_t now_microseconds(void);
extern void microsleep(int usec);

#endif
