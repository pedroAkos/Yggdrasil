//
// Created by Pedro Akos on 2019-10-11.
//

#ifndef YGGDRASIL_CMD_SERVER_H
#define YGGDRASIL_CMD_SERVER_H

#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "ygg_runtime.h"
#include "remote_control_new/utils/control_protocol_utils.h"

void cmd_server_init(queue_t* inBox);

#endif //YGGDRASIL_CMD_SERVER_H
