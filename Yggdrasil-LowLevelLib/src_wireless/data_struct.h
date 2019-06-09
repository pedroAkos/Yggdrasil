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

#ifndef YGG_LL_DATA_STRUCT_H_
#define YGG_LL_DATA_STRUCT_H_

#include <netlink/genl/genl.h>
#include <linux/nl80211.h>
#include <linux/filter.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <arpa/inet.h>

#include "../src/data_struct.h"
#include "utils.h"
#include "errors.h"

#define YGG_HEADER_LEN 3

#define DEFAULT_MTU 1500

#define CH_BROAD 0X001
#define CH_SENDTO 0x002
#define CH_SEND 0x003

#define IP_TYPE 0x3901
#define WLAN_BROADCAST "ff:ff:ff:ff:ff:ff"

#define AF_LK 0x4C4B50 //LKP
#define AF_LK_ARRAY {0x4C, 0x4B, 0x50}

#define AF_YGG 0x594747 //YGG
#define AF_YGG_ARRAY {0x59, 0x47, 0x47}

#pragma pack(1)
// Structure of a frame header
typedef struct _WLANHeader{
    // destination address
    WLANAddr destAddr;
    // source address
    WLANAddr srcAddr;
    // type
    unsigned short type;
} WLANHeader;

#define WLAN_HEADER_LEN sizeof(struct _WLANHeader)

// Structure of an Yggdrasil message
typedef struct _YGGHeader{
   // Protocol family identifation
   unsigned char data[YGG_HEADER_LEN];
} YggHeader;

#define MAX_PAYLOAD DEFAULT_MTU -WLAN_HEADER_LEN -YGG_HEADER_LEN -sizeof(unsigned short)

// Yggdrasil physical layer message
typedef struct _YggPhyMessage{
  //Physical Level header;
  WLANHeader phyHeader;
  //Lightkone Protocol header;
  YggHeader yggHeader;
  //PayloadLen
  uint16_t dataLen;
  //Payload
  char data[MAX_PAYLOAD];
} YggPhyMessage;

#pragma pack()

typedef struct _phy {
  int id;
  short modes[NUM_NL80211_IFTYPES];
  struct _phy* next;
} Phy;

typedef struct _Mesh {
	char pathSelection;
	char pathSelectionMetric;
	char congestionControl;
	char syncMethod;
	char authProtocol;
} Mesh;

typedef struct _Network {
	char* ssid;
	char mac_addr[20];
	char isIBSS;
	Mesh* mesh_info;
	int freq;
	int connected;
	struct _Network* next;
} Network;

/*************************************************
 * Error codes (moved to lk_errors.h)
 *************************************************/
//#define NO_IF_INDEX_ERR 1
//#define NO_IF_ADDR_ERR 2
//#define NO_IF_MTU_ERR 3

/*************************************************
 * Global Vars
 *************************************************/
Network* knownNetworks;

/*************************************************
 * Data structure manipulation
 *************************************************/
NetworkConfig* defineWirelessNetworkConfig(char* type, int freq, int nscan, short mandatory, char* name, const struct sock_filter* filter);

/*************************************************
 * Phy
 *************************************************/
Phy* alloc_phy();
Phy* free_phy(Phy* var);
void free_all_phy(Phy* var);
int phy_count(Phy* var);
Phy* clone_phy(Phy* var);

/*************************************************
 * Interface
 *************************************************/
Interface* alloc_interface();
Interface* alloc_named_interface(char* name);
Interface* free_interface(Interface* var);
void free_all_interfaces(Interface* var);

/*************************************************
 * Network
 *************************************************/
Network* alloc_network(char* netName);
Network* free_network(Network* net);

void fillMeshInformation(Mesh* mesh_info, unsigned char *ie, int ielen);

/*************************************************
 * YggPhyMessage
 *************************************************/
int initYggPhyMessage(YggPhyMessage *msg); //initializes an empty payload message
int initYggPhyMessageWithPayload(YggPhyMessage *msg, char* buffer, short bufferlen); //initializes a non empty payload message

int addPayload(YggPhyMessage *msg, char* buffer);
int deserializeYggPhyMessage(YggPhyMessage *msg, unsigned short msglen, void* buffer, int bufferLen);


/*************************************************
 * Channel
 *************************************************/
int createChannel(Channel* ch, char* interface);
int bindChannel(Channel* ch);

#endif /* YGG_LL_DATA_STRUCT_H_ */
