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

enum app_op{
    CONTROL,
    CMD,
};

struct pending_request{
    unsigned int op_num;
    short proto_origin; //if -1
    short operation;

    list* answers;
    void* cmd;
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
    deliverReply(&request);
    YggRequest_freePayload(&request);

}

static void process_command(unsigned short cmd, void* ptr) {
    switch (cmd) {
        case RUN:
            ygg_log_stdout("CONTROL PROCESS", "RUN cmd", (char*)ptr);
            char* bashcommand = (char*) ptr;
            pthread_t launch_child;
            pthread_create(&launch_child, NULL, (void * (*)(void *)) start_experience, (void*) bashcommand);
            pthread_detach(launch_child);
            break;
        case KILL:
            ygg_log_stdout("CONTROL PROCESS", "KILL cmd", (char*)ptr);
            stop_experience((char*) ptr);
            break;
        case SHUTDOWN:
            ygg_log_stdout("CONTROL_COMMAND_TREE", "SHUTDOWN cmd", "");
            sudo_shutdown();
            break;
        case REBOOT:
            ygg_log_stdout("CONTROL PROCESS", "REBOOT cmd", "");
            sudo_reboot();
            break;
        case LOCAL_DISABLE_DISC:
            ygg_log_stdout("CONTROL PROCESS", "DEACTIVATE_DISCOV cmd", "");
            YggRequest req; YggRequest_init(&req, myid, discov_proto, REQUEST, DEACTIVATE_DISCOV);
            deliverRequest(&req);
            break;
        case LOCAL_ENABLE_DISC:
            ygg_log_stdout("CONTROL PROCESS", "ACTIVATE_DISCOV cmd", "");
            YggRequest_init(&req, myid, discov_proto, REQUEST, ACTIVATE_DISCOV);
            deliverRequest(&req);
            break;
        default:;
            char cmd_str[4]; bzero(cmd_str, 4); sprintf(cmd_str, "%d", cmd);
            ygg_log_stdout("CONTROL PROCESS", "UNKNOWN cmd", cmd_str);
    }
}

static void store_command(unsigned int op_num, YggMessage* msg, void* read) {
    struct pending_request* p = list_find_item(pending_requests, (equal_function) equal_op, &op_num);
    if(!p) {
        p = malloc(sizeof(struct pending_request));
        p->answers = list_init();
        p->proto_origin = -1;
        p->cmd = NULL;
        list_add_item_to_tail(pending_requests, p);
    }

    p->op_num = op_num;
    read = YggMessage_readPayload(msg, read, &p->operation, sizeof(short));
    unsigned short cmdlen = msg->dataLen - (read - (void*) msg->dataLen);
    if(cmdlen > 0) {
        p->cmd = malloc(cmdlen);
        YggMessage_readPayload(msg, read, p->cmd, cmdlen);
    }

    unsigned short op = CONTROL;
    YggRequest req;
    YggRequest_init(&req, myid, dissemination_proto, REQUEST, DISSEMINATION_REQUEST);
    YggRequest_addPayload(&req, &op, sizeof(unsigned short));
    YggRequest_addPayload(&req, &op_num, sizeof(unsigned int));
    YggRequest_addPayload(&req, hostname, hostname_size);

    printf("Received command: %d op_num %d sending conf: %s\n", p->operation, op_num, hostname);

    deliverRequest(&req);
    YggRequest_freePayload(&req);

}

static void can_execute(unsigned int op_num, YggMessage* msg, void* read) {

    struct pending_request* p = list_find_item(pending_requests, (equal_function) equal_op, &op_num);
    if(!p) {
        p = malloc(sizeof(struct pending_request));
        p->answers = list_init();
        p->proto_origin = -1;
        p->op_num = op_num;
        p->cmd = NULL;
        list_add_item_to_tail(pending_requests, p);
    }

    int otherhost_size = msg->dataLen - (read - (void*) msg->data);
    char* otherhost = malloc(otherhost_size); YggMessage_readPayload(msg, read, otherhost, otherhost_size);
    printf("%s\n", otherhost);
    if(!list_find_item(p->answers, (equal_function) equal_answer, otherhost)) {
        list_add_item_to_tail(p->answers, otherhost);
    } else
        free(otherhost);

    if(p->answers->size == can_exec_requirement) {
        process_command(p->operation, p->cmd);
        if(p->proto_origin != -1)
            reply_to_client(p);
    }
}

void process_disseminated_cmd(YggMessage* msg) {
    unsigned short op; unsigned int op_num;
    void* ptr = YggMessage_readPayload(msg, NULL, &op, sizeof(unsigned short));
    ptr = YggMessage_readPayload(msg, ptr, &op_num, sizeof(unsigned int));
    printf("received app_op %s for op num %d\n", op==CMD ? "CMD" : "CONTROL", op_num);
    if(op == CMD) {
        curr_op = op_num;
        store_command(op_num, msg, ptr);
    } else {
        can_execute(op_num, msg, ptr);
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
    p->answers = list_init();
    list_add_item_to_tail(pending_requests, p);

    unsigned short op = CMD;
    YggRequest req;
    YggRequest_init(&req, myid, dissemination_proto, REQUEST, DISSEMINATION_REQUEST);
    YggRequest_addPayload(&req, &op, sizeof(unsigned short));
    YggRequest_addPayload(&req, &curr_op, sizeof(unsigned int));
    YggRequest_addPayload(&req, &request->request_type, sizeof(short));
    if(request->payload != NULL && request->length > 0) {
        YggRequest_addPayload(&req, request->payload, request->length);
        p->cmd = malloc(request->length);
        memcpy(p->cmd, request->payload, request->length);
        YggRequest_freePayload(request);
    }


    deliverRequest(&req);
    YggRequest_freePayload(&req);
}


int main(int argc, char* argv[]) {

	char* type = "AdHoc"; //should be an argument
	NetworkConfig* ntconf = defineWirelessNetworkConfig(type, 11, 5, 1, "pis", "CTL");

	pending_requests = list_init();

	//Init ygg_runtime and protocols
	ygg_runtime_init(ntconf);

	can_exec_requirement = 21; //TODO make param
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
            default:
                //nothing to do probably
                break;
        }


	}

	return 0;
}

