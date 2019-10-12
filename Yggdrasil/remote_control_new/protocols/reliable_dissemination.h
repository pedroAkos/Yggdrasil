//
// Created by Pedro Akos on 2019-10-11.
//

#ifndef YGGDRASIL_RELIABLE_DISSEMINATION_H
#define YGGDRASIL_RELIABLE_DISSEMINATION_H

#include "core/ygg_runtime.h"
#include "core/data_structures/generic/list.h"
#include "utils/hashfunctions.h"

#include "interfaces/discovery/discovery_events.h"

#define DISSEMINATION_REQUEST 77

typedef struct __reliable_dissemination_args {
    short discov_proto_id;

    unsigned short expiration;
    unsigned short timeout_s;
    unsigned long timeout_ns;

    short proto_id;
}reliable_dissemination_args;

proto_def* reliable_dissemination_init(reliable_dissemination_args* args);

#endif //YGGDRASIL_RELIABLE_DISSEMINATION_H
