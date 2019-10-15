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

#include "cmdio.h"

#define BUFSIZE 50

int executeCommand(CONTROL_COMMAND_TREE_REQUESTS commandCode, int sock) {
	return writefully(sock, &commandCode, sizeof(int));
}

char* getResponse(int sock) {
	char* answer = NULL;
	int size;
	int r = readfully(sock, &size, sizeof(int));
	if(! (r <= 0)) {
		answer = malloc(size);
		r = readfully(sock, answer, size);
		if(r <= 0) {
			free(answer);
			answer = NULL;
		}
	}
	return answer;
}

int executeCommandWithArgument(CONTROL_COMMAND_TREE_REQUESTS commandCode, const void* command, int commandLen, int sock) {
    if(command == NULL || commandLen == 0)
        return executeCommand(commandCode, sock);

    if(writefully(sock, &commandCode, sizeof(int)) > 0 &&
       writefully(sock, &commandLen, sizeof(int)) > 0 &&
       writefully(sock, (void*) command, commandLen) > 0 ) {
        return 1;
    }
    return 0;
}

int executeCommandWithStringArgument(CONTROL_COMMAND_TREE_REQUESTS commandCode, const char* command, int sock) {
	int commandLen = strlen(command) + 1;
	return executeCommandWithArgument(commandCode, command, commandLen, sock);
}


