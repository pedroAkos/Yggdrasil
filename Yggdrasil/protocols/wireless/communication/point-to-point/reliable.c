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

#include "reliable.h"

//#define TIME_TO_LIVE 5
//#define TIME_TO_RESEND 2
//#define TIMER 2 //2 seconds

//#define THRESHOLD 5 //five times resend

#define PANIC -1
#define DUPLICATE 0
#define OK 1

#define MAX_SQN ULONG_MAX

typedef enum types_{
	MSG,
	ACK
}types;

typedef struct _pending_msg{
	unsigned long sqn;
	YggMessage* msg;
	unsigned short resend;
	struct timespec time;
}pending_msg;

typedef struct _destination{
    struct timespec first;
	WLANAddr destination;
	list* msg_list;
}destination;

typedef struct meta_info_{
	unsigned long sqn;
	unsigned short type;
}meta_info;


typedef struct _reliable_state{
	short proto_id;
	unsigned long sqn;

	unsigned short ttl;
	unsigned short time_to_resend;
    unsigned long time_to_resend_ns;
	unsigned short threshold;

	list* outbound_msgs;
	list* inbound_msgs;

	WLANAddr bcastAddr;
	queue_t* dispatcher_queue;
	YggTimer* garbage_collect;
}p2p_reliable_state;

static void* serialize_meta_info(meta_info* info){
	void* buffer = malloc(sizeof(meta_info));

	memcpy(buffer, &info->sqn, sizeof(unsigned long));
	memcpy(buffer + sizeof(unsigned long), &info->type, sizeof(unsigned short));

	return buffer;
}

static void deserialize_meta_info(void* buffer, meta_info* info){
	memcpy(&info->sqn, buffer, sizeof(unsigned long));
	memcpy(&info->type, buffer + sizeof(unsigned long), sizeof(unsigned short));
}

static bool equal_destination(destination* dest, WLANAddr* addr) {
	return memcmp(dest->destination.data, addr->data, WLAN_ADDR_LEN) == 0;
}

static void store_outbond(YggMessage* msg, p2p_reliable_state* state){
	//simple store
	destination* dest = list_find_item(state->outbound_msgs, (equal_function) equal_destination, &msg->header.dst_addr.mac_addr);
	if(!dest) {
		dest = malloc(sizeof(destination));
		memcpy(dest->destination.data, msg->header.dst_addr.mac_addr.data, WLAN_ADDR_LEN);
		clock_gettime(CLOCK_MONOTONIC, &dest->first);
		list_add_item_to_tail(state->outbound_msgs, dest);
		dest->msg_list = list_init();
	}


	//add msg to list
	pending_msg* m = malloc(sizeof(pending_msg));
	m->msg = malloc(sizeof(YggMessage));
	memcpy(m->msg, msg, sizeof(YggMessage));
	m->sqn = state->sqn;
	m->resend = 0;
	clock_gettime(CLOCK_MONOTONIC, &m->time);

	list_add_item_to_head(dest->msg_list, m);
}

static bool equal_sqn(pending_msg* msg, unsigned long* sqn) {
	return msg->sqn == *sqn;
}

static short store_inbond(unsigned long recv_sqn, YggMessage* msg, p2p_reliable_state* state){
	//check if it and store it

	destination* dest = list_find_item(state->inbound_msgs, (equal_function) equal_destination, &msg->header.src_addr.mac_addr);

	if(!dest) {
		dest = malloc(sizeof(destination));
		memcpy(dest->destination.data, msg->header.src_addr.mac_addr.data, WLAN_ADDR_LEN);
		dest->msg_list = list_init();
		list_add_item_to_tail(state->inbound_msgs, dest);
	}

	pending_msg* m = list_find_item(dest->msg_list, (equal_function) equal_sqn, &recv_sqn);
	if(m){
		return DUPLICATE;
	}else {
		m = malloc(sizeof(pending_msg));
		m->sqn = recv_sqn;
		m->resend = 0;
		m->msg = NULL;
		clock_gettime(CLOCK_MONOTONIC, &m->time);
		list_add_item_to_tail(dest->msg_list, m);
	}

	return OK;

}

static short rm_outbond(unsigned long ack_sqn, WLANAddr* addr, p2p_reliable_state* state){

	destination* dest = list_find_item(state->outbound_msgs, (equal_function) equal_destination, addr);

	if(dest == NULL || dest->msg_list->size == 0){
		return PANIC;
	}

	pending_msg* msg = list_remove_item(dest->msg_list, (equal_function) equal_sqn, &ack_sqn);

	if(msg) {
		YggMessage_freePayload(msg->msg);
		free(msg->msg);
		free(msg);
		return OK;
	}

	return PANIC;
}

