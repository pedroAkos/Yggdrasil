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

#include "batman.h"

#define SEQNO_MAX 65535

#define BI_LINK_TIMEOUT 2

static short OGM = 0;
//static short MSG = 1;


typedef struct stats_{

	neighbour_item* neighbour_ref;

	short* window;
	int packet_count;
	time_t last_valid;
	unsigned short last_ttl;

}stats;

typedef struct originator_entry_{
	uuid_t node_id;
	WLANAddr addr;
	time_t last_awere_time;

	unsigned short current_seq_number;

	neighbour_item* best_hop;
	list* best_next_hops;
}originator_entry;


typedef struct _batman_state {
	unsigned int seq_number;
	unsigned int last_seq_number_sent;
	unsigned short ttl;
	int window_size;

	short protoId;

	uuid_t my_id;
	WLANAddr my_addr;
	WLANAddr bcast_addr;

	YggTimer* announce;
	YggTimer* log_routing_table;

	list* routing_table;
	list* direct_links;

	queue_t* dispatcher_queue;

}batman_state;



static bool is_bcast(WLANAddr* from, batman_state* state) {
    return memcmp(from->data, state->bcast_addr.data, WLAN_ADDR_LEN) == 0;
}

static bool equal_entry_id(originator_entry* entry, uuid_t origin) {
	return uuid_compare(entry->node_id, origin) == 0;
}

static bool equal_entry_mac(originator_entry* entry, WLANAddr* addr) {
    return memcmp(entry->addr.data, addr->data, WLAN_ADDR_LEN) == 0;
}

static bool equal_ref(stats* hop, neighbour_item* tofind) {
	return hop->neighbour_ref == tofind;
}

static stats* find_hop(list* hops, neighbour_item* hop) {
	return list_find_item(hops, (equal_function) equal_ref, hop);
}


static bool is_bidirectional(int last_recorded, batman_state* state) {
    return ((last_recorded + BI_LINK_TIMEOUT)% SEQNO_MAX) >= state->last_seq_number_sent;
}

static neighbour_item* set_best_hop(originator_entry* origin, batman_state* state) {
	list_item* it = origin->best_next_hops->head;
	neighbour_item* best = origin->best_hop;

	stats* hop = find_hop(origin->best_next_hops, best);
	int p;
	if(hop != NULL && is_bidirectional( *((int*)best->attribute), state))
        p = hop->packet_count;
    else
		p = 0;

	while(it != NULL) {
		stats* next_hops = it->data;
		if(p < next_hops->packet_count && is_bidirectional( *((int*)next_hops->neighbour_ref->attribute), state)){
			p = next_hops->packet_count;
			best = next_hops->neighbour_ref;
		}

		it = it->next;
	}

	return best;
}

static neighbour_item* find_best_next_hop(list* routing_table, WLANAddr* dest) {

	originator_entry* entry = list_find_item(routing_table, (equal_function) equal_entry_mac, dest);
	if(entry)
		return entry->best_hop;

	return NULL;
}

static void print_routing_table(batman_state* state) {
    list_item* it = state->routing_table->head;
    ygg_log("BATMAN", "INFO", "Printing routing Table");
    while(it != NULL) {
        originator_entry* omg_entry = it->data;
        char entry[200];
        bzero(entry, 200);
        char origin[37], origin_mac[33], best[37], best_mac[33];
        bzero(origin, 37);  bzero(origin_mac, 33); bzero(best, 37); bzero(best_mac, 33);
        uuid_unparse(omg_entry->node_id, origin); wlan2asc(&omg_entry->addr, origin_mac);
        if(omg_entry->best_hop != NULL) {
            uuid_unparse(omg_entry->best_hop->id, best); wlan2asc(&omg_entry->best_hop->addr, best_mac);
            stats* hop = find_hop(omg_entry->best_next_hops, omg_entry->best_hop);

            sprintf(entry, "Origin: %s - %s route: %s - %s (%d)", origin, origin_mac, best, best_mac, hop->packet_count);
        } else {
            sprintf(entry, "Origin: %s - %s no suitable route", origin, origin_mac);
        }

        ygg_log("BATMAN", "ROUTING TABLE", entry);

        it = it->next;
    }
    ygg_log("BATMAN", "INFO", "Ended");
}

