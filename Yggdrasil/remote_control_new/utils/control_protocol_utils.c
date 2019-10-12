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

#include "control_protocol_utils.h"


int readfully(int fd, void* buf, int len) {
	int missing = len;
	while(missing > 0) {
		int r = read(fd, buf + len - missing, missing);
		if(r <= 0)
			return r;
		missing-=r;
	}
	return len-missing;
}

int writefully(int fd, void* buf, int len) {
	int missing = len;
	while(missing > 0) {
		int w = write(fd, buf + len - missing, missing);
		if(w <= 0)
			return w;
		missing-=w;
	}
	return len-missing;
}
