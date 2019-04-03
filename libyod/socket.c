#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <nb30.h>
#else
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <sys/ioctl.h>
	#include <arpa/inet.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <net/if.h>
	#include <unistd.h>
	#include <netdb.h>
	#include <fcntl.h>
	#ifndef closesocket
	#define closesocket close
	#endif
#endif
	#include <errno.h>

#include "stdlog.h"
#include "socket.h"


#ifndef _YOD_SOCKET_DEBUG
#define _YOD_SOCKET_DEBUG 										0
#endif

#define YOD_SOCKET_LISTEN_BACKLOG								1024


/** {{{ static void yod_socket_set_sockaddr(const char *ipv4, const uint16_t port, struct sockaddr_in *saddr)
*/
static void yod_socket_set_sockaddr(const char *ipv4, const uint16_t port, struct sockaddr_in *saddr)
{
	struct hostent *host = NULL;
	char *sport = NULL;
	uint16_t iport = 0;
	char ipaddr[256];

	iport = port;
	memcpy(ipaddr, ipv4, strlen(ipv4) + 1);
	if ((sport = strchr(ipaddr, ':')) != NULL) {
		*sport = 0;
		iport = (uint16_t) atoi(++sport);
	}

	memset(saddr, 0, sizeof(struct sockaddr_in));
	saddr->sin_family = AF_INET;
	saddr->sin_port = htons(iport);
	saddr->sin_addr.s_addr = inet_addr(ipaddr);
	if (saddr->sin_addr.s_addr == INADDR_NONE)
	{
		host = gethostbyname(ipaddr);
		if (!host) {
			free(ipaddr);

			YOD_STDLOG_WARN("gethostbyname failed");
			return;
		}

		saddr->sin_addr.s_addr = *(uint32_t*) host->h_addr;
	}
}
/* }}} */