static short rm_inbond(p2p_reliable_state* state){
	list_item* it = state->inbound_msgs->head;
	while(it != NULL){
		destination* dest = it->data;
		if(dest->first.tv_sec != 0 &&  dest->first.tv_sec + state->ttl <  time(NULL)){
			list_item* it2 = dest->msg_list->head;
			while(it2 != NULL) {
				pending_msg* m = it2->data;
				if(m->time.tv_sec + state->ttl < time(NULL)){
					pending_msg* torm = list_remove_head(dest->msg_list);
					if(dest->msg_list->head != NULL) {
						m = dest->msg_list->head->data;
						dest->first = m->time;
					}
					else{
						dest->first.tv_sec = 0;
					}

					//YggMessage_freePayload(torm->msg);
					//free(torm->msg);
					free(torm);
				} else
					break;
			}
		}

		it = it->next;
	}

	return OK;
}

static void prepareAck(YggMessage* msg, unsigned long recv_sqn, WLANAddr* dest, p2p_reliable_state* state){
	YggMessage_init(msg, dest->data, state->proto_id);

	meta_info info;
	info.sqn = recv_sqn;
	info.type = ACK;

	void* buffer = serialize_meta_info(&info);

	pushPayload(msg, (char *) buffer, sizeof(meta_info), state->proto_id, dest);

	free(buffer);
}


static void notify_failed_delivery(YggMessage* msg, p2p_reliable_state* state){

	void* buffer = malloc(sizeof(meta_info));
	popPayload(msg, (char *) buffer, sizeof(meta_info));
	free(buffer);

	YggEvent ev;
	YggEvent_init(&ev, state->proto_id, FAILED_DELIVERY);
	YggEvent_addPayload(&ev, msg, sizeof(YggMessage));

	deliverEvent(&ev);

	YggEvent_freePayload(&ev);
}

static bool expired(struct timespec* t, p2p_reliable_state* state) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    struct timespec _t = *t;
    _t.tv_sec += state->time_to_resend;
    setNanoTime(&_t, state->time_to_resend_ns);

    return compare_timespec(&_t, &now) < 0;

}

static void * reliable_point2point_main_loop(main_loop_args* args){

	queue_t* inBox = args->inBox;
	p2p_reliable_state* state = args->state;

	while(1){
		queue_t_elem elem;
		queue_pop(inBox, &elem);

		if(elem.type == YGG_MESSAGE) {

			if(elem.data.msg.Proto_id != state->proto_id){
				//message from some protocol
				if(memcmp(elem.data.msg.header.dst_addr.mac_addr.data, state->bcastAddr.data, WLAN_ADDR_LEN) != 0){
					//only keep track of point to point messages
					meta_info info;
					info.sqn = state->sqn;
					info.type = MSG;

					void* buffer = malloc(sizeof(meta_info));
					memcpy(buffer, &info.sqn, sizeof(unsigned long));
					memcpy(buffer + sizeof(unsigned long), &info.type, sizeof(unsigned short));

					pushPayload(&elem.data.msg,(char *) buffer, sizeof(meta_info), state->proto_id, &elem.data.msg.header.dst_addr.mac_addr);

					store_outbond(&elem.data.msg, state);

//					char str[33]; bzero(str, 33); wlan2asc(&elem.data.msg.header.dst_addr.mac_addr, str);
//					printf("sending msg %ld to %s\n", info.sqn, str);

					state->sqn = (state->sqn + 1) % MAX_SQN;

					free(buffer);

				}

				queue_push(state->dispatcher_queue, &elem);

			}else{
				//message from the network

				void* buffer = malloc(sizeof(meta_info));

				popPayload(&elem.data.msg, (char *) buffer, sizeof(meta_info));

				meta_info info;

				deserialize_meta_info(buffer, &info);

				free(buffer);

				if(info.type == ACK){
					//check if is ack, and rm from outbond

//                    char str[33]; bzero(str, 33); wlan2asc(&elem.data.msg.header.src_addr.mac_addr, str);
//                    printf("received ack for msg %ld from %s\n", info.sqn, str);

					if(rm_outbond(info.sqn, &elem.data.msg.header.src_addr.mac_addr, state) == PANIC){
					    char log[150], str[33]; bzero(log, 150); bzero(str, 33); wlan2asc(&elem.data.msg.header.src_addr.mac_addr, str);
					    sprintf(log, "Tried to remove non existing out bound message sqn: %ld now on: %ld from %s", info.sqn, state->sqn, str);
					    ygg_log("RELIABLE POINT2POINT", "PANIC", log);
					}

				}else if(info.type == MSG){

//                    char str[33]; bzero(str, 33); wlan2asc(&elem.data.msg.header.src_addr.mac_addr, str);
//                    printf("received msg %ld from %s\n", info.sqn, str);

					//process msg
					if(store_inbond(info.sqn, &elem.data.msg, state) == OK){
						deliver(&elem.data.msg);
					} else {
                        char log[100], str[33]; bzero(log, 100); bzero(str, 33); wlan2asc(&elem.data.msg.header.src_addr.mac_addr, str);
                        sprintf(log, "Received dup message sqn: %ld from %s", info.sqn, str);
                        ygg_log("RELIABLE POINT2POINT", "LOG", log);
					}

					prepareAck(&elem.data.msg, info.sqn, &elem.data.msg.header.src_addr.mac_addr, state);

					queue_push(state->dispatcher_queue, &elem);
				}else{
					ygg_log("RELIABLE POINT2POINT", "WARNING", "Unknown message type");
				}

			}

		}else if(elem.type == YGG_TIMER) {

			//resend everything that is pending confirmation
			list_item* it = state->outbound_msgs->head;

			while(it != NULL){
				destination* dest  = it->data;
				list_item* it2 = dest->msg_list->head;
				list_item* prev = NULL;
				while(it2 != NULL){
					pending_msg* m = it2->data;
					if(m->resend > state->threshold){

						if(prev == NULL) {
						    it2 = it2->next;
							m = list_remove_head(dest->msg_list);
						}
						else {
							m = list_remove(dest->msg_list, prev);
							if(prev == dest->msg_list->tail)
								it2 = NULL;
							else
								it2 = prev->next;
						}

						notify_failed_delivery(m->msg, state);
						YggMessage_freePayload(m->msg);
						free(m->msg);
						free(m);
					}else if( expired(&m->time, state) ){

						elem.type = YGG_MESSAGE;
						elem.data.msg = *m->msg;
						//memcpy(&elem.data.msg, m->msg, sizeof(YggMessage));

//                        char str[33]; bzero(str, 33); wlan2asc(&elem.data.msg.header.dst_addr.mac_addr, str);
//                        printf("re sending msg %ld to %s\n", m->sqn, str);

						queue_push(state->dispatcher_queue, &elem);

						m->resend ++;
						clock_gettime(CLOCK_MONOTONIC, &m->time);

						prev = it2;
						it2 = it2->next;
					}else{
						prev = it2;
						it2 = it2->next;
					}
				}

				it = it->next;
			}

			//get some memory
			rm_inbond(state);

		}else{
		    queue_push(state->dispatcher_queue, &elem);
			//ygg_log("RELIABLE POINT2POINT", "WARNING", "Got something unexpected");
            free_elem_payload(&elem);
		}


	}

	return NULL;
}

