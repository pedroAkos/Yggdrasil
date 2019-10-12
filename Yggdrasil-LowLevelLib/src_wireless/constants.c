/*********************************************************
 * This code was written in the context of the Lightkone
 * European project.
 * Code is of the authorship of NOVA (NOVA LINCS @ DI FCT
 * NOVA University of Lisbon)
 * Authors:
 * Pedro Ákos Costa (pah.costa@campus.fct.unl.pt)
 * João Leitão (jc.leitao@fct.unl.pt)
 * (C) 2017
 *********************************************************/

#include "constants.h"

/**********************************************************
 * Constants Definition
 **********************************************************/

/**********************************************************
 * Filters
 **********************************************************/

const struct sock_filter YGG_filter[] = {
		{ 0x30, 0, 0, 0x0000000e },
		{ 0x15, 0, 5, 0x00000059 },
		{ 0x30, 0, 0, 0x0000000f },
		{ 0x15, 0, 3, 0x00000047 },
		{ 0x30, 0, 0, 0x00000010 },
		{ 0x15, 0, 1, 0x00000047 },
		{ 0x6, 0, 0, 0x00040000 },
		{ 0x6, 0, 0, 0x00000000 },
	};

const struct sock_filter LKM_filter[] = {
		{ 0x30, 0, 0, 0x0000000e },
		{ 0x15, 0, 5, 0x0000004c },
		{ 0x30, 0, 0, 0x0000000f },
		{ 0x15, 0, 3, 0x0000004b },
		{ 0x30, 0, 0, 0x00000010 },
		{ 0x15, 0, 1, 0x00000050 },
		{ 0x6, 0, 0, 0x00040000 },
		{ 0x6, 0, 0, 0x00000000 },
	};


struct sock_filter* build_filter(const char* filter) {

    if(strlen(filter) != 3) {
        fprintf(stderr, "Cannot define a filter with string: %s - Must have only 3 characters, e.g., YGG\n", filter);
        exit(1);
    }

    struct sock_filter sock_f[] = {
            { 0x30, 0, 0, 0x0000000e },
            { 0x15, 0, 5, filter[0] },
            { 0x30, 0, 0, 0x0000000f },
            { 0x15, 0, 3, filter[1] },
            { 0x30, 0, 0, 0x00000010 },
            { 0x15, 0, 1, filter[2] },
            { 0x6, 0, 0, 0x00040000 },
            { 0x6, 0, 0, 0x00000000 },
    };

    struct sock_filter* tort = malloc(sizeof(struct sock_filter)*8);
    memcpy(tort, sock_f, sizeof(struct sock_filter)*8);

    return tort;
}