//
// Created by Pedro Akos on 2019-08-30.
//

#include "ygg_comm.h"


static void peername_to_macaddr(char* peername, char* addr) {
    str2wlan(addr, "b8:27:eb:3a:96:0e"); //TODO change to be all
}

void init_msg(YggMessage* msg, char* peername, unsigned short dest_id, unsigned short src_id) {
    char addr[6];
    peername_to_macaddr(peername, addr);
    YggMessage_init(msg, addr, dest_id);
    YggMessage_addPayload(msg, &src_id, sizeof(unsigned short));

}

short send_to_destination(YggMessage* msg) {
    dispatch(msg);
    YggMessage_freePayload(msg);
}

bool delivery(YggMessage* msg, void** ptr, queue_t* inBox, unsigned short self_id) {

    struct timespec uptotime;
    clock_gettime(CLOCK_REALTIME, &uptotime);
    uptotime.tv_sec+=10; //because. (basically tcp timeout)

    while(1) {
        queue_t_elem elem;
        //queue_pop(inBox, &elem);
        if (queue_try_timed_pop(inBox, &uptotime, &elem) == 1) {
            if (elem.type == YGG_MESSAGE) { //it should be
                *msg = elem.data.msg;
                *ptr = YggMessage_readPayload(msg, NULL, &msg->Proto_id, sizeof(unsigned short));
                return true;
            } else if (elem.type == YGG_EVENT) {
                msg = elem.data.event.payload;
                if (*((unsigned short *) msg->data) == self_id) {
                    free_elem_payload(&elem);
                    printf("Failed to deliver message for protocol %d\n", self_id);
                    return false;
                }
                free_elem_payload(&elem);

            } else {
                printf("MAJOR PANIC\n");
                free_elem_payload(&elem);
                return false;
            }
        } else {
            printf("Operation timed out\n");
            return false;
        }
    }

}


void wait_delivery(YggMessage* msg, void** ptr, queue_t* inBox) {
    while(1) {
        queue_t_elem elem;
        queue_pop(inBox, &elem);

        if (elem.type == YGG_MESSAGE) {
            *msg = elem.data.msg;
            *ptr = YggMessage_readPayload(msg, NULL, &msg->Proto_id, sizeof(unsigned short));
            break;
        } else if(elem.type == YGG_EVENT) {
            msg = elem.data.event.payload;
            char str[33]; bzero(str, 33);
            wlan2asc(&msg->header.dst_addr.mac_addr, str);
            printf("Failed to deliver message for protocol %d in %s\n", msg->Proto_id, str);
            free_elem_payload(&elem);

        } else {
            printf("MAJOR PANIC\n");
            free_elem_payload(&elem);
        }
    }
}

