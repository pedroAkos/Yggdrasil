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

#include "ygg_runtime.h"
#include "data_structures/generic/list.h"
#include "cmd_server.h"
#include "remote_control_new/utils/commands.h"

#include "remote_control_new/protocols/discovery.h"
#include "remote_control_new/protocols/reliable_dissemination.h"

#define OP_NUM_MAX 50000

#define DEFAULT_TIMEOUT 60 //60s - 1min

enum app_op{
    CMD_WITH_AGREEMENT,
    AGREEMENT,
    CMD,
    CONTROL,
};

struct pending_request{
    unsigned int op_num;
    short proto_origin; //if -1
    short operation;

    list* answers;
    void* cmd;
    unsigned short cmdlen;
    //YggTimer* timeout;
};

static unsigned int curr_op = 0;

static short dissemination_proto = 333;
static short discov_proto = 332;
static short myid = 400;

static short can_exec_requirement;

list* pending_requests;

static char* hostname;
static int hostname_size;

static bool equal_op(struct pending_request* req, const unsigned int* op) {
    return req->op_num ==  *op;
}

static bool equal_answer(char* host, char* host2) {
    return strcmp(host, host2) == 0;
}

static char* op_to_str(unsigned short op) {
    switch (op) {
        case CMD:
           return "CMD";
        case CMD_WITH_AGREEMENT:
           return "CMD_WITH_AGREEMENT";
        case AGREEMENT:
            return "AGREEMENT";
        case CONTROL:
            return "CONTROL";
        default:
            return "UNKNOWN";
    }
}

static char* to_string(unsigned int op_count, unsigned short cmd_num, unsigned short op, char* cmd_payload, unsigned short cmd_payload_len) {
    char* cmd;
    char* payload = cmd_payload;
    switch (cmd_num) {
        case SETUP:
            cmd = "SETUP";
            char str[50]; bzero(str, 50);
            sprintf(str, "%d", *((int*)cmd_payload));
            break;
        case IS_UP:
            cmd = "IS_UP";
            break;
        case KILL:
            cmd = "KILL";
            break;
        case RUN:
            cmd = "RUN";
            break;
        case REMOTE_CHANGE_LINK:
            cmd = "REMOTE_CHANGE_LINK";
            break;
        case REMOTE_CHANGE_VAL:
            cmd = "REMOTE_CHANGE_VAL";
            break;
        case REBOOT:
            cmd = "REBOOT";
            break;
        case LOCAL_ENABLE_DISC:
            cmd = "LOCAL_ENABLE_DISC";
            break;
        case LOCAL_DISABLE_DISC:
            cmd = "LOCAL_DISABLE_DISC";
            break;
        case SHUTDOWN:
            cmd = "SHUTDOWN";
            break;
        case GET_NEIGHBORS:
            cmd = "GET_NEIGHBORS";
            break;
        case DEBUG_NEIGHBOR_TABLE:
            cmd = "DEBUG_NEIGHBOR_TABLE";
            break;
        default:
            cmd = "UNKNOWN";
            break;
    }
    char* op_str = op_to_str(op);
    int op_str_len = strlen(op_str);
    int cmd_len = strlen(cmd);
    int payload_len = 0;
    if(!payload || cmd_payload_len == 0) {
        payload = "";
    } else
        payload_len = strlen(payload);

    int ret_len = op_str_len + cmd_len + payload_len + 10;
    char* ret = malloc(ret_len);
    bzero(ret, ret_len);
    sprintf(ret, "%d: %s %s %s", op_count, op_str, cmd, payload);
    return ret;
}

static char* compute_response(struct pending_request* req) {
    int size = 0;
    for(list_item* it = req->answers->head; it != NULL; it = it->next) {
        size += strlen((char*) it->data)+1;
    }
    char response[size];
    char* r = malloc(size + 100); bzero(r, 100);
    char* p = response;
    for(list_item* it = req->answers->head; it != NULL; it = it->next) {
        size = strlen((char*) it->data);
        memcpy(p, it->data, size);
        p +=size; *p = '\n'; p++;
    }
    p --; *p='\0';

    sprintf(r, "Total nodes: %d\n%s\n", req->answers->size, response);
    return r;
}

