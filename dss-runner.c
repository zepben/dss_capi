#include "include/rmqpush.h"
#include "include/dss_capi.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[] ) {

    char cmd[80];

    if ( argc > 2) {
        printf("Too many args!");
        exit(-1);
    }

    connect_rabbitmq("localhost", 5672, "hc", "password", "opendss", "amq.direct");
    if ( argc == 1) {
        Text_Set_Command("compile high/Master.dss");
        disconnect_rabbitmq();
    } else {
        sprintf(cmd, "compile %s/Master.dss", argv[1]);
        Text_Set_Command(cmd);
        disconnect_rabbitmq();
    }
    
    return 0;
}
