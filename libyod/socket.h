#ifndef __YOD_SOCKET_H__
#define __YOD_SOCKET_H__

#include "system.h"

/* yod_socket_t */
#ifdef _WIN32
typedef intptr_t 												yod_socket_t;
#else
typedef int 													yod_socket_t;
#endif


#define yod_socket_init() 										_yod_socket_init(__ENV_ARGS)
#define yod_socket_listen(i, p) 								_yod_socket_listen(i, p __ENV_CARGS)
#define yod_socket_connect(i, p) 								_yod_socket_connect(i, p __ENV_CARGS)
#define yod_socket_accept(d) 									_yod_socket_accept(d __ENV_CARGS)
#define yod_socket_send(d, b, l) 								_yod_socket_send(d, b, l __ENV_CARGS)
#define yod_socket_recv(d, b, l) 								_yod_socket_recv(d, b, l __ENV_CARGS)
#define yod_socket_close(d) 									_yod_socket_close(d __ENV_CARGS)

#define yod_socket_get_sock(d, i, p) 							_yod_socket_get_sock(d, i, p __ENV_CARGS)
#define yod_socket_get_peer(d, i, p) 							_yod_socket_get_peer(d, i, p __ENV_CARGS)
#define yod_socket_get_mac(m, e) 								_yod_socket_get_mac(m, e __ENV_CARGS)

#define yod_socket_set_nonblock(d) 								_yod_socket_set_nonblock(d __ENV_CARGS)
#define yod_socket_set_reuseable(d) 							_yod_socket_set_reuseable(d __ENV_CARGS)
#define yod_socket_set_nodelay(d) 								_yod_socket_set_nodelay(d __ENV_CARGS)
#define yod_socket_is_block() 									_yod_socket_is_block()


int _yod_socket_init(__ENV_PARM);
yod_socket_t _yod_socket_listen(const char *ipv4, uint16_t port __ENV_CPARM);
yod_socket_t _yod_socket_connect(const char *ipv4, uint16_t port __ENV_CPARM);
yod_socket_t _yod_socket_accept(yod_socket_t fd __ENV_CPARM);
int _yod_socket_send(yod_socket_t fd, char *buf, int len __ENV_CPARM);
int _yod_socket_recv(yod_socket_t fd, char *buf, int len __ENV_CPARM);
int _yod_socket_close(yod_socket_t fd __ENV_CPARM);

int _yod_socket_get_sock(yod_socket_t fd, uint32_t *ipv4, uint16_t *port __ENV_CPARM);
int _yod_socket_get_peer(yod_socket_t fd, uint32_t *ipv4, uint16_t *port __ENV_CPARM);
int _yod_socket_get_mac(byte mac[6], char *eth __ENV_CPARM);

void _yod_socket_set_nonblock(yod_socket_t fd __ENV_CPARM);
void _yod_socket_set_reuseable(yod_socket_t fd __ENV_CPARM);
void _yod_socket_set_nodelay(yod_socket_t fd __ENV_CPARM);
int _yod_socket_is_block();

#endif
