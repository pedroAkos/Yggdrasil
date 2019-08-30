//
// Created by Pedro Akos on 2019-08-30.
//

#ifndef YGGDRASIL_YGG_COMM_H
#define YGGDRASIL_YGG_COMM_H

#include "core/ygg_runtime.h"

void init_msg(YggMessage* msg, char* peername, unsigned short dest_id, unsigned short src_id);

short send_to_destination(YggMessage* msg);

bool delivery(YggMessage* msg, void** ptr, queue_t* inBox);

void wait_delivery(YggMessage* msg, void** ptr, queue_t* inBox);


#endif //YGGDRASIL_YGG_COMM_H