static int is_in_window(unsigned short omg_seq_number, int upperbound, int lowerbound) {
	if(lowerbound > upperbound) { //window is splitted in half
		//omg seq number is in upper half of the window or omg seq number is in lower half
		return (omg_seq_number > upperbound && omg_seq_number >= lowerbound) || (omg_seq_number <= upperbound && omg_seq_number < lowerbound);
	} else { //window is normal
		return omg_seq_number <= upperbound && omg_seq_number >= lowerbound;
	}
}

static int out_of_range(batman_state* state, originator_entry* origin, unsigned short omg_seq_number, neighbour_item* trasnmiter, unsigned short omg_ttl, int window_size) {

	int seq_no = -1;
	int upperbound = origin->current_seq_number;
	int lowerbound = origin->current_seq_number - window_size +1;

	if(lowerbound < 0) { //adjust lowerbound
		lowerbound = SEQNO_MAX + lowerbound;
	}

	if(!is_in_window(omg_seq_number, upperbound, lowerbound)){ //check upper bound
		//Move window until new omg_seq_number
		seq_no = window_size -1;
		int tomove = omg_seq_number - upperbound;
		if(tomove < 0) {
			tomove = (upperbound +1 - SEQNO_MAX) + omg_seq_number +1;
		}

		if(tomove >= window_size)
			tomove = window_size;


		list_item* it = origin->best_next_hops->head;
		int new_link = 1;
		list_item* prev = NULL;
		while(it != NULL) {
			stats* entry = it->data;
			int torm = 0;
			for(int i = 0; i < tomove; i++) {
				if(entry->window[i])
					torm++;
			}

			//printf("drop %d packets for %d\n", torm, origin);
			entry->packet_count -= torm;
			if(tomove < window_size) {
				short newWindow[window_size];
				bzero(newWindow, window_size*sizeof(short));

				MEMCPY(newWindow, entry->window+tomove, (window_size -tomove)*sizeof(short));
				MEMCPY(entry->window, newWindow, window_size*sizeof(short));
			} else
				bzero(entry->window, window_size*sizeof(short));

			if(entry->neighbour_ref == trasnmiter) {
				new_link = 0;
				entry->window[seq_no] = 1;
				entry->packet_count ++;
				entry->last_ttl = omg_seq_number;
				entry->last_valid = time(NULL);
//				char str[37]; char str_2[37];  bzero(str, 37); bzero(str_2, 37);
//				uuid_unparse(origin->node_id, str); uuid_unparse(entry->neighbour_ref->id, str_2);
//				printf("updated entry: %s for origin: %s packet count: %d windown moved: %d\n", str_2, str, entry->packet_count, torm);
			}

			int purge_s = window_size*10;
			if(entry->last_valid + purge_s < time(NULL)) {
				//remove it;
                it = it->next;

				stats* torm = NULL;
				if(prev != NULL)
					torm = list_remove(origin->best_next_hops, prev);
				else
					torm = list_remove_head(origin->best_next_hops);



				torm->neighbour_ref = NULL;
				free(torm->window);
				free(torm);
			} else {
				prev = it;
				it = it->next;
			}
		}

		if(new_link) {
			stats* hop = malloc(sizeof(stats));
			hop->last_ttl = omg_ttl;
			hop->last_valid = time(NULL);
			hop->neighbour_ref = trasnmiter;
			hop->window = malloc(sizeof(short)*window_size);
			bzero(hop->window, sizeof(short)*window_size);
			hop->window[window_size -1] = 1;
			hop->packet_count = 1;
			list_add_item_to_head(origin->best_next_hops, hop);
		}
		origin->current_seq_number = omg_seq_number;

	} else {//check lower bound

		stats* hop = find_hop(origin->best_next_hops, trasnmiter);
		seq_no = omg_seq_number - lowerbound;
		if(seq_no < 0) {
			seq_no = seq_no + SEQNO_MAX;
		}

		if(seq_no >= window_size)
			printf("WARNING: seq_no %d will corrupt memory got here by: upper bound: %d lower bound: %d omg_seq_number: %d WINDOW: %d, SEQNO_MAX: %d\n", seq_no, upperbound, lowerbound, omg_seq_number, window_size, SEQNO_MAX);

		if(hop == NULL) {
			//create new entry
			hop = malloc(sizeof(stats));
			hop->last_ttl = omg_ttl;
			hop->last_valid = time(NULL);
			hop->neighbour_ref = trasnmiter;
			hop->window = malloc(sizeof(short)*window_size);
			bzero(hop->window, sizeof(short)*window_size);
			hop->window[seq_no] = 1;
			hop->packet_count = 1;
			list_add_item_to_head(origin->best_next_hops, hop);

		} else {
			//update entry
			if(hop->window[seq_no]){
				seq_no = -1;
			}
			else{
				hop->window[seq_no] = 1;
				hop->packet_count ++;
				hop->last_ttl = omg_ttl;
				hop->last_valid = time(NULL);
//                char str[37]; char str_2[37];  bzero(str, 37); bzero(str_2, 37);
//                uuid_unparse(origin->node_id, str); uuid_unparse(hop->neighbour_ref->id, str_2);
//                printf("updated entry: %s for origin: %s packet count: %d\n", str_2, str, hop->packet_count);
			}
		}

	}

	if(seq_no >= 0)
		origin->best_hop = set_best_hop(origin, state);

//	if(origin->best_hop != NULL) {
//        char str[37]; char str_2[37];  bzero(str, 37); bzero(str_2, 37);
//        uuid_unparse(origin->node_id, str); uuid_unparse(origin->best_hop->id, str_2);
//        printf("best hop: %s for origin: %s is: %s last_recorded: %d last_sent: %d\n", str_2, str, is_bidirectional(*((int*)origin->best_hop->attribute), state)?"Bi":"Uni", *((int*)origin->best_hop->attribute), state->last_seq_number_sent);
//	}

	return seq_no;
}