static void reply_to_client(struct pending_request* req) {

    YggRequest request;
    YggRequest_init(&request, myid, req->proto_origin, REPLY, req->operation);
    request.payload = compute_response(req);
    request.length = strlen(request.payload)+1;

    ygg_log_stdout("YGGDRASIL CONTROL", "REPLY", (char*) request.payload);
    deliverReply(&request);
    YggRequest_freePayload(&request);

}

static void set_timeout(struct pending_request* p, int timeout) {
    YggTimer timer;
    YggTimer_init(&timer, myid, myid);
    YggTimer_set(&timer, timeout, 0,0,0);
    YggTimer_addPayload(&timer, &p->op_num, sizeof(unsigned int));
    setupTimer(&timer);

    //printf("set timeout for op: %d to %d\n", p->op_num, timeout);
    YggTimer_freePayload(&timer);
}

static unsigned short get_cmd_type(YggRequest* req, struct pending_request* p) {
    int timeout = DEFAULT_TIMEOUT;
    switch (req->request_type) {
        case SETUP:
            if(req->payload && req->length > sizeof(int)) {
                YggRequest_readPayload(req, req->payload+ sizeof(int), &timeout, sizeof(int));
                req->length -= sizeof(int);
            }
            set_timeout(p, timeout);
            return CMD;
        case IS_UP:
            if(req->payload) {
                YggRequest_readPayload(req, NULL, &timeout, sizeof(int));
                YggRequest_freePayload(req);
            }
            set_timeout(p, timeout);
            return CMD;
        case KILL:
            return CMD_WITH_AGREEMENT;
        case RUN:
            return CMD_WITH_AGREEMENT;
        case REMOTE_CHANGE_LINK:
            return CMD_WITH_AGREEMENT;
        case REMOTE_CHANGE_VAL:
            return CMD_WITH_AGREEMENT;
        case REBOOT:
            return CMD_WITH_AGREEMENT;
        case LOCAL_ENABLE_DISC:
            return CMD_WITH_AGREEMENT;
        case LOCAL_DISABLE_DISC:
            return CMD_WITH_AGREEMENT;
        case SHUTDOWN:
            return CMD_WITH_AGREEMENT;
        case GET_NEIGHBORS:
            set_timeout(p, timeout);
            return CMD;
        case DEBUG_NEIGHBOR_TABLE:
            set_timeout(p, timeout);
            return CMD;
        default:
            return -1;
    }
}

static void process_command(unsigned short cmd, void* ptr) {
    YggRequest req; pthread_t launch_child;
    switch (cmd) {
        case SETUP:
            can_exec_requirement = *((int*) ptr);
            break;
        case IS_UP:
            break;
        case RUN:
            pthread_create(&launch_child, NULL, (void * (*)(void *)) start_experience, ptr);
            pthread_detach(launch_child);
            break;
        case KILL:
            stop_experience((char*) ptr);
            break;
        case SHUTDOWN:
            sudo_shutdown();
            break;
        case REBOOT:
            sudo_reboot();
            break;
        case LOCAL_DISABLE_DISC:
            YggRequest_init(&req, myid, discov_proto, REQUEST, DEACTIVATE_DISCOV);
            deliverRequest(&req);
            break;
        case LOCAL_ENABLE_DISC:
            YggRequest_init(&req, myid, discov_proto, REQUEST, ACTIVATE_DISCOV);
            deliverRequest(&req);
            break;
        default:
            break;
    }
}

static void send_control_msg(unsigned short op, unsigned int op_num) {
    YggRequest req;
    YggRequest_init(&req, myid, dissemination_proto, REQUEST, DISSEMINATION_REQUEST);
    YggRequest_addPayload(&req, &op, sizeof(unsigned short));
    YggRequest_addPayload(&req, &op_num, sizeof(unsigned int));
    YggRequest_addPayload(&req, hostname, hostname_size);

    deliverRequest(&req);
    YggRequest_freePayload(&req);
}

