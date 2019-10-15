//
// Created by Pedro Akos on 2019-10-11.
//

#include "reliable_dissemination.h"


#define GC 1
#define TIMEOUT 2

struct received_msg {
    int hash;
    void* msg_contents;
    unsigned short msg_size;
    struct timespec received_at;
    struct timespec last_sent;

    unsigned short proto_request;

    //bool delivered; //if delivered or not
    list* acks; //list of remaining acks
    //YggTimer timeout;
};

static bool equal_msg(struct received_msg* msg, const int* hash) {
    return msg->hash == *hash;
}

static bool equal_addr(WLANAddr* addr, WLANAddr* addr2) {
    return memcmp(addr->data, addr2->data, WLAN_ADDR_LEN) == 0;
}


struct state {
    list* received;
    list* pending;

    list* neighs; //enough mac_addrs

    unsigned short timeout_s;
    unsigned long timeout_ns;

    unsigned short expiration;
    uuid_t myid;
    const WLANAddr* myaddr;
    short proto_id;

    short discov_id;


};

static bool clear_to_send(struct state* state, struct received_msg* msg) {
    bool send = false;
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    if(msg->last_sent.tv_sec + state->timeout_s < now.tv_sec) {
        send = true;
    } else if (msg->last_sent.tv_sec + state->timeout_s == now.tv_sec &&
                    msg->last_sent.tv_nsec + state->timeout_ns < now.tv_nsec) {
        send = true;
    }

    if(send)
        clock_gettime(CLOCK_MONOTONIC, &msg->last_sent);

    return send;
}

static struct received_msg* regist_msg(int mid, void* msg_contents, unsigned short msg_size, unsigned short proto_origin, list* acks, struct state* state) {

    struct received_msg* msg = malloc(sizeof(struct received_msg));
    msg->hash = mid;
    msg->msg_contents = msg_contents;
    msg->msg_size = msg_size;
    msg->proto_request = proto_origin;
    clock_gettime(CLOCK_MONOTONIC, &msg->received_at);
    msg->last_sent = msg->received_at;
    msg->acks = acks;

    //msg->delivered = false;
    YggTimer timeout;
    YggTimer_init(&timeout, state->proto_id, state->proto_id);
    YggTimer_set(&timeout, state->timeout_s, state->timeout_ns, 0, 0);
    YggTimer_setType(&timeout, TIMEOUT);
    YggTimer_addPayload(&timeout, &mid, sizeof(int));
    setupTimer(&timeout);
    YggTimer_freePayload(&timeout);

    list_add_item_to_tail(state->pending, msg);
    return msg;
}

static void trigger_oneHopBcast(struct state* state, int mid, list* missing_acks, unsigned short proto_request, void* payload,
                                unsigned short payload_length, struct received_msg* r_msg) {

    if(clear_to_send(state, r_msg)) {
        YggMessage msg;
        YggMessage_initBcast(&msg, state->proto_id);
        YggMessage_addPayload(&msg, (char *) &mid, sizeof(int));
        YggMessage_addPayload(&msg, (char *) &missing_acks->size, sizeof(short));
        for (list_item *it = missing_acks->head; it != NULL; it = it->next) {
            YggMessage_addPayload(&msg, it->data, WLAN_ADDR_LEN);
        }
        YggMessage_addPayload(&msg, (char *) &proto_request, sizeof(unsigned short));
        YggMessage_addPayload(&msg, (char *) payload, payload_length);

        dispatch(&msg);
        YggMessage_freePayload(&msg);
    }

}

static void deliver_msg(unsigned short proto_dest, void* contents, unsigned short contents_size, WLANAddr* src) {
    YggMessage m;
    YggMessage_initBcast(&m, proto_dest);
    m.header.src_addr.mac_addr = *src;
    m.data = contents;
    m.dataLen = contents_size;
    deliver(&m);
}

static int hash(char* to_hash, unsigned short to_hash_len) {
    return DJBHash(to_hash, to_hash_len);
}

