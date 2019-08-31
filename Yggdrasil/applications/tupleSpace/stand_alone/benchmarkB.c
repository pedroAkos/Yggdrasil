//
// Created by Pedro Akos on 2019-08-31.
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
#include <pthread.h>

#include "tuple.h"


char* getHostname() {
    char* hostname;
    FILE* f = fopen("/etc/hostname","r");
    if(f > 0) {
        struct stat s;
        if(fstat(fileno(f), &s) < 0){
            perror("FSTAT");
        }
        hostname = malloc(s.st_size+1);
        if(hostname == NULL) {

            exit(1);
        }

        memset(hostname, 0, s.st_size+1);
        if(fread(hostname, 1, s.st_size, f) <= 0){
            if(s.st_size < 8) {
                free(hostname);
                hostname = malloc(9);
                memset(hostname,0,9);
            }
            int r = rand() % 10000;
            sprintf(hostname, "host%04d", r);
        }else{

            int i;
            for(i = 0; i < s.st_size + 1; i++){
                if(hostname[i] == '\n') {
                    hostname[i] = '\0';
                    break;
                }
            }
        }
    } else {

        hostname = malloc(9);
        memset(hostname,0,9);

        int r = rand() % 10000;
        sprintf(hostname, "host%04d", r);
    }
    return hostname;
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

pthread_mutex_t mutex;

void * resolver_app(void* args) {

    struct tuple *s, *t, *u;

    struct timeval T0, T1;

    char description[100];

    struct context ctx = *((struct context*) args);

    char self[10]; bzero(self, 10);
    memcpy(self, getHostname(), 9);
    char name[10]; bzero(name, 10);

    s = make_tuple("????s", self);

    pthread_mutex_lock(&mutex);

    gettimeofday(&T0, NULL);

    char buff[100];

    for(int i = 1; i <= 2000; i++) {
        u = read_tuple(s, &ctx);
        if(u == NULL) {
            gettimeofday(&T1, NULL);
            printf("FAILED READ %d  %f\n", i, TIMEVAL_DIFF(T0, T1));
        } else {
            t = get_tuple(u, &ctx);
            if(t == NULL) {
                gettimeofday(&T1, NULL);
                LOGTUPLE(u, buff);
                printf("FAILED GET %d  %f %s\n", i, TIMEVAL_DIFF(T0, T1), buff);
                destroy_tuple(u);
            } else {
                gettimeofday(&T1, NULL);
                LOGTUPLE(u, buff);
                printf("GET %d  %f %s\n", i, TIMEVAL_DIFF(T0, T1), buff);
                destroy_tuple(t);
                destroy_tuple(u);

            }
        }


    }

    pthread_mutex_unlock(&mutex);

//    gettimeofday(&T0, NULL);
//
//    while (1) {
//
//        u = get_tuple(s, &ctx);
//        if(u == NULL) {
//            perror("get_tuple failed");
//            exit(1);
//        }
//
//        printf("got tuple trouble\n");
//
//        t->elements[1] = u->elements[1];
//        t->elements[2] = u->elements[2];
//        t->elements[3] = u->elements[3];
//
//        bzero(t->elements[4].data.s.ptr, 100);
//        sprintf(t->elements[4].data.s.ptr, "no more trouble there");
//        t->elements[4].data.s.len = strlen(t->elements[4].data.s.ptr);
//
//        put_tuple(t, &ctx);
//        printf("put tuple no more trouble\n");
//
//
//        gettimeofday(&T1, NULL);
//        printf("%f\n", TIMEVAL_DIFF(T0, T1));
//    }
}

int
main(int argc, char *argv[])
{

    pthread_mutex_init(&mutex, NULL);

    struct tuple *s, *t, *u;

    struct timeval T0, T1;
    struct context ctx, ctx_re;

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

    pthread_t resolver;
    pthread_create(&resolver, NULL, resolver_app, &ctx);



    char self[10]; bzero(self, 10);
    memcpy(self, getHostname(), 9);
    char name[10]; bzero(name, 10);

    s = make_tuple("sidds", self , 0,0,0, name);

    char buff[100];

    gettimeofday(&T0, NULL);


    for(int i = 1; i <= 100; i++) {
        for(int j = 2; j <= 21; j++) {
            int rnd = random_int();
            double X = rnd / 2;
            double Y = rnd / 3;
            s->elements[1].data.i = i;
            s->elements[2].data.d = X;
            s->elements[3].data.d = Y;
            sprintf(name, "raspi-%02d", j);
            s->elements[4].data.s.ptr = name;
            s->elements[4].data.s.len = strlen(name);
            if(put_tuple(s, &ctx)) {
                LOGTUPLE(s, buff);
                gettimeofday(&T1, NULL);
                printf("FAILED PUT %d:%d   %f  %s\n", i, j, TIMEVAL_DIFF(T0, T1), buff);
            } else {
                LOGTUPLE(s, buff);
                gettimeofday(&T1, NULL);
                printf("PUT %d:%d   %f  %s\n", i, j, TIMEVAL_DIFF(T0, T1), buff);
            }
        }
    }

    pthread_mutex_lock(&mutex);
    pthread_mutex_unlock(&mutex);

    fflush(stdout);
//    while (1) {
//
//        int rnd = random_int();
//        int quadrant = (rnd  % 21) +1;
//        double X = rnd / 2;
//        double Y = rnd / 3;
//
//        s->elements[1].data.i = quadrant;
//        s->elements[2].data.d = X;
//        s->elements[3].data.d = Y;
//        bzero(s->elements[4].data.s.ptr, 100);
//        sprintf(s->elements[4].data.s.ptr, "trouble in %d %f %f", quadrant, X, Y);
//        s->elements[4].data.s.len = strlen(s->elements[4].data.s.ptr);
//
//        put_tuple(s, &ctx);
//
//        printf("put tuple trouble\n");
//
//        t->elements[1].data.i = quadrant;
//        t->elements[2].data.d = X;
//        t->elements[3].data.d = Y;
//
//        u = get_tuple(t, &ctx);
//
//        if (u == NULL) {
//            perror("get_tuple failed");
//            exit(1);
//        }
//
//        printf("%s\n", u->elements[4].data.s.ptr);
//
//
//        gettimeofday(&T1, NULL);
//        printf("%f\n", TIMEVAL_DIFF(T0, T1));
//
//        sleep(1);
//    }

}