static void store_command(unsigned int op_num, YggMessage* msg, void* read) {
    struct pending_request* p = list_find_item(pending_requests, (equal_function) equal_op, &op_num);
    if(!p) {
        p = malloc(sizeof(struct pending_request));
        p->answers = list_init();
        p->proto_origin = -1;
        p->cmd = NULL;
        p->cmdlen = 0;
        list_add_item_to_tail(pending_requests, p);
    }

    p->op_num = op_num;
    read = YggMessage_readPayload(msg, read, &p->operation, sizeof(short));
    p->cmdlen = msg->dataLen - (read - (void*) msg->data);
    if(p->cmdlen > 0) {
        p->cmd = malloc(p->cmdlen);
        YggMessage_readPayload(msg, read, p->cmd, p->cmdlen);
    }

    char* str = to_string(p->op_num, p->operation, CMD_WITH_AGREEMENT, p->cmd, p->cmdlen);
    ygg_log_stdout("YGGDRASIL CONTROL", "PENDING", str);
    free(str);
    send_control_msg(AGREEMENT, op_num);

}

static void can_execute(unsigned int op_num, YggMessage* msg, void* read) {

    struct pending_request* p = list_find_item(pending_requests, (equal_function) equal_op, &op_num);
    if(!p) {
        p = malloc(sizeof(struct pending_request));
        p->answers = list_init();
        p->proto_origin = -1;
        p->op_num = op_num;
        p->cmd = NULL;
        p->cmdlen = 0;
        list_add_item_to_tail(pending_requests, p);
    }

    int otherhost_size = msg->dataLen - (read - (void*) msg->data);
    char* otherhost = malloc(otherhost_size); YggMessage_readPayload(msg, read, otherhost, otherhost_size);

    char str[otherhost_size + 10]; bzero(str, otherhost_size+10);
    sprintf(str, "%d: %s", p->op_num, otherhost);
    ygg_log_stdout("YGGDRASIL CONTROL", "ANSWER", str);


    if(!list_find_item(p->answers, (equal_function) equal_answer, otherhost)) {
        list_add_item_to_tail(p->answers, otherhost);
    } else
        free(otherhost);

    if(p->answers->size >= can_exec_requirement) {
        char* str2 = to_string(p->op_num, p->operation, CMD_WITH_AGREEMENT, p->cmd, p->cmdlen);
        ygg_log_stdout("YGGDRASIL CONTROL", "EXECUTING", str2);
        free(str2);
        process_command(p->operation, p->cmd);
        if(p->proto_origin != -1)
            reply_to_client(p);
    }
}

void execute(unsigned int op_num, YggMessage* msg, void* read) {
    void* cmd = NULL; short operation;
    read = YggMessage_readPayload(msg, read, &operation, sizeof(short));
    unsigned short cmdlen = msg->dataLen - (read - (void*) msg->data);
    if(cmdlen > 0) {
        cmd = malloc(cmdlen);
        YggMessage_readPayload(msg, read, cmd, cmdlen);
    }

    char* str2 = to_string(op_num, operation, CMD, cmd, cmdlen);
    ygg_log_stdout("YGGDRASIL CONTROL", "EXECUTING", str2);
    free(str2);
    process_command(operation, cmd);
    send_control_msg(CONTROL, op_num);
    free(cmd);
}

void collect(unsigned int op_num, YggMessage* msg, void* read) {
    struct pending_request* p = list_find_item(pending_requests, (equal_function) equal_op, &op_num);
    if(p) {
        int otherhost_size = msg->dataLen - (read - (void*) msg->data);
        char* otherhost = malloc(otherhost_size); YggMessage_readPayload(msg, read, otherhost, otherhost_size);

        char str[otherhost_size + 10]; bzero(str, otherhost_size+10);
        sprintf(str, "%d: %s", p->op_num, otherhost);
        ygg_log_stdout("YGGDRASIL CONTROL", "COLLECT", str);

        if(!list_find_item(p->answers, (equal_function) equal_answer, otherhost)) {
            list_add_item_to_tail(p->answers, otherhost);
        } else
            free(otherhost);
    }//else ignore
}

void process_disseminated_cmd(YggMessage* msg) {
    unsigned short op; unsigned int op_num;
    void* ptr = YggMessage_readPayload(msg, NULL, &op, sizeof(unsigned short));
    ptr = YggMessage_readPayload(msg, ptr, &op_num, sizeof(unsigned int));
    //printf("received app_op %s for op num %d\n", op==CMD ? "CMD" : "CONTROL", op_num);
    if(op == CMD_WITH_AGREEMENT) {
        curr_op = op_num;
        store_command(op_num, msg, ptr);
    }else if(op == AGREEMENT) {
        can_execute(op_num, msg, ptr);
    } else if(op == CMD) {
        curr_op = op_num;
        execute(op_num, msg, ptr);
    } else if (op == CONTROL) {
        collect(op_num, msg, ptr);
    }
    YggMessage_freePayload(msg);
}