static void destroy_msgs(list* msgs) {
	while(msgs->size > 0) {
		destination* dest = list_remove_head(msgs);
		while(dest->msg_list->size > 0) {
			pending_msg* m = list_remove_head(dest->msg_list);
			free(m->msg);
			free(m);
		}
		free(dest->msg_list);
		free(dest);
	}
}

static void reliable_p2p_destroy(p2p_reliable_state* state) {
	if(state->garbage_collect) {
		cancelTimer(state->garbage_collect);
		free(state->garbage_collect);
	}
	if(state->inbound_msgs) {
		destroy_msgs(state->inbound_msgs);
		free(state->inbound_msgs);
	}
	if(state->outbound_msgs) {
		destroy_msgs(state->outbound_msgs);
		free(state->outbound_msgs);
	}
	state->dispatcher_queue = NULL;
	free(state);
}

proto_def* reliable_point2point_init(void* args) {
	p2p_reliable_state* state = malloc(sizeof(p2p_reliable_state));
	state->inbound_msgs = list_init();
	state->outbound_msgs = list_init();
	state->proto_id = PROTO_P2P_RELIABLE_DELIVERY;
	state->dispatcher_queue = interceptProtocolQueue(PROTO_DISPATCH, state->proto_id);

	setBcastAddr(&state->bcastAddr);
	state->sqn = 1;

	state->threshold = ((reliable_p2p_args*)args)->threshold;
    state->ttl = ((reliable_p2p_args*)args)->ttl;
    state->time_to_resend = ((reliable_p2p_args*)args)->time_to_resend_s;
    state->time_to_resend_ns = ((reliable_p2p_args*)args)->time_to_resend_ns;

	proto_def* p2p_reliable = create_protocol_definition(state->proto_id, "Reliable P2P delivery", state, (Proto_destroy) reliable_p2p_destroy);
	proto_def_add_protocol_main_loop(p2p_reliable, (Proto_main_loop) reliable_point2point_main_loop);
	proto_def_add_produced_events(p2p_reliable, 1);

	state->garbage_collect = malloc(sizeof(YggTimer));
	YggTimer_init(state->garbage_collect, state->proto_id, state->proto_id);
	YggTimer_set(state->garbage_collect, state->time_to_resend/2, state->time_to_resend_ns, state->time_to_resend/2, state->time_to_resend_ns);
	setupTimer(state->garbage_collect);

	return p2p_reliable;

}

reliable_p2p_args* reliableP2PArgs_init(unsigned short ttl, unsigned short time_to_resend_s, unsigned long time_to_resend_ns, unsigned short threshold) {
    reliable_p2p_args* args = malloc(sizeof(reliable_p2p_args));
    args->ttl = ttl;
    args->time_to_resend_s = time_to_resend_s;
    args->time_to_resend_ns = time_to_resend_ns;
    args->threshold = threshold;
}

void reliableP2PArgs_destroy(reliable_p2p_args* args) {
    free(args);
}