static int is_best_hop(originator_entry* origin, neighbour_item* trasnmiter) {
	return origin->best_hop == trasnmiter;
}



static int update_origin_stats(batman_state* state, uuid_t origin, WLANAddr* origin_addr, neighbour_item* trasnmiter, unsigned short omg_ttl, unsigned int omg_seq_number) {

	originator_entry* entry = list_find_item(state->routing_table, (equal_function) equal_entry_id, origin);

	int retrasmit = 0;

	if(entry == NULL) {
		//create new entry
		originator_entry* new_origin = malloc(sizeof(originator_entry));
		MEMCPY(new_origin->node_id, origin, sizeof(uuid_t));
		MEMCPY(new_origin->addr.data, origin_addr->data, WLAN_ADDR_LEN);
		new_origin->current_seq_number = omg_seq_number;

		new_origin->best_next_hops = list_init();

		stats* trasnmiter_stats = malloc(sizeof(stats));
		trasnmiter_stats->neighbour_ref = trasnmiter;
		trasnmiter_stats->last_ttl = omg_ttl;
		trasnmiter_stats->last_valid = time(NULL);
		trasnmiter_stats->window = malloc(sizeof(short)*state->window_size);
		bzero(trasnmiter_stats->window, sizeof(short)*state->window_size);
		trasnmiter_stats->window[state->window_size -1] = 1;
		trasnmiter_stats->packet_count = 1;

		list_add_item_to_head(new_origin->best_next_hops, trasnmiter_stats);

        new_origin->best_hop = trasnmiter;
//		if(is_bidirectional(*((int*)trasnmiter->attribute), state))
//            new_origin->best_hop = trasnmiter;
//		else
//			new_origin->best_hop = NULL;

		list_add_item_to_head(state->routing_table, new_origin);

		retrasmit = 1;

	} else if(entry->current_seq_number != omg_seq_number){
		//update entry

        int seq_out = out_of_range(state, entry, omg_seq_number, trasnmiter, omg_ttl, state->window_size);

        if(is_best_hop(entry, trasnmiter) && seq_out >= 0) {
            retrasmit = 1;
        } else if(seq_out < 0) {
            retrasmit = 0;
        }

	}

	return retrasmit;
}