static int generate_mid(YggRequest* req, struct state* state) {
    unsigned short to_hash_len = req->length + sizeof(unsigned short) + WLAN_ADDR_LEN + sizeof(uuid_t) + sizeof(struct timespec);
    char* to_hash = malloc(to_hash_len);
    memcpy(to_hash, req->payload, req->length);
    int off = req->length;
    memcpy(to_hash+off, &req->proto_origin, sizeof(unsigned short));
    off += sizeof(unsigned short);
    memcpy(to_hash+off, state->myaddr, WLAN_ADDR_LEN);
    off += WLAN_ADDR_LEN;
    memcpy(to_hash+off, state->myid, sizeof(uuid_t));
    off += sizeof(uuid_t);
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    memcpy(to_hash+off, &t, sizeof(struct timespec));

    int mid = hash(to_hash, to_hash_len);

    free(to_hash);

    return mid;
}

static void process_msg(YggMessage* msg, struct state* state) {
    int mid; short m_acks; unsigned short proto, payload_len; list* acks = list_init();
    void *ptr, *payload;
    ptr = YggMessage_readPayload(msg, NULL, &mid, sizeof(int));
    ptr = YggMessage_readPayload(msg, ptr, &m_acks, sizeof(short));
    for(int i = 0; i < m_acks; i++) {
        WLANAddr* addr = malloc(sizeof(WLANAddr));
        ptr = YggMessage_readPayload(msg, ptr, addr, sizeof(WLANAddr));
        list_add_item_to_head(acks, addr);
    }
    payload = YggMessage_readPayload(msg, ptr, &proto, sizeof(unsigned short));
    payload_len = msg->dataLen -  (payload - (void*) msg->data);

    struct received_msg* r_msg = list_find_item(state->received, (equal_function) equal_msg, &mid);
    struct received_msg* p_msg = list_find_item(state->pending, (equal_function) equal_msg, &mid);

    bool send = false;
    if(!r_msg && !p_msg) {
        void* to_store = malloc(payload_len); memcpy(to_store, payload, payload_len);
        list* my_acks = list_init();
        for(list_item* it = state->neighs->head; it != NULL; it = it->next) {
//            if(memcmp(((WLANAddr*)it->data)->data, &msg->header.src_addr.mac_addr, WLAN_ADDR_LEN) == 0)
//                continue;
            WLANAddr* addr = malloc(sizeof(WLANAddr)); memcpy(addr, it->data, sizeof(WLANAddr));
            list_add_item_to_head(my_acks, addr);
        }
        p_msg = regist_msg(mid, to_store, payload_len, proto, my_acks, state);
        send = true;
        //trigger_oneHopBcast(state, mid, my_acks, proto, payload, payload_len);
    }

    if(p_msg) {
        WLANAddr *addr = list_remove_item(p_msg->acks, (equal_function) equal_addr, &msg->header.src_addr.mac_addr);
        if (addr)
            free(addr);
        if (p_msg->acks->size == 0) {
            deliver_msg(proto, payload, payload_len, &msg->header.src_addr.mac_addr);
            list_remove_item(state->pending, (equal_function) equal_msg, &mid);
            list_add_item_to_head(state->received, p_msg);
        }
        r_msg = p_msg;
    }

    if(list_find_item(acks, (equal_function) equal_addr, state->myaddr) || send)
        trigger_oneHopBcast(state, mid, r_msg->acks, proto, payload, payload_len, r_msg);

    while(acks->size > 0) {
        WLANAddr* addr = list_remove_head(acks);
        free(addr);
    }
    free(acks);
}

static bool expired(struct received_msg* msg, struct state* state) {
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    return msg->received_at.tv_sec + state->expiration < now.tv_sec;
}

static void process_timer(YggTimer* timer, struct state* state) {

    if(timer->timer_type == GC) {
        while (state->received->size > 0) {
            struct received_msg *msg = list_remove_head(state->received);
            if (!expired(msg, state)) {
                list_add_item_to_head(state->received, msg);
                break;
            }
            free(msg->acks);
            free(msg->msg_contents);
            free(msg);
        }
    } else if(timer->timer_type == TIMEOUT) {
        struct received_msg* msg = list_find_item(state->pending, (equal_function) equal_msg, timer->payload);
        if(msg) {
            trigger_oneHopBcast(state, msg->hash, msg->acks, msg->proto_request, msg->msg_contents, msg->msg_size, msg);

            if(msg->acks->size == 0) { //in case msg was set for transmission without any known neighbours - solution should default to best effort
                deliver_msg(msg->proto_request, msg->msg_contents, msg->msg_size, state->myaddr);
                list_remove_item(state->pending, (equal_function) equal_msg, &msg->hash);
                list_add_item_to_head(state->received, msg);
            } else {
                YggTimer_set(timer, state->timeout_s, state->timeout_ns, 0, 0);
                setupTimer(timer);
            }
        }
        YggTimer_freePayload(timer);
    }

}

