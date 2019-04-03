#ifndef __YOD_SERVER_H__
#define __YOD_SERVER_H__

#include "evloop.h"
#include "socket.h"


enum
{
	YOD_SERVER_EVENT_CONNECT = 0x01,
	YOD_SERVER_EVENT_INPUT = 0x02,
	YOD_SERVER_EVENT_OUTPUT = 0x04,
	YOD_SERVER_EVENT_CLOSE = 0x08,
	YOD_SERVER_EVENT_TIMEOUT = 0x10,
	YOD_SERVER_EVENT_BLOCK = 0x20
};


/* yod_server_t */
typedef struct _yod_server_t 									yod_server_t;

/* yod_server_fn */
typedef int (*yod_server_fn) (yod_server_t *server, yod_socket_t fd, short what, void *arg __ENV_CPARM);


#define __EVS_CONNECT 											YOD_SERVER_EVENT_CONNECT
#define __EVS_INPUT 											YOD_SERVER_EVENT_INPUT
#define __EVS_OUTPUT 											YOD_SERVER_EVENT_OUTPUT
#define __EVS_CLOSE 											YOD_SERVER_EVENT_CLOSE
#define __EVS_TIMEOUT 											YOD_SERVER_EVENT_TIMEOUT
#define __EVS_BLOCK 											YOD_SERVER_EVENT_BLOCK


#define yod_server_new(n) 										_yod_server_new(n __ENV_CARGS)
#define yod_server_free(x) 										_yod_server_free(x __ENV_CARGS)

#define yod_server_start(x) 									_yod_server_start(x __ENV_CARGS)
#define yod_server_stop(x) 										_yod_server_stop(x __ENV_CARGS)

#define yod_server_listen(x, s, p, f, a, t) 					_yod_server_listen(x, s, p, f, a, t __ENV_CARGS)
#define yod_server_connect(x, s, p, f, a, t) 					_yod_server_connect(x, s, p, f, a, t __ENV_CARGS)
#define yod_server_tick(x, f, a, t) 							_yod_server_tick(x, f, a, t __ENV_CARGS)

#define yod_server_recv(x, l) 									_yod_server_recv(x, l __ENV_CARGS)
#define yod_server_send(x, d, l) 								_yod_server_send(x, d, l __ENV_CARGS)
#define yod_server_close(x) 									_yod_server_close(x __ENV_CARGS)

#define yod_server_setcb(x, f, a) 								_yod_server_setcb(x, f, a __ENV_CARGS)
#define yod_server_count(x) 									_yod_server_count(x __ENV_CARGS)
#define yod_server_dump(x) 										_yod_server_dump(x)


yod_server_t *_yod_server_new(int thread_num __ENV_CPARM);
void _yod_server_free(yod_server_t *self __ENV_CPARM);

void _yod_server_start(yod_server_t *self __ENV_CPARM);
void _yod_server_stop(yod_server_t *self __ENV_CPARM);

int _yod_server_listen(yod_server_t *self, const char *server_ip, uint16_t port,
	yod_server_fn func, void *arg, uint32_t timeout __ENV_CPARM);

int _yod_server_connect(yod_server_t *self, const char *server_ip, uint16_t port,
	yod_server_fn func, void *arg, uint32_t timeout __ENV_CPARM);

int _yod_server_tick(yod_server_t *self, yod_server_fn func, void *arg, uint32_t tick __ENV_CPARM);

byte *_yod_server_recv(yod_server_t *self, int *len __ENV_CPARM);
int _yod_server_send(yod_server_t *self, byte *data, int len __ENV_CPARM);
void _yod_server_close(yod_server_t *self __ENV_CPARM);

int _yod_server_setcb(yod_server_t *self, yod_server_fn func, void *arg __ENV_CPARM);
ulong _yod_server_count(yod_server_t *self __ENV_CPARM);
char *_yod_server_dump(yod_server_t *self);

#endif
