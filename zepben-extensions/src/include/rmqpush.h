// Zepben

#ifndef rmq_push_h
#define rmq_push_h

#include "EnergyMeter.pb-c.h"
int send_di_as_double(ResultType type, char *name, double year, double *data,
                      char **fieldnames, int len);
int send_di_data(ResultType type, char *name, double year, char **data,
                 char **fieldnames, int len);
void close_queue();

#endif
