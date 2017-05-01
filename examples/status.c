/*
 * Copyright (c) 2017, Spectra Logic Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * thislist of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>

#include <err.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libifconfig.h>

static void
print_inet4_addr(ifconfig_handle_t *lifh, struct ifaddrs *ifa)
{
	char addr_buf[NI_MAXHOST];
	struct sockaddr_in *sin, *dst, null_sin, *mask, *bcast;

	memset(&null_sin, 0, sizeof(null_sin));
	sin = (struct sockaddr_in*)ifa->ifa_addr;
	if (sin == NULL)
		return;
	inet_ntop(AF_INET, &sin->sin_addr, addr_buf, sizeof(addr_buf));
	printf("\tinet %s", addr_buf);
	if (ifa->ifa_flags & IFF_POINTOPOINT) {
		dst = (struct sockaddr_in*)ifa->ifa_dstaddr;
		if (dst == NULL)
			dst = &null_sin;
		printf(" --> %s", inet_ntoa(sin->sin_addr));
	}
	mask = (struct sockaddr_in*)ifa->ifa_netmask;
	if (mask == NULL)
		mask = &null_sin;
	printf(" netmask 0x%lx ", (unsigned long)ntohl(mask->sin_addr.s_addr));
	if (ifa->ifa_flags & IFF_BROADCAST) {
		bcast = (struct sockaddr_in*)ifa->ifa_broadaddr;
		if (bcast != NULL && bcast->sin_addr.s_addr != 0)
			printf("broadcast %s ", inet_ntoa(bcast->sin_addr));
	}
	/* TODO: print vhid*/
	printf("\n");
}

static int inet6_prefixlen(struct in6_addr *addr)
{
	int prefixlen = 0;
	int i;

	for (i=0; i < 4; i++) {
		int x = ffs(ntohl(addr->__u6_addr.__u6_addr32[i]));
		prefixlen += x == 0 ? 0 : 33 - x;
		if (x != 1)
			break;
	}
	return (prefixlen);
}

static void
print_inet6_addr(ifconfig_handle_t *lifh, struct ifaddrs *ifa)
{
	char addr_buf[NI_MAXHOST];
	struct sockaddr_in6 *sin6, *netmask, null_sin6;

	memset(&null_sin6, 0, sizeof(null_sin6));
	sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
	if (sin6 == NULL)
		return;

	if (0 != getnameinfo((struct sockaddr*)sin6, sin6->sin6_len, addr_buf,
	    sizeof(addr_buf), NULL, 0, NI_NUMERICHOST))
		inet_ntop(AF_INET6, &sin6->sin6_addr, addr_buf,
		    sizeof(addr_buf));
	printf("\tinet6 %s", addr_buf);

	if (ifa->ifa_flags & IFF_POINTOPOINT) {
		if (sin6 != NULL && sin6->sin6_family == AF_INET6) {
			inet_ntop(AF_INET6, &sin6->sin6_addr, addr_buf,
			    sizeof(addr_buf));
			printf(" --> %s", addr_buf);
		}
	}

	netmask = (struct sockaddr_in6 *)ifa->ifa_netmask;
	if (netmask == NULL)
		netmask = &null_sin6;
	printf(" prefixlen %d ", inet6_prefixlen(&netmask->sin6_addr));

	if (sin6->sin6_scope_id)
		printf("scopeid 0x%x ", sin6->sin6_scope_id);

	// TODO: print vhid
	printf("\n");
}

static void
print_link_addr(ifconfig_handle_t *lifh, struct ifaddrs *ifa)
{
	char addr_buf[NI_MAXHOST];
	struct sockaddr_dl *sdl;
	int n;

	sdl = (struct sockaddr_dl*) ifa->ifa_addr;
	if (sdl != NULL && sdl->sdl_alen > 0) {
		if ((sdl->sdl_type == IFT_ETHER ||
		    sdl->sdl_type == IFT_L2VLAN ||
		    sdl->sdl_type == IFT_BRIDGE) &&
		    sdl->sdl_alen == ETHER_ADDR_LEN) {
			ether_ntoa_r((struct ether_addr *)LLADDR(sdl), addr_buf);
			printf("\tether %s\n", addr_buf);
		} else {
			n = sdl->sdl_nlen > 0 ? sdl->sdl_nlen + 1 : 0;

			printf("\tlladdr %s\n", link_ntoa(sdl) + n);
		}
	}
}

static void
print_ifaddr(ifconfig_handle_t *lifh, struct ifaddrs *ifa)
{
	switch (ifa->ifa_addr->sa_family) {
	case AF_INET:
		print_inet4_addr(lifh, ifa);
		break;
	case AF_INET6:
		/*
		 * printing AF_INET6 status requires calling SIOCGIFAFLAG_IN6
		 * and SIOCGIFALIFETIME_IN6.  TODO: figure out the best way to
		 * do that from within libifconfig
		 */
		print_inet6_addr(lifh, ifa);
		break;
	case AF_LINK:
		print_link_addr(lifh, ifa);
		break;
	case AF_LOCAL:
	case AF_UNSPEC:
	default:
		/* TODO */
		break;
	}
}

static void
print_nd6(ifconfig_handle_t *lifh, struct ifaddrs *ifa)
{
	struct in6_ndireq nd;

	if (ifconfig_get_nd6(lifh, ifa->ifa_name, &nd) == 0) {
		printf("\tnd6 options=%x\n", nd.ndi.flags);
	} else
		err(1, "Failed to get nd6 options");
}

static void
print_fib(ifconfig_handle_t *lifh, struct ifaddrs *ifa)
{
	int fib;

	if (ifconfig_get_fib(lifh, ifa->ifa_name, &fib) == 0) {
		printf("\tfib: %d\n", fib);
	} else
		err(1, "Failed to get interface FIB");
}