void process_dissemination_req(YggRequest* request) {

    curr_op = (curr_op+1)%OP_NUM_MAX;

    struct pending_request* p = malloc(sizeof(struct pending_request));
    p->proto_origin = request->proto_origin;
    p->operation = request->request_type;
    p->op_num = curr_op;
    p->cmd = NULL;
    p->cmdlen = 0;
    p->answers = list_init();
    list_add_item_to_tail(pending_requests, p);

    unsigned short op = get_cmd_type(request, p);

    char* str = to_string(p->op_num, request->request_type, op, request->payload, request->length);
    ygg_log_stdout("YGGDRASIL CONTROL", "REQUEST", str);
    free(str);

    YggRequest req;
    YggRequest_init(&req, myid, dissemination_proto, REQUEST, DISSEMINATION_REQUEST);
    YggRequest_addPayload(&req, &op, sizeof(unsigned short));
    YggRequest_addPayload(&req, &curr_op, sizeof(unsigned int));
    YggRequest_addPayload(&req, &request->request_type, sizeof(short));
    if(request->payload != NULL && request->length > 0) {
        YggRequest_addPayload(&req, request->payload, request->length);
        p->cmd = malloc(request->length);
        p->cmdlen = request->length;
        memcpy(p->cmd, request->payload, request->length);
        YggRequest_freePayload(request);
    }

    deliverRequest(&req);
    YggRequest_freePayload(&req);
}


static void process_timeout(YggTimer* t) {
    unsigned int op_num; YggTimer_readPayload(t, NULL, &op_num, sizeof(unsigned int));

    struct pending_request* p = list_find_item(pending_requests, (equal_function) equal_op, &op_num);
    if(p) {
        reply_to_client(p);
    }
    YggTimer_freePayload(t);
}


int main(int argc, char* argv[]) {

    char* configFile = argv[1];
    config_t* cf = read_config_file(configFile);
    NetworkConfig* ntconf = read_network_properties(cf);
    //char* type = "AdHoc"; //should be an argument
	//NetworkConfig* ntconf = defineWirelessNetworkConfig(type, 11, 5, 1, "pis", "CTL");

	pending_requests = list_init();

	//Init ygg_runtime and protocols
	ygg_runtime_init(ntconf);

	can_exec_requirement = 0; //TODO make param
	const char* lhostname = getHostname();
	hostname_size = strlen(lhostname)+1;
	hostname = malloc(hostname_size);
	memcpy(hostname, lhostname, hostname_size);
	hostname[hostname_size] = '\0';

	discovery_args discoveryArgs = {
	        .proto_id = discov_proto,
	        .announce_period_s = 2,
	        .announce_period_ns = 0
	};
	registerProtocol(discoveryArgs.proto_id, (Proto_init) discovery_init, &discoveryArgs);
	reliable_dissemination_args reliableDisseminationArgs = {
	        .proto_id = dissemination_proto,
	        .discov_proto_id = discoveryArgs.proto_id,
	        .expiration = 1000,
	        .timeout_s = 0,
	        .timeout_ns = 500*1000*1000
	};
	registerProtocol(dissemination_proto, (Proto_init) reliable_dissemination_init, &reliableDisseminationArgs);

	app_def* app = create_application_definition(myid, "Yggdrasil Wireless Control");
    queue_t* inBox = registerApp(app);

    app_def* cmd_server = create_application_definition(401, "Control Command Server");
    queue_t* cmd_inBox = registerApp(cmd_server);
	//Start ygg_runtime
	ygg_runtime_start();

	pthread_t cmd_server_thread;
	pthread_create(&cmd_server_thread, NULL, (void *(*) (void*)) cmd_server_init, (void*) cmd_inBox );

	while(1) {
        queue_t_elem elem;
        queue_pop(inBox, &elem);

        switch(elem.type) {
            case YGG_MESSAGE:
                process_disseminated_cmd(&elem.data.msg);
                break;
            case YGG_REQUEST:
                process_dissemination_req(&elem.data.request);
                break;
            case YGG_TIMER:
                process_timeout(&elem.data.timer);
            default:
                //nothing to do probably
                break;
        }


	}

	return 0;
}

