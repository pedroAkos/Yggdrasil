//
// Created by Pedro Akos on 2019-10-11.
//

#include "dissemination.h"

#define GC 1

struct received_msg {
    int hash;
    void* msg_contents;
    unsigned short msg_size;
    struct timespec received_at;
};

static bool equal_msg(struct received_msg* msg, const int* hash) {
    return msg->hash == *hash;
}

struct state {
    list* received;

    unsigned short expiration;
    uuid_t myid;
    WLANAddr* myaddr;
    short proto_id;
};

static void regist_msg(int mid, void* msg_contents, unsigned short msg_size, struct state* state) {
    struct received_msg* msg = malloc(sizeof(struct received_msg));
    msg->hash = mid;
    msg->msg_contents = msg_contents;
    msg->msg_size = msg_size;
    clock_gettime(CLOCK_MONOTONIC, &msg->received_at);

    list_add_item_to_tail(state->received, msg);
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
    int mid;
    void* ptr = YggMessage_readPayload(msg, NULL, &mid, sizeof(int));
    if(!list_find_item(state->received, (equal_function) equal_msg, &mid)) {
        regist_msg(mid, msg->data, msg->dataLen, state);
        unsigned short proto;
        ptr = YggMessage_readPayload(msg, ptr, &proto, sizeof(unsigned short));
        deliver_msg(proto, ptr, msg->dataLen - sizeof(int) - sizeof(unsigned short), &msg->header.src_addr.mac_addr);

        dispatch(msg);
    }
    //YggMessage_freePayload(msg);
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
            free(msg->msg_contents);
            free(msg);
        }
    }
}

static void process_request(YggRequest* request, struct state* state) {
    if(request->request == REQUEST && request->request_type == DISSEMINATION_REQUEST) {
        deliver_msg(request->proto_origin, request->payload, request->length, state->myaddr);
        int mid = generate_mid(request, state);
        YggMessage msg; YggMessage_initBcast(&msg, state->proto_id);
        YggMessage_addPayload(&msg, (char*) &mid, sizeof(int));
        YggMessage_addPayload(&msg, (char*) &request->proto_origin, sizeof(unsigned short));
        YggMessage_addPayload(&msg, (char*) request->payload, request->length);
        regist_msg(mid, msg.data, msg.dataLen, state);
        dispatch(&msg);
    }
    YggRequest_freePayload(request);
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

proto_def* dissemination_init(dissemination_args* args) {

    struct state* state = malloc(sizeof(struct state));
    state->received = list_init();
    state->myaddr = getMyWLANAddr();
    getmyId(state->myid);
    state->proto_id = args->proto_id;
    state->expiration = args->expiration;

    proto_def* dissemination = create_protocol_definition(state->proto_id, "Dissemination", state, NULL);

    proto_def_add_protocol_main_loop(dissemination, (Proto_main_loop) main_loop);

    YggTimer t; YggTimer_init(&t, state->proto_id, state->proto_id);
    YggTimer_set(&t, state->expiration, 0, state->expiration, 0);
    YggTimer_setType(&t, GC);
    setupTimer(&t);

    return dissemination;
}