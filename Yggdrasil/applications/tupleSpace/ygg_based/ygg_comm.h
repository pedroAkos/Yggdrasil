//
// Created by Pedro Akos on 2019-08-30.
//

#ifndef YGGDRASIL_YGG_COMM_H
#define YGGDRASIL_YGG_COMM_H

#include "core/ygg_runtime.h"

enum delivery_status {
    OK = 0,
    ERR = 1,
    OUT_OF_ORDER = 2,
};

void init_msg(YggMessage* msg, char* peername, unsigned short dest_id, unsigned short src_id, int rid);

short send_to_destination(YggMessage* msg);

int delivery(YggMessage* msg, void** ptr, queue_t* inBox, unsigned short self_id, int rid);

void wait_delivery(YggMessage* msg, int* rid, void** ptr, queue_t* inBox);


#endif //YGGDRASIL_YGG_COMM_H
