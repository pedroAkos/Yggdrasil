/*********************************************************
 * This code was written in the context of the Lightkone
 * European project.
 * Code is of the authorship of NOVA (NOVA LINCS @ DI FCT
 * NOVA University of Lisbon)
 * Authors:
 * Pedro Ákos Costa (pah.costa@campus.fct.unl.pt)
 * João Leitão (jc.leitao@fct.unl.pt)
 * (C) 2018
 *********************************************************/

#ifndef PROTOCOLS_COMMUNICATION_POINT_TO_POINT_RELIABLE_H_
#define PROTOCOLS_COMMUNICATION_POINT_TO_POINT_RELIABLE_H_

#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <limits.h>

#include "core/ygg_runtime.h"
#include "src_wireless/Yggdrasil_wireless_lowlvl.h"

#include "core/utils/utils.h"

#include "data_structures/generic/list.h"

#define PROTO_P2P_RELIABLE_DELIVERY 150

typedef enum reliable_p2p_events_{
	FAILED_DELIVERY
}reliable_p2p_events;

typedef struct _reliable_p2p_args {
    unsigned short ttl;
    unsigned short time_to_resend_s;
    unsigned long time_to_resend_ns;

    unsigned short threshold;

}reliable_p2p_args;

proto_def * reliable_point2point_init(void * args);


reliable_p2p_args* reliableP2PArgs_init(unsigned short ttl/**/,
                                        unsigned short time_to_resend_s/**/,
                                        unsigned long time_to_resend_ns,
                                        unsigned short threshold/**/);

void reliableP2PArgs_destroy(reliable_p2p_args* args);

#endif /* PROTOCOLS_COMMUNICATION_POINT_TO_POINT_RELIABLE_H_ */
