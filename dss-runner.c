#include "include/rmqpush.h"
#include "include/dss_capi.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[] ) {

    char cwd[1000];
    char cmd[1000];
    char path[200];
    char *file_names[4] = { "main.dss", "Main.dss", "master.dss", "Master.dss" };
    char *file_path;

    if ( argc > 2) {
        printf("Too many args!");
        exit(-1);
    }

    if ( argc == 1) {
        sprintf(path, "%s", "high");
    } else {
        sprintf(path, "%s", argv[1]);
    }

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
       perror("getcwd() error");
       exit(1);
    }

    // Check if the path exists
    file_path = malloc(sizeof(char) * 2000);
    sprintf(file_path, "%s/%s", cwd, path);
    if ( access(file_path, F_OK) != 0 ) {
        printf("The provided path '%s' doesn't exist!\n", file_path);
        exit(1);
    }

    // Look out for the master dss file
    for (int i = 0; i < 4; i++) {
        file_path = malloc(sizeof(char) * 2000);
        sprintf(file_path, "%s/%s/%s", cwd, path, file_names[i]);

        if (access(file_path, F_OK) == 0) {
          printf("File %s found\n", file_path);
          break;
        } else {
            file_path = NULL;
        }
    }

    if (file_path == NULL) {
        printf("Can't find a file to work with!\n");
        exit(1);
    }

    connect_rabbitmq("localhost", 5672, "hc", "password", "opendss", "amq.direct", 100);
    if ( argc == 1) {
        Text_Set_Command("compile high/Master.dss");
        disconnect_rabbitmq();
    } else {
        sprintf(cmd, "compile %s/Master.dss", argv[1]);
        Text_Set_Command(cmd);
        disconnect_rabbitmq();
    }

    printf("All done\n");
    return 0;
}
