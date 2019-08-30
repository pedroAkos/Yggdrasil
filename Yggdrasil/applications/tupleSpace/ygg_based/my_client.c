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

#include "protocols/wireless/communication/routing/batman.h"
#include "protocols/wireless/communication/point-to-point/reliable.h"

void * resolver_app(void* args) {

    struct tuple *s, *t, *u;

    struct timeval T0, T1;

    char description[100];

    struct context ctx = *((struct context*) args);

    s = make_tuple("s????", "unresolved");


    t = make_tuple("s???s#", "resolved", description, 0);

    gettimeofday(&T0, NULL);

    while (1) {

        u = get_tuple(s, &ctx);
        if(u == NULL) {
            perror("get_tuple failed");
            exit(1);
        }

        printf("got tuple trouble\n");

        t->elements[1] = u->elements[1];
        t->elements[2] = u->elements[2];
        t->elements[3] = u->elements[3];

        bzero(t->elements[4].data.s.ptr, 100);
        sprintf(t->elements[4].data.s.ptr, "no more trouble there");
        t->elements[4].data.s.len = strlen(t->elements[4].data.s.ptr);

        put_tuple(t, &ctx);
        printf("put tuple no more trouble\n");


        gettimeofday(&T1, NULL);
        printf("%f\n", TIMEVAL_DIFF(T0, T1));
    }
}

int
main(int argc, char *argv[])
{
    struct tuple *s, *t, *u;

    struct timeval T0, T1;
    struct context ctx, ctx_re;

    unsigned short server_id = 500;

    NetworkConfig* ntconf = defineWirelessNetworkConfig("AdHoc", 0, 5, 0, "ledge", YGG_filter);

    ygg_runtime_init(ntconf);


    batman_args* bargs = batman_args_init(false, false, 2, 0, 5, DEFAULT_BATMAN_WINDOW_SIZE, 3);
    registerProtocol(PROTO_ROUTING_BATMAN, batman_init, bargs);
    batman_args_destroy(bargs);

    reliable_p2p_args* rargs = reliableP2PArgs_init(10, 2, 5);
    registerProtocol(PROTO_P2P_RELIABLE_DELIVERY, reliable_point2point_init, rargs);
    reliableP2PArgs_destroy(rargs);

    short myId = 400;

    app_def* myapp = create_application_definition(myId, "MyApp");
    ctx.inBox = registerApp(myapp);
    ctx.ygg_id = myId;
    ctx.server_ygg_id = server_id;

    app_def* myresolver = create_application_definition(myId+1, "MyResover");
    ctx_re.inBox = registerApp(myresolver);
    ctx_re.ygg_id = myId+1;
    ctx_re.server_ygg_id = server_id;

    ygg_runtime_start();

    pthread_t resolver;
    pthread_create(&resolver, NULL, resolver_app, &ctx_re);


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

        printf("put tuple trouble\n");

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

}