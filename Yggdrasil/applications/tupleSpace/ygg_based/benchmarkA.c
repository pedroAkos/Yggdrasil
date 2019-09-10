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

#include "utils/hashfunctions.h"

char self[10];

struct timeval BEGIN;

void log_bytes(const char* msg) {

    struct timeval now;

    char buff[50];

    FILE* tx= fopen("/sys/class/net/wlan0/statistics/tx_bytes","r");
    FILE* rx= fopen("/sys/class/net/wlan0/statistics/rx_bytes","r");
    gettimeofday(&now, NULL);
    bzero(buff, 50); fscanf(tx, "%s", buff); printf("%s %s TX-BYTES %s  %f\n", self, msg, buff, TIMEVAL_DIFF(BEGIN, now));
    bzero(buff, 50); fscanf(rx, "%s", buff); printf("%s %s RX-BYTES %s  %f\n", self, msg, buff, TIMEVAL_DIFF(BEGIN, now));
    fclose(tx); fclose(rx);

}

void* control_func(void) {

    while (1) {
        if (access("control.file", F_OK) != -1) {
            break;
        } else {
            // file doesn't exist
        }
        sleep(60);
    }

    remove("control.file");
    fflush(stdout);
    sleep(120);
    log_bytes("EXIT");
    exit(1);
}


void print_tuple_local(struct tuple* t, char* buff) {
    bzero(buff, 100);

    int i = 0;
    buff[i] = '('; i++;

    struct element e = t->elements[0];
    unsigned char* s = (unsigned char*)e.data.s.ptr;
    int n = e.data.s.len;

    for(int j = 0; j < n; j++) {
        buff[i] = s[j];
        i++;
    }

    buff[i] = ','; i++;
    i += sprintf(buff+i, "%d", t->elements[1].data.i);

    buff[i] = ','; i++;

    e = t->elements[4];
    s = (unsigned char*)e.data.s.ptr;
    n = e.data.s.len;

    for(int j = 0; j < n; j++) {
        buff[i] = s[j];
        i++;
    }

    buff[i] = ')';
}

#define LOGTUPLE(s, buff)    print_tuple_local(s, buff);


pthread_mutex_t mutex_put;
pthread_mutex_t mutex_get;

int counter_put = 0;
int counter_get = 0;

int stop = 0;

void inc_get() {
    pthread_mutex_lock(&mutex_get);
    counter_get ++;
    pthread_mutex_unlock(&mutex_get);
}

void inc_put() {
    pthread_mutex_lock(&mutex_put);
    counter_put ++;
    pthread_mutex_unlock(&mutex_put);
}

void* counter_fun(void) {

    struct timeval START, T0;
    gettimeofday(&START, NULL);
    while(!stop) {
        sleep(10);
        pthread_mutex_lock(&mutex_put);
        pthread_mutex_lock(&mutex_get);
        gettimeofday(&T0, NULL);
        printf("%s App-PUT  %d  %f\n", self, counter_put, TIMEVAL_DIFF(START, T0));
        printf("%s App-GET  %d  %f\n", self, counter_get, TIMEVAL_DIFF(START, T0));
        counter_put = 0;
        counter_get = 0;
        pthread_mutex_unlock(&mutex_get);
        pthread_mutex_unlock(&mutex_put);
    }

    printf("%s App-PUT  %d  %f\n", self, counter_put, TIMEVAL_DIFF(START, T0));
    printf("%s App-GET  %d  %f\n", self, counter_get, TIMEVAL_DIFF(START, T0));

    pthread_exit(NULL);

}

list *op_puts, *op_gets;

struct remaining_op {
    struct tuple* t;
    int i;
    int j;
    int r;
};

void store_op(list* op_list, struct tuple* t, int i, int j, int r) {
    struct remaining_op* op = malloc(sizeof(struct remaining_op));

    op->t = t;
    op->i = i;
    op->j = j;
    op->r = r;

    list_add_item_to_tail(op_list, op);
}

void * resolver_app(void* args) {

    struct tuple *s, *u;

    struct timeval START, T0, T1;

    struct context ctx = *((struct context*) args);

    char buff[100];


    gettimeofday(&START, NULL);

    while(op_gets->size > 0) {
        struct remaining_op* op = list_remove_head(op_gets);
        s = op->t;
        gettimeofday(&T0, NULL);
        u = get_nb_tuple(s, &ctx);
        if(u == NULL || u == (struct tuple*) -1) {
            gettimeofday(&T1, NULL);
            //LOGTUPLE(u, buff);
            printf("%s FAILED GET  %d:%d:%d  %f  %f\n", self, op->i, op->j, op->r, TIMEVAL_DIFF(T0, T1), TIMEVAL_DIFF(START, T1));
            store_op(op_gets, s, op->i, op->j, op->r+1);
        } else {
            gettimeofday(&T1, NULL);
            LOGTUPLE(u, buff);
            printf("%s GET %d:%d:%d   %f  %f  %s\n", self, op->i, op->j, op->r, TIMEVAL_DIFF(T0, T1), TIMEVAL_DIFF(START, T1), buff);
            destroy_tuple(u);
            destroy_tuple(s);
            inc_get();
        }
        free(op);

    }


    pthread_exit(NULL);

}

