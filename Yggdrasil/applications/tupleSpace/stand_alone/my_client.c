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

    char description[100];

    s = make_tuple("sidds#", "unresolved", 0,0,0, description, 0);


    t = make_tuple("sidd?", "resolved", 0, 0, 0);

    gettimeofday(&T0, NULL);

    while (1) {

        int rnd = random_int();
        int quadrant = (rnd  % 21) +1;
        double X = rnd / 2;
        double Y = rnd / 3;

        s->elements[1].data.i = quadrant;
        s->elements[2].data.d = X;
        s->elements[3].data.d = Y;
        bzero(s->elements[4].data.s.ptr, 100);
        sprintf(s->elements[4].data.s.ptr, "trouble in %d %f %f", quadrant, X, Y);
        s->elements[4].data.s.len = strlen(s->elements[4].data.s.ptr);

        put_tuple(s, &ctx);

        t->elements[1].data.i = quadrant;
        t->elements[2].data.d = X;
        t->elements[3].data.d = Y;

        u = get_tuple(t, &ctx);

        if (u == NULL) {
            perror("get_tuple failed");
            exit(1);
        }

        printf("%s\n", u->elements[4].data.s.ptr);


        gettimeofday(&T1, NULL);
        printf("%f\n", TIMEVAL_DIFF(T0, T1));

        sleep(1);
    }

    close(rndsock);
    destroy_tuple(s);
    destroy_tuple(t);
    destroy_tuple(u);

    return 0;
}