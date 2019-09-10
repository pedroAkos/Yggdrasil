//
// Created by Pedro Akos on 2019-08-30.
//

#include "ygg_comm.h"


static void peername_to_macaddr(char* peername, char* addr) {
    str2wlan(addr, peername); //TODO change to be all
}

void init_msg(YggMessage* msg, char* peername, unsigned short dest_id, unsigned short src_id, int rid) {
    unsigned  char addr[6];
    peername_to_macaddr(peername, (char*) addr);
    YggMessage_init(msg, addr, dest_id);
    YggMessage_addPayload(msg, (char*) &src_id, sizeof(unsigned short));
    YggMessage_addPayload(msg, (char*) &rid, sizeof(int));

}

short send_to_destination(YggMessage* msg) {
//    char str[33]; bzero(str, 33); wlan2asc(&msg->header.dst_addr.mac_addr, str);
//    printf("Sending Tuple to %s\n", str);
    dispatch(msg);
    YggMessage_freePayload(msg);
    return 0;
}

int delivery(YggMessage* msg, void** ptr, queue_t* inBox, unsigned short self_id, int rid) {

    struct timespec uptotime;
    clock_gettime(CLOCK_REALTIME, &uptotime);
    uptotime.tv_sec+=20; //because. (basically tcp timeout)

    while(1) {
        queue_t_elem elem;
        //queue_pop(inBox, &elem);
        if (queue_try_timed_pop(inBox, &uptotime, &elem) == 1) {
            if (elem.type == YGG_MESSAGE) { //it should be
                YggMessage m = elem.data.msg;
                void *tmp_ptr = YggMessage_readPayload(&m, NULL, &m.Proto_id, sizeof(unsigned short));
                int received;
                tmp_ptr = YggMessage_readPayload(&m, tmp_ptr, &received, sizeof(int));
                if(received == rid) {
                    *msg = m;
                    *ptr = tmp_ptr;
                    return OK;
                }else {
                    printf("Received out-of-order message\n");
                    *msg = m;
                    *ptr = tmp_ptr;
                    return OUT_OF_ORDER;
                }
            } else if (elem.type == YGG_EVENT) {
                YggMessage* m = elem.data.event.payload;
                if (*((unsigned short *) m->data) == self_id) {
                    YggMessage_freePayload(m);
                    free_elem_payload(&elem);
                    printf("Failed to deliver message for protocol %d\n", self_id);
                    return ERR;
                }
                //YggMessage_freePayload(m);
                free_elem_payload(&elem);

            } else {
                printf("MAJOR PANIC\n");
                free_elem_payload(&elem);
                return ERR;
            }
        } else {
            printf("Operation timed out\n");
            return ERR;
        }
    }

}


void wait_delivery(YggMessage* msg, int* rid, void** ptr, queue_t* inBox) {
    while(1) {
        queue_t_elem elem;
        queue_pop(inBox, &elem);

        if (elem.type == YGG_MESSAGE) {
            *msg = elem.data.msg;
            *ptr = YggMessage_readPayload(msg, NULL, &msg->Proto_id, sizeof(unsigned short));
            *ptr = YggMessage_readPayload(msg, *ptr, rid, sizeof(int));
            //printf("delivery pointer at: %p %p\n", *ptr, msg->data);
            break;
        } else if(elem.type == YGG_EVENT) {
            YggMessage* m = (YggMessage*)elem.data.event.payload;
            char str[33]; bzero(str, 33);
            wlan2asc(&m->header.dst_addr.mac_addr, str);
            printf("Failed to deliver message for protocol %d in %s\n", m->Proto_id, str);
            *msg = *m;
            *ptr = YggMessage_readPayload(msg, NULL, &msg->Proto_id, sizeof(unsigned short));
            *ptr = YggMessage_readPayload(msg, *ptr, rid, sizeof(int));
            YggEvent_freePayload(&elem.data.event);
            break;

        } else {
            printf("MAJOR PANIC\n");
            free_elem_payload(&elem);
        }
    }
}

