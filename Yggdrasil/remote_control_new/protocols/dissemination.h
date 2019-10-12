//
// Created by Pedro Akos on 2019-10-11.
//

#ifndef YGGDRASIL_DISSEMINATION_H
#define YGGDRASIL_DISSEMINATION_H

#include "core/ygg_runtime.h"
#include "core/data_structures/generic/list.h"
#include "utils/hashfunctions.h"

#define DISSEMINATION_REQUEST 77

typedef struct __dissemination_args{
    unsigned short expiration;
    short proto_id;
}dissemination_args;

proto_def* dissemination_init(dissemination_args* args);

#endif //YGGDRASIL_DISSEMINATION_H