static short process_msg(YggMessage* msg, batman_state* state) {

    bool retrasnmit = false;
    unsigned short* ttl_pos;
    short type;
	void* ptr = YggMessage_readPayload(msg, NULL, &type, sizeof(short));

	if(type == OGM) {
		WLANAddr* trasnmiter_addr = &msg->header.src_addr.mac_addr;
		WLANAddr origin_addr;
		uuid_t origin;
		uuid_t trasnmiter_id;
		unsigned short unidirectional_flag, direct_link_flag;
		unsigned short omg_ttl;
		int omg_seq_number;
		void* id_ptr = ptr;
		ptr = YggMessage_readPayload(msg, ptr, trasnmiter_id, sizeof(uuid_t));
		unsigned short* uni_flag_ptr = (unsigned short*) ptr;
		ptr = YggMessage_readPayload(msg, ptr, &unidirectional_flag, sizeof(unsigned short));
        unsigned short* bi_flag_ptr = (unsigned short*) ptr;
        ptr = YggMessage_readPayload(msg, ptr, &direct_link_flag, sizeof(unsigned short));
		ptr = YggMessage_readPayload(msg, ptr, origin_addr.data, WLAN_ADDR_LEN);
		ptr = YggMessage_readPayload(msg, ptr, origin, sizeof(uuid_t));


		if(uuid_compare(origin, state-> my_id) == 0){
			//drop packet and check link as biderectional

			ptr = YggMessage_readPayload(msg, ptr, &omg_ttl, sizeof(unsigned short));
			ptr = YggMessage_readPayload(msg, ptr, &omg_seq_number, sizeof(unsigned int));
			neighbour_item* trasnmiter = neighbour_find_by_addr(state->direct_links, trasnmiter_addr);
			if(omg_seq_number == state->last_seq_number_sent || direct_link_flag == 1) {
				if(trasnmiter != NULL)
					*((int*)trasnmiter->attribute) = (int) omg_seq_number;
				else {
					trasnmiter = new_neighbour(trasnmiter_id, *trasnmiter_addr,  &omg_seq_number, sizeof(int), NULL);
					neighbour_add_to_list(state->direct_links, trasnmiter);
				}

			}
		} else if(unidirectional_flag == 0) {

			ttl_pos = (unsigned short*) ptr;
			ptr = YggMessage_readPayload(msg, ptr, &omg_ttl, sizeof(unsigned short));
			ptr = YggMessage_readPayload(msg, ptr, &omg_seq_number, sizeof(unsigned int));

			neighbour_item* transmiter = neighbour_find_by_addr(state->direct_links, trasnmiter_addr);

			if(transmiter == NULL){

				int is_direct_link = -1;
                transmiter = new_neighbour(trasnmiter_id, *trasnmiter_addr, &is_direct_link, sizeof(int), NULL);
				neighbour_add_to_list(state->direct_links, transmiter);
			}

			if( *((int*)transmiter->attribute) == -1 || !is_bidirectional(*((int*)transmiter->attribute), state)) {
				*uni_flag_ptr = 1;
			}else {
			    *bi_flag_ptr = 1;
                retrasnmit = update_origin_stats(state, origin, &origin_addr, transmiter, omg_ttl, omg_seq_number);
			}

            if(uuid_compare(origin, trasnmiter_id) == 0)
                retrasnmit = true;

			if(retrasnmit && omg_ttl >= 2) {
				MEMCPY(id_ptr, state->my_id, sizeof(uuid_t));
				retrasnmit = true;
			} else
			    retrasnmit = false;

		}


	} else  { //destination is me?
	/**
	 * this is true and ok because size of pushed is > 0
	 * | (2) pushed size | (2)   ttl    | (6) src_addr   |
	 * |-------------------------------------------------|
	 * | (2) proto_origin| (6) dest_addr| (2) payload_len|
	 * |-------------------------------------------------|
	 * | (payload_len)       payload                     |
	 * */

        ttl_pos = (unsigned short*) ptr;
        unsigned short msg_ttl;
        ptr = YggMessage_readPayload(msg, ptr, &msg_ttl, sizeof(unsigned short));
        WLANAddr* src_addr = ptr;
        unsigned short* proto_origin = (unsigned short*) (ptr+WLAN_ADDR_LEN);
        ptr += sizeof(unsigned short)+WLAN_ADDR_LEN;
        WLANAddr dest_addr;
        ptr = YggMessage_readPayload(msg, ptr, dest_addr.data, WLAN_ADDR_LEN);

		if(memcmp(dest_addr.data, state->my_addr.data, WLAN_ADDR_LEN) == 0) {
			//yes
			YggMessage todeliver;
			todeliver.Proto_id = *proto_origin;
			todeliver.header = msg->header;
			todeliver.header.dst_addr.mac_addr = dest_addr;
			todeliver.header.src_addr.mac_addr = *src_addr;
			ptr = YggMessage_readPayload(msg, ptr, &todeliver.dataLen, sizeof(unsigned short));
			todeliver.data = ptr;

			deliver(&todeliver);
		} else {
//            char src_str[33], via_str[33], dst_str[33], to_send[33]; bzero(src_str, 33);  bzero(via_str, 33); bzero(dst_str, 33); bzero(to_send, 33);
//            wlan2asc(src_addr, src_str);
//            wlan2asc(&msg->header.src_addr.mac_addr, via_str);
//            wlan2asc(&dest_addr, dst_str);
//            printf("routing msg from %s via %s to %s ttl: %d", src_str, via_str, dst_str, msg_ttl);
			//no
			neighbour_item* next_hop = find_best_next_hop(state->routing_table, &dest_addr);
			if(next_hop != NULL && msg_ttl > 0) {
				MEMCPY(msg->header.dst_addr.mac_addr.data, next_hop->addr.data, WLAN_ADDR_LEN);
				retrasnmit = true;
//				wlan2asc(&msg->header.dst_addr.mac_addr, to_send);
//				printf(" next hop: %s\n", to_send);
			}
			else {

                char log_msg[200]; bzero(log_msg, 200);
                char src_str[33], via_str[33], dst_str[33]; bzero(src_str, 33); bzero(via_str, 33); bzero(dst_str, 33);
                wlan2asc(src_addr, src_str);
                wlan2asc(&msg->header.src_addr.mac_addr, via_str);
                wlan2asc(&dest_addr, dst_str);
                sprintf(log_msg, "dropping msg from %s via %s to %s ttl: %d", src_str, via_str, dst_str, msg_ttl);
                ygg_log("BATMAN", "NO ROUTE TO HOST", log_msg);
                //print_routing_table(state);
            }
		}

	}
    if(retrasnmit) {
        *ttl_pos = *ttl_pos -1;
        queue_t_elem elem = {
                .type = YGG_MESSAGE,
                .data.msg = *msg,
        };
        queue_push(state->dispatcher_queue, &elem);
    }
    YggMessage_freePayload(msg);

	return SUCCESS;
}