typedef void *(*threadfunction) (void *);


int
main(int argc, char *argv[])
{
    NetworkConfig* ntconf = defineWirelessNetworkConfig("AdHoc", 0, 5, 0, "ledge", YGG_filter);

    ygg_runtime_init(ntconf);

    op_gets = list_init();
    op_puts = list_init();

    bzero(self, 10);
    memcpy(self, getHostname(), 9);

    gettimeofday(&BEGIN, NULL);
    log_bytes("START");

    pthread_mutex_init(&mutex_put, NULL);
    pthread_mutex_init(&mutex_get, NULL);
    struct tuple *s;

    struct timeval START, T0, T1;
    struct context ctx, ctx_re;

    char * server_mac = "b8:27:eb:3a:96:0e";
//    bzero(ctx.peername, 100); bzero(ctx_re.peername, 100);
//    memcpy(ctx.peername)
    strcpy(ctx.peername,server_mac);
    strcpy(ctx_re.peername,server_mac);

    unsigned short server_id = 500;


    batman_args* bargs = batman_args_init(false, false, 2, 0, 16, DEFAULT_BATMAN_WINDOW_SIZE, 0);
    registerProtocol(PROTO_ROUTING_BATMAN, batman_init, bargs);
    batman_args_destroy(bargs);

    reliable_p2p_args* rargs = reliableP2PArgs_init(30, 0, 500000000, 10);
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

    char name[10]; bzero(name, 10);
    char buff[100]; bzero(buff, 100);

    for(int i = 1; i <= 100; i++) {
        for(int j = 2; j <= 21; j++) {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            sprintf(buff,"%ld%ld%s", now.tv_sec, now.tv_nsec, self);
            int hash = djb2(buff);
            s = make_tuple(hash,"sidds", self , 0,0.0,0.0, name);
            int rnd = random_int();
            double X = rnd / 2;
            double Y = rnd / 3;
            s->elements[1].data.i = i;
            s->elements[2].data.d = X;
            s->elements[3].data.d = Y;
            sprintf(name, "raspi-%02d", j);
            int s_len = strlen(name);
            s->string_space = malloc(s_len);
            memcpy(s->string_space, name, s_len);
            s->elements[4].data.s.ptr = s->string_space;
            s->elements[4].data.s.len = s_len;
            /**
             * Put operations
             */
            store_op(op_puts, s, i, j, 0);

            /**
             * Get operations
             * (hash is only verified on puts)
             */
            s = make_tuple(hash,"?i???", 0);
            s->elements[1].data.i = i;
            store_op(op_gets, s, i, j, 0);
        }
    }


    sleep(120); //grace period

    log_bytes("BEGIN");

    pthread_t resolver;
    pthread_create(&resolver, NULL, resolver_app, &ctx_re);

    pthread_t counter;
    pthread_create(&counter, NULL, (threadfunction) counter_fun, NULL);

    pthread_t control;
    pthread_create(&control, NULL, (threadfunction) control_func, NULL);


    gettimeofday(&START, NULL);


    while(op_puts->size > 0) {
        struct remaining_op* op = list_remove_head(op_puts);
        s = op->t;
        gettimeofday(&T0, NULL);
        if(put_tuple(s, &ctx)) {
            LOGTUPLE(s, buff);
            gettimeofday(&T1, NULL);
            printf("%s FAILED PUT %d:%d:%d  %f  %f  %s\n", self, op->i, op->j, op->r, TIMEVAL_DIFF(T0, T1), TIMEVAL_DIFF(START, T1), buff);
            store_op(op_puts,s, op->i, op->j, op->r+1);
        } else {
            LOGTUPLE(s, buff);
            gettimeofday(&T1, NULL);
            printf("%s PUT %d:%d:%d   %f  %f  %s\n", self, op->i, op->j, op->r, TIMEVAL_DIFF(T0, T1), TIMEVAL_DIFF(START, T1), buff);
            inc_put();
            destroy_tuple(s);
        }
        free(op);
    }

    void* ptr = NULL;
    pthread_join(resolver, &ptr);
    stop = 1;
    pthread_join(counter, &ptr);

    log_bytes("END");

    pthread_join(control, &ptr);

}