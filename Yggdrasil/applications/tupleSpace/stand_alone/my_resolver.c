//
// Created by Pedro Akos on 2019-08-30.
//


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "tuple.h"

int
main(int argc, char *argv[])
{
    struct tuple *s, *t, *u;

    char description[100];


    struct timeval T0, T1;
    int rndsock;
    struct context ctx;

    if (get_server_portnumber(&ctx)) {
        if (argc < 3) {
            /* help message */
            fprintf(stderr,
                    "Usage: %s <server> <portnumber>\n", argv[0]);
            exit(1);
        }
        strcpy(ctx.peername, argv[1]);
        ctx.portnumber = atoi(argv[2]);
    }



    s = make_tuple("s????", "unresolved");


    t = make_tuple("s???s#", "resolved", description, 0);

    gettimeofday(&T0, NULL);

    while (1) {

        u = get_tuple(s, &ctx);
        if(u == NULL) {
            perror("get_tuple failed");
            exit(1);
        }

        t->elements[1] = u->elements[1];
        t->elements[2] = u->elements[2];
        t->elements[3] = u->elements[3];

        bzero(t->elements[4].data.s.ptr, 100);
        sprintf(t->elements[4].data.s.ptr, "no more trouble there");
        t->elements[4].data.s.len = strlen(t->elements[4].data.s.ptr);

        put_tuple(t, &ctx);


        gettimeofday(&T1, NULL);
        printf("%f\n", TIMEVAL_DIFF(T0, T1));
    }

    close(rndsock);
    destroy_tuple(s);
    destroy_tuple(t);
    destroy_tuple(u);

    return 0;
}