static void
print_groups(ifconfig_handle_t *lifh, struct ifaddrs *ifa)
{
	struct ifgroupreq ifgr;
	struct ifg_req *ifg;
	int len;
	int cnt = 0;

	if (ifconfig_get_groups(lifh, ifa->ifa_name, &ifgr) != 0)
		err(1, "Failed to get groups");

	ifg = ifgr.ifgr_groups;
	len = ifgr.ifgr_len;
	for (; ifg && len >= sizeof(struct ifg_req); ifg++) {
		len -= sizeof(struct ifg_req);
		if (strcmp(ifg->ifgrq_group, "all")) {
			if (cnt == 0)
				printf("\tgroups: ");
			cnt++;
			printf("%s ", ifg->ifgrq_group);
		}
	}
	if (cnt)
		printf("\n");

	free(ifgr.ifgr_groups);
}

static void
print_media(ifconfig_handle_t *lifh, struct ifaddrs *ifa)
{
	int i;

	/* Outline:
	 * 1) Determine whether the iface supports SIOGIFMEDIA or SIOGIFXMEDIA
	 * 2) Get the full media list
	 * 3) Print the current media word
	 * 4) Print the active media word, if different
	 * 5) Print the status
	 * 6) Print the supported media list
	 *
	 * How to print the media word:
	 * 1) Get the top-level interface type and description
	 * 2) Print the subtype
	 * 3) For current word only, print the top type, if it exists
	 * 4) Print options list
	 * 5) Print the instance, if there is one
	 *
	 * How to get the top-level interface type
	 * 1) Shift ifmw right by 0x20 and index into IFM_TYPE_DESCRIPTIONS
	 *
	 * How to get the top-level interface subtype
	 * 1) Shift ifmw right by 0x20, index into ifmedia_types_to_subtypes
	 * 2) Iterate through the resulting table's subtypes table, ignoring
	 *    aliases.  Iterate through the resulting ifmedia_description
	 *    tables,  finding an entry with the right media subtype
	 */
	struct ifmediareq ifmr;
	char opts[80];

	if (ifconfig_get_media(lifh, ifa->ifa_name, &ifmr) != 0) {
		if (ifconfig_err_errtype(lifh) != OK)
			err(1, "Failed to get media info");
		else
			return;	/* Interface doesn't support media info */
	}

	printf("\tmedia: %s %s", ifconfig_get_media_type(ifmr.ifm_current),
	    ifconfig_get_media_subtype(ifmr.ifm_current));
	if (ifmr.ifm_active != ifmr.ifm_current) {
		printf(" (%s", ifconfig_get_media_subtype(ifmr.ifm_active));
		ifconfig_get_media_options_string(ifmr.ifm_active, opts,
		    sizeof(opts));
		if (opts[0] != '\0')
			printf(" <%s>)\n", opts);
		else
			printf(")\n");
	} else
		printf("\n");

	if (ifmr.ifm_status & IFM_AVALID) {
		printf("\tstatus: %s\n",
		    ifconfig_get_media_status(&ifmr));
	}

	printf("\tsupported media:\n");
	for (i=0; i < ifmr.ifm_count; i++) {
		printf("\t\tmedia %s",
		    ifconfig_get_media_subtype(ifmr.ifm_ulist[i]));
		ifconfig_get_media_options_string(ifmr.ifm_ulist[i], opts,
		    sizeof(opts));
		if (opts[0] != '\0')
			printf(" mediaopt %s\n", opts);
		else
			printf("\n");
	}
}

static void
print_iface(ifconfig_handle_t *lifh, struct ifaddrs *ifa)
{
	int metric, mtu;
	char *description = NULL;
	struct ifconfig_capabilities caps;
	struct ifstat ifs;

	printf("%s: flags=%x ", ifa->ifa_name, ifa->ifa_flags);

	if (ifconfig_get_metric(lifh, ifa->ifa_name, &metric) == 0)
		printf("metric %d ", metric);
	else
		err(1, "Failed to get interface metric");

	if (ifconfig_get_mtu(lifh, ifa->ifa_name, &mtu) == 0)
		printf("mtu %d\n", mtu);
	else
		err(1, "Failed to get interface MTU");

	if (ifconfig_get_description(lifh, ifa->ifa_name, &description) == 0)
		printf("\tdescription: %s\n", description);

	if (ifconfig_get_capability(lifh, ifa->ifa_name, &caps) == 0) {
		if (caps.curcap != 0)
			printf("\toptions=%x\n", caps.curcap);
		if (caps.reqcap != 0)
			printf("\tcapabilities=%x\n", caps.reqcap);
	} else
		err(1, "Failed to get interface capabilities");

	ifconfig_foreach_ifaddr(lifh, ifa, print_ifaddr);

	/* This paragraph is equivalent to ifconfig's af_other_status funcs */
	print_nd6(lifh, ifa);
	print_media(lifh, ifa);
	print_groups(lifh, ifa);
	print_fib(lifh, ifa);

	if (ifconfig_get_ifstatus(lifh, ifa->ifa_name, &ifs) == 0)
		printf("%s", ifs.ascii);

	free(description);
}

int
main(int argc, char *argv[])
{
	ifconfig_handle_t *lifh;

	if (argc != 1)
		errx(1, "Usage: example_status");

	lifh = ifconfig_open();
	if (lifh == NULL)
		errx(1, "Failed to open libifconfig handle.");

	if (ifconfig_foreach_iface(lifh, print_iface) != 0)
		err(1, "Failed to get interfaces");

	ifconfig_close(lifh);
	lifh = NULL;
	return (-1);
}
