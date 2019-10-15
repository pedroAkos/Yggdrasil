//
// Created by Pedro Akos on 2019-10-11.
//

#include "discovery.h"

#define MAX_SEQ 10000

#define RELIABLE_THRESHOLD 12
#define UNRELIABLE_THRESHOLD 4

static WLANAddr bcast_address;

struct neigh {
    uuid_t id;
    WLANAddr mac;
    uint32_t reliability;
    short last;
    short count;
    bool notified;
    bool sym;
};

static void mark_msg(struct neigh* n,  unsigned short msg_num) {

    short to_mv = msg_num - n->last;
    if(to_mv < 0)
        to_mv = (MAX_SEQ - n->last) + msg_num;

//    char str[37]; bzero(str, 37); uuid_unparse(n->id, str);
//    printf("neigh: %s %p is %d reliable (%d) to_mv: %d count: %d seq: %d notified: %s\n", str, n, n->count, n->reliability, to_mv,n->last, msg_num, n->notified == true ? "yes" : "no");

    uint32_t reliability = n->reliability;

    reliability = reliability << (uint32_t) to_mv;

    uint32_t flag = 1;
    reliability |= flag;

    n->last = msg_num;
    n->count = __builtin_popcount(reliability);
    n->reliability = reliability;
}

static bool equal_neigh(struct neigh* n, WLANAddr* addr) {
    return memcmp(n->mac.data, addr->data, WLAN_ADDR_LEN) == 0;
}

struct state {
    list* neighs;

    uuid_t myid;
    const WLANAddr* myaddr;

    short proto_id;
    YggTimer announce;

    int announce_period_s;
    int announce_period_ns;

    bool status;

    unsigned short curr;

    queue_t* dispatcher;
};

static void inc_curr(struct state* state) {
    state->curr = (state->curr + 1) % MAX_SEQ;
}

static void check_if_reliable(struct state* state, struct neigh* n) {

//    char str[37]; bzero(str, 37); uuid_unparse(n->id, str);
//    printf("neigh: %s is %d reliable (%d) count: %d notified: %s\n", str, n->count, n->reliability, n->last, n->notified == true ? "yes" : "no");
    if(!n->notified && n->count > RELIABLE_THRESHOLD && n->sym) {
        send_event_neighbour_up(state->proto_id, n->id, &n->mac);
        n->notified = true;
    } else if (n->notified && (n->count < UNRELIABLE_THRESHOLD || !n->sym)) {
        send_event_neighbour_down(state->proto_id, n->id, &n->mac);
        n->notified = false;
    }
}

static void process_msg(YggMessage* msg, struct state* state) {

    if(msg->Proto_id != state->proto_id) {
        pushPayload(msg, (char*) &state->curr, sizeof(unsigned short),state->proto_id, &bcast_address);
        directDispatch(msg);
        inc_curr(state);
    } else { //comes from the network
        unsigned short count;
        popPayload(msg, (char*) &count, sizeof(unsigned short));
        if(msg->Proto_id != state->proto_id)
            filterAndDeliver(msg);

        struct neigh* n = list_find_item(state->neighs, (equal_function) equal_neigh, &msg->header.src_addr.mac_addr);
        if(!n) {
            n = malloc(sizeof(struct neigh));
            n->mac = msg->header.src_addr.mac_addr;
            n->notified = false;
            n->reliability = 0;
            n->last = count;
            n->sym = false;
            list_add_item_to_tail(state->neighs, n);
        }

        if(msg->Proto_id == state->proto_id) {
            n->sym = false;
            void* ptr = YggMessage_readPayload(msg, NULL, n->id, sizeof(uuid_t));
            short neigh_count; ptr = YggMessage_readPayload(msg, ptr, &neigh_count, sizeof(short));
            for(short i = 0; i < neigh_count; i ++) {
                WLANAddr addr; ptr = YggMessage_readPayload(msg, ptr, addr.data, WLAN_ADDR_LEN);
                if(memcmp(addr.data, state->myaddr->data, WLAN_ADDR_LEN) == 0)
                    n->sym = true;
            }
        }

        mark_msg(n, count);
        check_if_reliable(state, n);
    }

    YggMessage_freePayload(msg);
}

static void process_timer(YggTimer* timer, struct state* state) {
    if(state->status) {
        YggMessage msg;
        YggMessage_initBcast(&msg, state->proto_id);
        YggMessage_addPayload(&msg, (char*) state->myid, sizeof(uuid_t));
        YggMessage_addPayload(&msg, (char*) &state->neighs->size, sizeof(short));
        for(list_item* it = state->neighs->head; it != NULL; it = it->next) {
            if(((struct neigh*)it->data)->count > RELIABLE_THRESHOLD)
                YggMessage_addPayload(&msg, (char*) ((struct neigh*) it->data)->mac.data, WLAN_ADDR_LEN);
            else
                *((short *)(msg.data +sizeof(uuid_t))) = *((short *)(msg.data +sizeof(uuid_t))) - 1;
        }

        pushPayload(&msg, (char*) &state->curr, sizeof(unsigned short), state->proto_id, &bcast_address);

        directDispatch(&msg);
        YggMessage_freePayload(&msg);
        inc_curr(state);
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
                if(elem.data.timer.proto_dest != state->proto_id)
                    queue_push(state->dispatcher, &elem);
                else
                    process_timer(&elem.data.timer, state);
                break;
            case YGG_REQUEST:
                if(elem.data.request.proto_dest != state->proto_id)
                    queue_push(state->dispatcher, &elem);
                else
                    process_request(&elem.data.request, state);
                break;
            default:
                queue_push(state->dispatcher, &elem);
                break;
        }
        free_elem_payload(&elem);
    }
}


proto_def* discovery_init(discovery_args* args) {
    struct state* state = malloc(sizeof(struct state));
    state->neighs = list_init();
    state->status = true;
    state->proto_id = args->proto_id;
    state->announce_period_s = args->announce_period_s;
    state->announce_period_ns = args->announce_period_ns;
    state->curr = 0;

    getmyId(state->myid);

    setBcastAddr(&bcast_address);
    state->myaddr = getMyWLANAddr();

    state->dispatcher = interceptProtocolQueue(PROTO_DISPATCH, state->proto_id);

    proto_def* discov = create_protocol_definition(state->proto_id, "Discovery", state, NULL);

    proto_def_add_protocol_main_loop(discov, (Proto_main_loop) main_loop);

    proto_def_add_produced_events(discov, 2); //NEIGH_UP, NEIGH_DOWN

    YggTimer_init(&state->announce, state->proto_id, state->proto_id);
    YggTimer_set(&state->announce, state->announce_period_s, state->announce_period_ns,
            state->announce_period_s, state->announce_period_ns);
    setupTimer(&state->announce);

    return discov;
}


