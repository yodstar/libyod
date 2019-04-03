#ifndef __YOD_EVLOOP_H__
#define __YOD_EVLOOP_H__

#include "socket.h"


enum
{
	YOD_EVLOOP_EVENT_READ = 0x01,
	YOD_EVLOOP_EVENT_WRITE = 0x02,
	YOD_EVLOOP_EVENT_LOOP = 0x04,
	YOD_EVLOOP_EVENT_CLOSE = 0x08,
	YOD_EVLOOP_EVENT_ERROR = 0x10,
	YOD_EVLOOP_EVENT_ALL = 0xFF,
};


/* yod_evloop_t */
typedef struct _yod_evloop_t 									yod_evloop_t;

/* yod_evloop_fn */
typedef void (*yod_evloop_fn) (yod_evloop_t *evloop, yod_socket_t fd, short what, void *arg __ENV_CPARM);


#define __EVL_READ 												YOD_EVLOOP_EVENT_READ
#define __EVL_WRITE 											YOD_EVLOOP_EVENT_WRITE
#define __EVL_LOOP 												YOD_EVLOOP_EVENT_LOOP
#define __EVL_CLOSE 											YOD_EVLOOP_EVENT_CLOSE
#define __EVL_ERROR 											YOD_EVLOOP_EVENT_ERROR
#define __EVL_ALL 												YOD_EVLOOP_EVENT_ALL


#define yod_evloop_new() 										_yod_evloop_new(__ENV_ARGS)
#define yod_evloop_free(x) 										_yod_evloop_free(x __ENV_CARGS)

#define yod_evloop_start(x, w) 									_yod_evloop_start(x, w __ENV_CARGS)
#define yod_evloop_stop(x) 										_yod_evloop_stop(x __ENV_CARGS)

#define yod_evloop_loop(x, f, a) 								_yod_evloop_loop(x, f, a __ENV_CARGS)
#define yod_evloop_add(x, d, w, f, a, e) 						_yod_evloop_add(x, d, w, f, a, e __ENV_CARGS)
#define yod_evloop_set(x, w, a) 								_yod_evloop_set(x, w, a __ENV_CARGS)
#define yod_evloop_del(x, w) 									_yod_evloop_del(x, w __ENV_CARGS)

#define yod_evloop_count(x) 									_yod_evloop_count(x __ENV_CARGS)
#define yod_evloop_dump(x) 										_yod_evloop_dump(x)


yod_evloop_t *_yod_evloop_new(__ENV_PARM);
void _yod_evloop_free(yod_evloop_t *self __ENV_CPARM);

void _yod_evloop_start(yod_evloop_t *self, uint32_t wait __ENV_CPARM);
void _yod_evloop_stop(yod_evloop_t *self __ENV_CPARM);

int _yod_evloop_loop(yod_evloop_t *self, yod_evloop_fn func, void *arg __ENV_CPARM);
int _yod_evloop_add(yod_evloop_t *self, yod_socket_t fd, short what, yod_evloop_fn func, void *arg, yod_evloop_t **evl __ENV_CPARM);
int _yod_evloop_set(yod_evloop_t *self, short what, void *arg __ENV_CPARM);
int _yod_evloop_del(yod_evloop_t *self, short what __ENV_CPARM);

ulong _yod_evloop_count(yod_evloop_t *self __ENV_CPARM);
char *_yod_evloop_dump(yod_evloop_t *self);

#endif
