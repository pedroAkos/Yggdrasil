//
// Created by Pedro Akos on 2019-10-11.
//

#include "discovery.h"

struct state {
    list* neighs;

    uuid_t myid;

    short proto_id;
    YggTimer announce;

    int announce_period_s;
    int announce_period_ns;

    bool status;
};


static void process_msg(YggMessage* msg, struct state* state) {
    if(state->status) {
        if(!neighbour_find_by_addr(state->neighs, &msg->header.src_addr.mac_addr)) {
            uuid_t id; YggMessage_readPayload(msg, NULL, id, sizeof(uuid_t));
            neighbour_item* neigh = new_neighbour(id, msg->header.src_addr.mac_addr, NULL, 0, NULL);
            neighbour_add_to_list(state->neighs, neigh);

            send_event_neighbour_up(state->proto_id, id, &msg->header.src_addr.mac_addr);
        }
    }
    YggMessage_freePayload(msg);
}

static void process_timer(YggTimer* timer, struct state* state) {
    if(state->status) {
        YggMessage msg;
        YggMessage_initBcast(&msg, state->proto_id);
        YggMessage_addPayload(&msg, (char*) state->myid, sizeof(uuid_t));

        directDispatch(&msg);
        YggMessage_freePayload(&msg);
    }
}

static void process_request(YggRequest* req, struct state* state) {
    if(req->request == REQUEST) {
        if(req->request_type == ACTIVATE_DISCOV && !state->status) {
            YggTimer_set(&state->announce, state->announce_period_s, state->announce_period_ns,
                    state->announce_period_s, state->announce_period_ns);
            setupTimer(&state->announce);
            state->status = true;
        } else if (req->request_type == DEACTIVATE_DISCOV && state->status) {
            cancelTimer(&state->announce);
            state->status = false;
        } else {
            //ignore
        }
    }
    YggRequest_freePayload(req);
}


static void main_loop(main_loop_args* args) {
    queue_t* inBox = args->inBox;
    struct state* state = (struct state*) args->state;
    while(true) {
        queue_t_elem elem;
        queue_pop(inBox, &elem);
        switch (elem.type) {
            case YGG_MESSAGE:
                process_msg(&elem.data.msg, state);
                break;
            case YGG_TIMER:
                process_timer(&elem.data.timer, state);
                break;
            case YGG_REQUEST:
                process_request(&elem.data.request, state);
                break;
            default:
                break;
        }
    }
}


proto_def* discovery_init(discovery_args* args) {
    struct state* state = malloc(sizeof(struct state));
    state->neighs = list_init();
    state->status = true;
    state->proto_id = args->proto_id;
    state->announce_period_s = args->announce_period_s;
    state->announce_period_ns = args->announce_period_ns;

    proto_def* discov = create_protocol_definition(state->proto_id, "Discovery", state, NULL);

    proto_def_add_protocol_main_loop(discov, (Proto_main_loop) main_loop);

    proto_def_add_produced_events(discov, 2); //NEIGH_UP, NEIGH_DOWN

    YggTimer_init(&state->announce, state->proto_id, state->proto_id);
    YggTimer_set(&state->announce, state->announce_period_s, state->announce_period_ns,
            state->announce_period_s, state->announce_period_ns);
    setupTimer(&state->announce);

    return discov;
}


