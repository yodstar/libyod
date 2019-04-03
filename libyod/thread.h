#ifndef __YOD_THREAD_H__
#define __YOD_THREAD_H__

#include "evloop.h"


/* yod_thread_t */
typedef struct _yod_thread_t 									yod_thread_t;


#define yod_thread_new(n) 										_yod_thread_new(n __ENV_CARGS)
#define yod_thread_free(x) 										_yod_thread_free(x __ENV_CARGS)

#define yod_thread_add(x) 										_yod_thread_add(x __ENV_CARGS)
#define yod_thread_run(x, f, a) 								_yod_thread_run(x, f, a __ENV_CARGS)

#define yod_thread_count(x) 									_yod_thread_count(x __ENV_CARGS)
#define yod_thread_dump(x) 										_yod_thread_dump(x)


yod_thread_t *_yod_thread_new(int thread_num __ENV_CPARM);
void _yod_thread_free(yod_thread_t *self __ENV_CPARM);

int _yod_thread_add(yod_thread_t *self __ENV_CPARM);
int _yod_thread_run(yod_thread_t *self, yod_evloop_fn func, void *arg __ENV_CPARM);

ulong _yod_thread_count(yod_thread_t *self __ENV_CPARM);
char *_yod_thread_dump(yod_thread_t *self);

#endif