static void process_request(YggRequest* request, struct state* state) {
    if(request->request == REQUEST && request->request_type == DISSEMINATION_REQUEST) {
        int mid = generate_mid(request, state);
        list* acks = list_init();
        for(list_item* it = state->neighs->head; it != NULL; it = it->next) {
            WLANAddr* addr = malloc(sizeof(WLANAddr));
            memcpy(addr, it->data, sizeof(WLANAddr));
            list_add_item_to_head(acks, addr);
        }
        struct received_msg* msg = regist_msg(mid, request->payload, request->length, request->proto_origin, acks, state);

        trigger_oneHopBcast(state, mid, state->neighs, request->proto_origin, request->payload, request->length, msg);
    }
}

static void process_event(YggEvent* ev, struct state* state) {
    if(ev->proto_origin == state->discov_id) {
        if(ev->notification_id == NEIGHBOUR_UP) {

            uuid_t id; YggEvent_readPayload(ev, NULL, id, sizeof(uuid_t)); char str[37]; bzero(str,37); uuid_unparse(id, str);
           // printf("UP: %s\n", str);

            WLANAddr* addr = malloc(WLAN_ADDR_LEN); YggEvent_readPayload(ev, ev->payload+ sizeof(uuid_t), addr, WLAN_ADDR_LEN);
            if(!list_find_item(state->neighs, (equal_function) equal_addr, addr))
                list_add_item_to_head(state->neighs, addr);
            else
                free(addr);
        } else if(ev->notification_id == NEIGHBOUR_DOWN) {
            uuid_t id; YggEvent_readPayload(ev, NULL, id, sizeof(uuid_t)); char str[37]; bzero(str,37); uuid_unparse(id, str);
           // printf("DOWN: %s\n", str);
            WLANAddr addr; YggEvent_readPayload(ev, ev->payload+ sizeof(uuid_t), &addr, WLAN_ADDR_LEN);
            WLANAddr* addr1 = list_remove_item(state->neighs, (equal_function) equal_addr, &addr);
            if(addr1) {
                list_item* it = state->pending->head, *prev = NULL;
                while(it != NULL) {
                    bool removed = false;
                    struct received_msg* msg = (struct received_msg*) it->data;
                    WLANAddr* addr2 = list_remove_item(msg->acks, (equal_function) equal_addr, addr1);
                    if(addr2) {
                        if(msg->acks->size == 0) {
                            deliver_msg(msg->proto_request, msg->msg_contents, msg->msg_size, addr2);
                            list_remove(state->pending, prev);
                            list_add_item_to_head(state->received, msg);
                            if(prev)
                                it = prev->next; //so that next iteration progresses (prev->next = it->next)
                            else {
                                it = state->pending->head;
                            }
                            removed = true;

                        }
                        free(addr2);
                    }
                    if(!removed) {
                        prev = it;
                        it = it->next;
                    }
                }
                free(addr1);
            }
        }
    }
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
            case YGG_EVENT:
                process_event(&elem.data.event, state);
                break;
            default:
                break;
        }
    }
}

proto_def* reliable_dissemination_init(reliable_dissemination_args* args) {

    struct state* state = malloc(sizeof(struct state));
    state->received = list_init();
    state->pending = list_init();
    state->myaddr = getMyWLANAddr();
    getmyId(state->myid);
    state->proto_id = args->proto_id;
    state->expiration = args->expiration;
    state->neighs = list_init();
    state->timeout_s = args->timeout_s;
    state->timeout_ns = args->timeout_ns;
    state->discov_id = args->discov_proto_id;

    proto_def* dissemination = create_protocol_definition(state->proto_id, "Dissemination", state, NULL);

    proto_def_add_protocol_main_loop(dissemination, (Proto_main_loop) main_loop);

    proto_def_add_consumed_event(dissemination, state->discov_id, NEIGHBOUR_UP);
    proto_def_add_consumed_event(dissemination, state->discov_id, NEIGHBOUR_DOWN);

    YggTimer t; YggTimer_init(&t, state->proto_id, state->proto_id);
    YggTimer_set(&t, state->expiration, 0, state->expiration, 0);
    YggTimer_setType(&t, GC);
    setupTimer(&t);

    return dissemination;
}