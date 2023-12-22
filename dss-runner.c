#include "include/dss_capi.h"
#include "include/rmqpush.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {

    char cmd[1000];
    char path[200];
    char *file_names[4] = {"main.dss", "Main.dss", "master.dss", "Master.dss"};
    char *file_path = NULL;

    if (argc > 2) {
        printf("Too many args!");
        exit(-1);
    }

    if (argc == 1) {
        sprintf(path, "%s", "high");
    } else {
        sprintf(path, "%s", argv[1]);
    }

    for (int i = 0; i < 4; i++) {
        file_path = malloc(sizeof(char) * 2000);
        sprintf(file_path, "%s/%s", path, file_names[i]);

        if (access(file_path, F_OK) == 0) {
            printf("File %s found\n", file_path);
            break;
        }
    }

    init_tracing();
    connect_to_stream("localhost", 5552, "hc", "password", "opendss", 100);
    if (file_path == NULL) {
        printf("Running with 'high/Master.dss' as nothing legit was provided\n");
        Text_Set_Command("compile high/Master.dss");
    } else {
        sprintf(cmd, "compile %s", file_path);
        Text_Set_Command(cmd);
    }
    disconnect_from_stream();

    printf("All done\n");
    return 0;
}