static short process_timer(YggTimer* timer, batman_state* state) {

	if(uuid_compare(timer->id, state->announce->id) == 0) {
		unsigned short zero = 0;

		YggMessage ogm;
		YggMessage_initBcast(&ogm, state->protoId);
		YggMessage_addPayload(&ogm, (char*) &OGM, sizeof(short));
		//add trasnmiter id
		YggMessage_addPayload(&ogm, (char*) state->my_id, sizeof(uuid_t));
		//add unidirectional flag
		YggMessage_addPayload(&ogm, (char*) &zero, sizeof(unsigned short));
        //add direct_link flag
        YggMessage_addPayload(&ogm, (char*) &zero, sizeof(unsigned short));
		//add my addr,
		YggMessage_addPayload(&ogm, (char*) state->my_addr.data, WLAN_ADDR_LEN);
		//add my originator id,
		YggMessage_addPayload(&ogm, (char*) state->my_id, sizeof(uuid_t));
		//dunno what to do with ttl...
		YggMessage_addPayload(&ogm, (char*) &state->ttl, sizeof(unsigned short));

		YggMessage_addPayload(&ogm, (char*) &state->seq_number, sizeof(unsigned int));

		state->last_seq_number_sent = state->seq_number;
		state->seq_number = (state->seq_number + 1) % SEQNO_MAX;

		queue_t_elem elem = {
		        .type = YGG_MESSAGE,
		        .data.msg = ogm,
		};
		queue_push(state->dispatcher_queue, &elem);
		YggMessage_freePayload(&ogm);

	} else
		print_routing_table(state);

	return SUCCESS;

}

