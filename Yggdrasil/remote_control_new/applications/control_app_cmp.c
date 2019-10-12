//
// Created by Pedro Akos on 2019-10-12.
//

#include "control_app_cmp.h"

#define BUFSIZE 50

static char* read_line(void)
{
    int bufsize = BUFSIZE;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufsize);
    int c;

    if (!buffer) {
        fprintf(stderr, "allocation error\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Read a character
        c = getchar();

        // If we hit EOF, replace it with a null character and return.
        if (c == EOF || c == '\n') {
            buffer[position] = '\0';
            return buffer;
        } else {
            buffer[position] = c;
        }
        position++;

        // If we have exceeded the buffer, reallocate.
        if (position >= bufsize) {
            bufsize += BUFSIZE;
            buffer = realloc(buffer, bufsize);
            if (!buffer) {
                fprintf(stderr, "allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

static void* control_app_init() {

    short protoId = 500;

    while(1) {

        char* line = read_line();
        if(line == NULL) {
            //empty command, sepoku time
            printf("NULL line read, getchar returned EOF (or error) shutting down\n");
            ygg_logflush();
            exit(1);
        }

        char tmp[20];
        bzero(tmp, 20);
        memcpy(tmp, line, strlen(line));
        char * l = tmp;

        ygg_log("CONTROL APP", "RECEIVED COMMAND", line);


        char* cmd = strsep(&l, " ");

        if(strcasecmp(cmd, "CHANGE_LINK") == 0){
            char* param = strsep(&l, " ");
            int neigh = atoi(param);

            YggRequest req;
            req.proto_dest = PROTO_TOPOLOGY_MANAGER;
            req.request = REQUEST;
            req.request_type = CHANGE_LINK;

            req.payload = malloc(sizeof(int));
            memcpy(req.payload, &neigh, sizeof(int));
            req.length = sizeof(int);

            deliverRequest(&req);

            free(req.payload);


        }else if(strcasecmp(cmd, "CHANGE_VAL") == 0){
            char* param = strsep(&l, " ");
            double val = (double) atoi(param);

            YggEvent ev;
            ev.notification_id = AGG_CHANGE_VAL;
            ev.proto_origin = protoId;
            ev.payload = malloc(sizeof(double));
            memcpy(ev.payload, &val, sizeof(double));

            deliverEvent(&ev);

            free(ev.payload);


        } else if(strcasecmp(cmd, "KILL") == 0){
            ygg_logflush();
            exit(0);
        }
        else {
            //ignore?
        }

        free(line);
    }
}

void start_control_app_cmp() {
    pthread_t thread;
    pthread_create(&thread, NULL, (void* (*)(void*)) control_app_init, NULL);
    pthread_detach(thread);
}