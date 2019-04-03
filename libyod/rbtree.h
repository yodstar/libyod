#ifndef __YOD_RBTREE_H__
#define __YOD_RBTREE_H__

#include "system.h"


/* yod_rbtree_t */
typedef struct _yod_rbtree_t 									yod_rbtree_t;


#define yod_rbtree_new(f) 										_yod_rbtree_new(f __ENV_CARGS)
#define yod_rbtree_free(x) 										_yod_rbtree_free(x __ENV_CARGS)

#define yod_rbtree_add(x, k, v, f) 								_yod_rbtree_add(x, k, v, f __ENV_CARGS)
#define yod_rbtree_set(x, k, v) 								_yod_rbtree_add(x, k, v, 1 __ENV_CARGS)
#define yod_rbtree_del(x, k) 									_yod_rbtree_del(x, k __ENV_CARGS)
#define yod_rbtree_find(x, k) 									_yod_rbtree_find(x, k __ENV_CARGS)

#define yod_rbtree_count(x) 									_yod_rbtree_count(x __ENV_CARGS)
#define yod_rbtree_head(x, k) 									_yod_rbtree_head(x, k __ENV_CARGS)
#define yod_rbtree_tail(x, k) 									_yod_rbtree_tail(x, k __ENV_CARGS)
#define yod_rbtree_next(x, k) 									_yod_rbtree_next(x, k __ENV_CARGS)
#define yod_rbtree_prev(x, k) 									_yod_rbtree_prev(x, k __ENV_CARGS)


yod_rbtree_t *_yod_rbtree_new(void (*vfree) (void * __ENV_CPARM) __ENV_CPARM);
void _yod_rbtree_free(yod_rbtree_t *self __ENV_CPARM);

int _yod_rbtree_add(yod_rbtree_t *self, uint64_t key, void *value, int force __ENV_CPARM);
int _yod_rbtree_del(yod_rbtree_t *self, uint64_t key __ENV_CPARM);
void *_yod_rbtree_find(yod_rbtree_t *self, uint64_t key __ENV_CPARM);

ulong _yod_rbtree_count(yod_rbtree_t *self __ENV_CPARM);
void *_yod_rbtree_head(yod_rbtree_t *self, uint64_t *key __ENV_CPARM);
void *_yod_rbtree_tail(yod_rbtree_t *self, uint64_t *key __ENV_CPARM);
void *_yod_rbtree_next(yod_rbtree_t *self, uint64_t *key __ENV_CPARM);
void *_yod_rbtree_prev(yod_rbtree_t *self, uint64_t *key __ENV_CPARM);

void yod_rbtree_print(yod_rbtree_t *self);

#endif