static short process_event(YggEvent* event, batman_state* state) {
	//noop
	return SUCCESS;
}

static short route(YggMessage* msg, batman_state* state) {


    if(is_bcast(&msg->header.dst_addr.mac_addr, state)) {
        queue_t_elem elem = {
            .type =   YGG_MESSAGE,
            .data.msg = *msg,

        };
        queue_push(state->dispatcher_queue, &elem);
        YggMessage_freePayload(msg);
    }

//    char log_msg[200];
//    bzero(log_msg, 200);
//    char str[33];
//    bzero(str, 33);
//    wlan2asc(&msg->header.dst_addr.mac_addr, str);
//    sprintf(log_msg, "trying to route message to %s", str);
//    ygg_log("BATMAN", "REQUEST", log_msg);

    //find best next_hop
    neighbour_item* next_hop = find_best_next_hop(state->routing_table, &msg->header.dst_addr.mac_addr);
    if(next_hop == NULL) {
        char log_msg[200]; bzero(log_msg, 200);
        char dst_str[33]; bzero(dst_str, 33);
        wlan2asc(&msg->header.dst_addr.mac_addr, dst_str);
        sprintf(log_msg, "dropping msg to %s", dst_str);
        ygg_log("BATMAN", "NO ROUTE TO HOST", log_msg);
        //print_routing_table(state);
        return FAILED;
    }

    //build msg to be sent
    /**     routing_info info = {
    *           .msg_type = MSG,
    *           .ttl = state->ttl,
    *       };
    */

    void* buff = malloc(sizeof(unsigned short) + WLAN_ADDR_LEN);
        MEMCPY(buff, &state->ttl, sizeof(unsigned short));
        MEMCPY(buff+ sizeof(unsigned short), state->my_addr.data, WLAN_ADDR_LEN);

    pushPayload(msg,(char *) buff, sizeof(unsigned short)+WLAN_ADDR_LEN, state->protoId, &next_hop->addr);

    queue_t_elem elem = {
            .type = YGG_MESSAGE,
            .data.msg = *msg,
    };

    queue_push(state->dispatcher_queue, &elem);
    free(buff);
    YggMessage_freePayload(msg);

    return SUCCESS;
}

static bool set_destination_mac(YggMessage* msg, uuid_t destination, batman_state* state) {
    originator_entry* e = list_find_item(state->routing_table, (equal_function) equal_entry_id, destination);
    if(e) {
        msg->header.dst_addr.mac_addr = e->addr;
        return true;
    }
    return false;
}

static short process_request(YggRequest* request, batman_state* state) {
	if(request->request == REQUEST && request->request_type == SEND_MESSAGE) {

		YggMessage msg;
		uuid_t destination;
		unload_request_route_message(request, &msg, destination);

//        char log_msg[200];
//        bzero(log_msg, 200);
//        char u1[37];
//        uuid_unparse(destination, u1);
//        sprintf(log_msg, "trying to route message to %s", u1);
//        ygg_log("BATMAN", "REQUEST", log_msg);

		if(!set_destination_mac(&msg, destination, state)) {
            YggRequest reply;
            YggRequest_init(&reply, state->protoId, request->proto_origin, REPLY, NO_ROUTE_TO_HOST);
            deliverReply(&reply);
            ygg_log("BATMAN", "NO ROUTE TO HOST", "dropping message");
            return FAILED;
        }

		return route(&msg, state);
	}

	return FAILED;
}


