#ifndef RTGETNEIGH_H
#define RTGETNEIGH_H

#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/neighbour.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

void print_ndmsg(struct ndmsg *ndm);
void parse_neigh_attributes(struct ndmsg *ndm, struct rtattr *attr, int attrlen);
#endif

void print_ndmsg(struct ndmsg *ndm)
{
	printf("--------------------------------\n");
	printf("Family       : %s\n", ndm->ndm_family == AF_INET ? "IPv4" :
	       ndm->ndm_family == AF_INET6 ? "IPv6" : "Unknown");
	printf("Interface    : %d\n", ndm->ndm_ifindex);
	printf("State        : 0x%x\n", ndm->ndm_state);
	printf("Flags        : 0x%x\n", ndm->ndm_flags);
	printf("Type         : %d\n", ndm->ndm_type);
}


void parse_neigh_attributes(struct ndmsg *ndm, struct rtattr *attr, int attrlen)
{
	for (; RTA_OK(attr, attrlen); attr = RTA_NEXT(attr, attrlen)) {

		switch (attr->rta_type) {

		case NDA_DST:
			if (ndm->ndm_family == AF_INET) {
				struct in_addr *ip = RTA_DATA(attr);
				printf("Destination  : %s\n", inet_ntoa(*ip));
			}
			break;

		case NDA_LLADDR:
			unsigned char *mac = RTA_DATA(attr);

			printf("Link address : ");
			for (int i = 0; i < RTA_PAYLOAD(attr); i++)
				printf("%02x%s", mac[i], i + 1 == RTA_PAYLOAD(attr) ? "\n" : ":");
		break;

		case NDA_CACHEINFO:
			struct nda_cacheinfo *ci = RTA_DATA(attr);

			printf("Confirmed    : %u\n", ci->ndm_confirmed);
			printf("Used         : %u\n", ci->ndm_used);
			printf("Updated      : %u\n", ci->ndm_updated);
			printf("Refcnt       : %u\n", ci->ndm_refcnt);
		break;

		}
	}
}