/** {{{ int _yod_socket_init(__ENV_PARM)
*/
int _yod_socket_init(__ENV_PARM)
{
	int ret = 0;

#ifdef _WIN32
	WSADATA wsaData;
	WORD wReqest = MAKEWORD(1, 1);
	if (WSAStartup(wReqest, &wsaData) != 0)
	{
		YOD_STDLOG_WARN("WSAStartup failed");
		ret = -1;
	}
#endif

#if (_YOD_SYSTEM_DEBUG && _YOD_SOCKET_DEBUG)
	yod_stdlog_debug(NULL, "%s() in %s:%d %s", __FUNCTION__, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ int _yod_socket_destroy(__ENV_PARM)
*/
int _yod_socket_destroy(__ENV_PARM)
{
	int ret = 0;

#if (_YOD_SYSTEM_DEBUG && _YOD_SOCKET_DEBUG)
	yod_stdlog_debug(NULL, "%s() in %s:%d %s", __FUNCTION__, __ENV_TRACE);
#else
	__ENV_VOID
#endif

#ifdef _WIN32
	if (WSACleanup() != 0) {
		ret = -1;
	}
#endif

	return ret;
}


/** {{{ yod_socket_t _yod_socket_listen(const char *ipv4, uint16_t port __ENV_CPARM)
*/
yod_socket_t _yod_socket_listen(const char *ipv4, uint16_t port __ENV_CPARM)
{
	struct sockaddr_in saddr;
	yod_socket_t ret = 0;

#if (_YOD_SYSTEM_DEBUG && _YOD_SOCKET_DEBUG)
	yod_stdlog_debug(NULL, "%s(%s, %d) in %s:%d %s",
		__FUNCTION__, ipv4, port, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (!ipv4) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return INVALID_SOCKET;
	}

	ret = socket(AF_INET, SOCK_STREAM, 0);
	if (ret == INVALID_SOCKET) {
		YOD_STDLOG_WARN("socket failed");
		return INVALID_SOCKET;
	}

	yod_socket_set_reuseable(ret);
	yod_socket_set_sockaddr(ipv4, port, &saddr);
	if (bind(ret, (struct sockaddr *)&saddr, sizeof(saddr)) != 0) {
		closesocket(ret);

		YOD_STDLOG_WARN("bind failed");
		return INVALID_SOCKET;
	}

	if (listen(ret, YOD_SOCKET_LISTEN_BACKLOG) != 0) {
		closesocket(ret);

		YOD_STDLOG_WARN("listen failed");
		return INVALID_SOCKET;
	}

	return ret;
}
/* }}} */


/** {{{ yod_socket_t _yod_socket_accept(yod_socket_t fd __ENV_CPARM)
*/
yod_socket_t _yod_socket_accept(yod_socket_t fd __ENV_CPARM)
{
	struct sockaddr_in saddr;
	socklen_t saddr_size = sizeof(saddr);
	yod_socket_t ret = 0;

#if (_YOD_SYSTEM_DEBUG && _YOD_SOCKET_DEBUG)
	yod_stdlog_debug(NULL, "%s(%d) in %s:%d %s",
		__FUNCTION__, fd, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	ret = accept(fd, (struct sockaddr*) &saddr, &saddr_size);
	if (ret == INVALID_SOCKET) {
		if (!yod_socket_is_block()) {
			YOD_STDLOG_WARN("accept failed");
		}
	}

	return ret;
}
/* }}} */


/** {{{ yod_socket_t _yod_socket_connect(const char *ipv4, uint16_t port __ENV_CPARM)
*/
yod_socket_t _yod_socket_connect(const char *ipv4, uint16_t port __ENV_CPARM)
{
	struct sockaddr_in saddr;
	yod_socket_t ret = 0;

#if (_YOD_SYSTEM_DEBUG && _YOD_SOCKET_DEBUG)
	yod_stdlog_debug(NULL, "%s(%s, %d) in %s:%d %s",
		__FUNCTION__, ipv4, port, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (!ipv4) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return INVALID_SOCKET;
	}

	ret = socket(AF_INET, SOCK_STREAM, 0);
	if (ret == INVALID_SOCKET) {
		YOD_STDLOG_WARN("socket failed");
		return INVALID_SOCKET;
	}

	yod_socket_set_sockaddr(ipv4, port, &saddr);
	if ((connect(ret, (struct sockaddr*)&saddr, sizeof(saddr)) != 0)
		&& (!yod_socket_is_block()))
	{
		closesocket(ret);
		return INVALID_SOCKET;
	}

	return ret;
}
/* }}} */


/** {{{ int _yod_socket_send(yod_socket_t fd, char *buf, int len __ENV_CPARM)
*/
int _yod_socket_send(yod_socket_t fd, char *buf, int len __ENV_CPARM)
{
	int ret = 0;

	if ((ret = send(fd, buf, len, 0)) == SOCKET_ERROR)
	{
		if (yod_socket_is_block()) {
			ret = 0;
/*
			yod_stdlog_info(NULL, "%s(%d, %p, %d): socket is block, errno=%d in %s:%d",
				__FUNCTION__, fd, buf, len, errno, __ENV_TRACE2);
*/
		}
#ifdef _WIN32
		else if (errno != WSAECONNABORTED && errno != WSAECONNRESET)
#else
		else if (errno != EPIPE && errno != ECONNRESET)
#endif
		{
			YOD_STDLOG_WARN("send failed");
		}
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_SOCKET_DEBUG)
	yod_stdlog_debug(NULL, "%s(%d, %p, %d): %d in %s:%d %s",
		__FUNCTION__, fd, buf, len, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ int _yod_socket_recv(yod_socket_t fd, char *buf, int len __ENV_CPARM)
*/
int _yod_socket_recv(yod_socket_t fd, char *buf, int len __ENV_CPARM)
{
	int ret = 0;

#if (_YOD_SYSTEM_DEBUG && _YOD_SOCKET_DEBUG)
	yod_stdlog_debug(NULL, "%s(%d, %p, %d) in %s:%d %s",
		__FUNCTION__, fd, buf, len, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if ((ret = recv(fd, buf, len, 0)) == SOCKET_ERROR) {
		if (!yod_socket_is_block()) {
			ret = 0;
		}
	}
	return ret;
}
/* }}} */


/** {{{ int _yod_socket_close(yod_socket_t fd __ENV_CPARM)
*/
int _yod_socket_close(yod_socket_t fd __ENV_CPARM)
{
#if (_YOD_SYSTEM_DEBUG && _YOD_SOCKET_DEBUG)
	yod_stdlog_debug(NULL, "%s(%d) in %s:%d %s",
		__FUNCTION__, fd, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return closesocket(fd);
}
/* }}} */


/** {{{ int _yod_socket_get_sock(yod_socket_t fd, uint32_t *ipv4, uint16_t *port __ENV_CPARM)
*/
int _yod_socket_get_sock(yod_socket_t fd, uint32_t *ipv4, uint16_t *port __ENV_CPARM)
{
	struct sockaddr_in saddr;
	socklen_t saddr_size = sizeof(saddr);

#if (_YOD_SYSTEM_DEBUG && _YOD_SOCKET_DEBUG)
	yod_stdlog_debug(NULL, "%s(%d, %p, %p) in %s:%d %s",
		__FUNCTION__, fd, ipv4, port, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (getsockname(fd, (struct sockaddr*) &saddr, &saddr_size) != 0) {
		YOD_STDLOG_WARN("getsockname failed");
		return (-1);
	}

	if (ipv4) {
		*ipv4 = ntohl(saddr.sin_addr.s_addr);
	}

	if (port) {
		*port = ntohs(saddr.sin_port);
	}

	return (0);
}
/* }}} */


/** {{{ int _yod_socket_get_peer(yod_socket_t fd, uint32_t ipv4, uint16_t *port __ENV_CPARM)
*/
int _yod_socket_get_peer(yod_socket_t fd, uint32_t *ipv4, uint16_t *port __ENV_CPARM)
{
	struct sockaddr_in saddr;
	socklen_t saddr_size = sizeof(saddr);

#if (_YOD_SYSTEM_DEBUG && _YOD_SOCKET_DEBUG)
	yod_stdlog_debug(NULL, "%s(%d, %p, %p) in %s:%d %s",
		__FUNCTION__, fd, ipv4, port, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (getpeername(fd, (struct sockaddr*) &saddr, &saddr_size) != 0) {
		YOD_STDLOG_WARN("getpeername failed");
		return (-1);
	}

	if (ipv4) {
		*ipv4 = ntohl(saddr.sin_addr.s_addr);
	}

	if (port) {
		*port = ntohs(saddr.sin_port);
	}

	return (0);
}
/* }}} */


/** {{{ int _yod_socket_get_mac(byte mac[6], char *eth __ENV_CPARM)
*/
int _yod_socket_get_mac(byte mac[6], char *eth __ENV_CPARM)
{
#ifdef _WIN32
	NCB ncb = {0};
	LANA_ENUM lenum = {0};
	ADAPTER_STATUS adapt = {0};
	int ifnum = 0;
	UCHAR ret = 0;
#else
	yod_socket_t sockfd = 0;
	struct ifreq ifreq[16];
	struct ifconf ifcfg;
	int ifnum = 0;
	int ret = -1;
#ifdef __APPLE__
	char tmp[80];

	eth = NULL;
#endif
#endif

#if (_YOD_SYSTEM_DEBUG && _YOD_SOCKET_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %s) in %s:%d %s",
		__FUNCTION__, mac, eth, __ENV_TRACE);
#else
	__ENV_VOID
#endif

#ifdef _WIN32
	memset(&ncb, 0, sizeof(ncb));
	memset(&lenum, 0, sizeof(lenum));
	ncb.ncb_command = NCBENUM;
	ncb.ncb_buffer = (UCHAR *) &lenum;
	ncb.ncb_length = sizeof(lenum);
	ret = Netbios(&ncb);
	if (ret != NRC_GOODRET) {
		YOD_STDLOG_WARN("netbios failed");
		return (-1);
	}
	ifnum = lenum.length;

	while (ifnum-- > 0) {
		memset(&ncb, 0, sizeof(ncb));
		ncb.ncb_command = NCBRESET;
		ncb.ncb_lana_num = lenum.lana[ifnum];
		ret = Netbios(&ncb);
		if (ret == NRC_GOODRET) {
			break;
		}
	}

	if (ret != NRC_GOODRET) {
		YOD_STDLOG_WARN("netbios failed");
		return (-1);
	}

	memset(&ncb, 0, sizeof(ncb));
	ncb.ncb_command = NCBASTAT;
	ncb.ncb_lana_num = lenum.lana[ifnum];

	memcpy(ncb.ncb_callname, "*", 2);
	ncb.ncb_buffer = (UCHAR *) &adapt;
	ncb.ncb_length = sizeof(adapt);
	ret = Netbios(&ncb);
	if (ret != NRC_GOODRET) {
		YOD_STDLOG_WARN("netbios failed");
		return (-1);
	}

	memcpy(mac, adapt.adapter_address, 6);

	(void) eth;
#else
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd == INVALID_SOCKET) {
		YOD_STDLOG_WARN("socket failed");
		return (-1);
	}

	if (!eth) {
		ifcfg.ifc_len = sizeof(ifreq);
		ifcfg.ifc_buf = (caddr_t) ifreq;
		if ((ioctl(sockfd, SIOCGIFCONF, (char *) &ifcfg)) != 0) {
			closesocket(sockfd);

			YOD_STDLOG_WARN("ioctl failed");
			return (-1);
		}
		ifnum = ifcfg.ifc_len / sizeof (struct ifreq);
	}
	else {
		memset(&ifreq[0], 0, sizeof(ifreq[0]));
		strcpy(ifreq[0].ifr_name, eth);
		ifnum = 1;
	}

	while (ifnum-- > 0) {
#ifdef __APPLE__
		if (ifreq[ifnum].ifr_addr.sa_family != AF_LINK) {
			continue;
		}
		memcpy(mac, ifreq[ifnum].ifr_addr.sa_data, 6);
#else
		if ((ioctl(sockfd, SIOCGIFHWADDR, &ifreq[ifnum])) != 0) {
			YOD_STDLOG_WARN("ioctl failed");
			continue;
		}
		memcpy(mac, ifreq[ifnum].ifr_hwaddr.sa_data, 6);
#endif
		if (*((uint32_t *) mac) > 0) {
			ret = 0;
			break;
		}
	}
	closesocket(sockfd);
#endif

	return ret;
}
/* }}} */

/** {{{ void _yod_socket_set_nonblock(yod_socket_t fd __ENV_CPARM)
*/
void _yod_socket_set_nonblock(yod_socket_t fd __ENV_CPARM)
{
#ifdef _WIN32
	u_long nonblock = 1;
	int ret = ioctlsocket(fd, FIONBIO, &nonblock);
#else
	int ret = fcntl(fd, F_SETFL, O_NONBLOCK | fcntl(fd, F_GETFL));
#endif

#if (_YOD_SYSTEM_DEBUG && _YOD_SOCKET_DEBUG)
	yod_stdlog_debug(NULL, "%s(%d) in %s:%d %s",
		__FUNCTION__, fd, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (ret == SOCKET_ERROR)
	{
		YOD_STDLOG_WARN("set_nonblock failed");
	}
}
/* }}} */


/** {{{ void _yod_socket_set_reuseable(yod_socket_t fd __ENV_CPARM)
*/
void _yod_socket_set_reuseable(yod_socket_t fd __ENV_CPARM)
{
	int reuseable = 1;
	int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseable, sizeof(int));

#if (_YOD_SYSTEM_DEBUG && _YOD_SOCKET_DEBUG)
	yod_stdlog_debug(NULL, "%s(%d) in %s:%d %s",
		__FUNCTION__, fd, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (ret == SOCKET_ERROR)
	{
		YOD_STDLOG_WARN("set_reuseable failed");
	}
}
/* }}} */


/** {{{ void _yod_socket_set_nodelay(yod_socket_t fd __ENV_CPARM)
*/
void _yod_socket_set_nodelay(yod_socket_t fd __ENV_CPARM)
{
	int nodelay = 1;
	int ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(int));

#if (_YOD_SYSTEM_DEBUG && _YOD_SOCKET_DEBUG)
	yod_stdlog_debug(NULL, "%s(%d) in %s:%d %s",
		__FUNCTION__, fd, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (ret == SOCKET_ERROR)
	{
		YOD_STDLOG_WARN("set_nodelay failed");
	}
}
/* }}} */


/** {{{ int _yod_socket_is_block()
*/
int _yod_socket_is_block()
{
#ifdef _WIN32
	errno = WSAGetLastError();
	return ((errno == WSAEINPROGRESS) || (errno == WSAEWOULDBLOCK));
#else
	return ((errno == EAGAIN) || (errno == EINPROGRESS) || (errno == EWOULDBLOCK));
#endif
}
/* }}} */