static void * batman_main_loop(main_loop_args* args) {

	queue_t* inBox = args->inBox;
	batman_state* state = args->state;

	while(1) {
		queue_t_elem elem;
		queue_pop(inBox, &elem);

		switch(elem.type) {
		case YGG_MESSAGE:
		    if(elem.data.msg.Proto_id != state->protoId) { //not from me
		        //printf("routing msg\n");
		        route(&elem.data.msg, state);
		    } else {
                //printf("received announce\n");
                process_msg(&elem.data.msg, state);
            }
			break;
		case YGG_TIMER:
            if(elem.data.timer.proto_dest != state->protoId) { //not for me
                queue_push(state->dispatcher_queue, &elem);
            }else
			    process_timer(&elem.data.timer, state);
			break;
		case YGG_EVENT:
            if(elem.data.event.proto_dest != state->protoId) { //not for me
                queue_push(state->dispatcher_queue, &elem);
            }else
			    process_event(&elem.data.event, state);
			break;
		case YGG_REQUEST:
            if(elem.data.request.proto_dest != state->protoId) { //not for me
                queue_push(state->dispatcher_queue, &elem);
            }else
			    process_request(&elem.data.request, state);
			break;
		default:
			break;
		}

		free_elem_payload(&elem);
	}

	return NULL;
}

static void destroy_origin_list(list* origins) {
	while(origins->size > 0) {
		originator_entry* entry = list_remove_head(origins);
		if(entry->best_next_hops) {
			while(entry->best_next_hops->size > 0) {
				stats* hop = list_remove_head(entry->best_next_hops);
				free(hop->window);
				hop->neighbour_ref = NULL;
				free(hop);
			}
			free(entry->best_next_hops);
		}
		entry->best_hop = NULL;
		free(entry);
	}
}

static void batman_destroy(batman_state* state) {

	if(state->announce) {
		cancelTimer(state->announce);
		free(state->announce);
	}
	if(state->log_routing_table) {
		cancelTimer(state->log_routing_table);
		free(state->log_routing_table);
	}

	destroy_origin_list(state->routing_table);
	neighbour_list_destroy(state->direct_links);
	free(state->routing_table);
	free(state->direct_links);


	free(state);
}

proto_def* batman_init(void* args) {

	batman_args* bargs = (batman_args*) args;
	batman_state* state = malloc(sizeof(batman_state));

	getmyId(state->my_id);
	setMyAddr(&state->my_addr);
	setBcastAddr(&state->bcast_addr);

	state->direct_links = list_init();
	state->routing_table = list_init();
	state->seq_number = 0;
	state->ttl = bargs->ttl;
	state->window_size = bargs->window_size;

	if(bargs->standart)
		state->protoId = PROTO_ROUTING;
	else
		state->protoId = PROTO_ROUTING_BATMAN;

	state->dispatcher_queue = interceptProtocolQueue(PROTO_DISPATCH, state->protoId);

	proto_def* batman =  create_protocol_definition(state->protoId, "BATMAN", state, (Proto_destroy) batman_destroy);

	if(bargs->executor) {
		proto_def_add_msg_handler(batman, (YggMessage_handler) process_msg);
		proto_def_add_timer_handler(batman, (YggTimer_handler) process_timer);
		proto_def_add_event_handler(batman, (YggEvent_handler) process_event);
		proto_def_add_request_handler(batman, (YggRequest_handler) process_request);
	} else {
		proto_def_add_protocol_main_loop(batman, (Proto_main_loop) batman_main_loop);
	}

	state->announce = malloc(sizeof(YggTimer));
	YggTimer_init(state->announce, state->protoId, state->protoId);
	YggTimer_set(state->announce, bargs->omg_beacon_s, bargs->omg_beacon_ns, bargs->omg_beacon_s, bargs->omg_beacon_ns);

	setupTimer(state->announce);
	state->log_routing_table = NULL;
	if(bargs->log_s > 0) {
		state->log_routing_table = malloc(sizeof(YggTimer));
		YggTimer_init(state->log_routing_table, state->protoId, state->protoId);
		YggTimer_set(state->log_routing_table, bargs->log_s, 0, bargs->log_s, 0);

		setupTimer(state->log_routing_table);
	}
	return batman;
}

batman_args* batman_args_init(bool executor, bool standart, time_t omg_beacon_s, unsigned long omg_beacon_ns, unsigned short ttl, int window_size, int log_s) {
	batman_args* args = malloc(sizeof(batman_args));
	args->executor = executor;
	args->standart = standart;
	args->omg_beacon_s = omg_beacon_s;
	args->omg_beacon_ns = omg_beacon_ns;
	args->log_s = log_s;
	args->ttl = ttl;
	args->window_size = window_size;
	return args;
}
void batman_args_destroy(batman_args* args) {
	free(args);
}
