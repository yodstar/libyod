#ifndef __YOD_HTABLE_H__
#define __YOD_HTABLE_H__

#include "system.h"


/* yod_htable_t */
typedef struct _yod_htable_t 									yod_htable_t;


#define yod_htable_new(f) 										_yod_htable_new(f __ENV_CARGS)
#define yod_htable_ref(x) 										_yod_htable_ref(x __ENV_CARGS)
#define yod_htable_reset(x) 									_yod_htable_reset(x __ENV_CARGS)
#define yod_htable_free(x) 										_yod_htable_free(x __ENV_CARGS)

#define yod_htable_add(x, i, k, l, v) 							_yod_htable_add(x, i, k, l, v, 1 __ENV_CARGS)
#define yod_htable_del(x, i, k, l) 								_yod_htable_del(x, i, k, l __ENV_CARGS)
#define yod_htable_find(x, i, k, l) 							_yod_htable_find(x, i, k, l __ENV_CARGS)

#define yod_htable_count(x) 									_yod_htable_count(x __ENV_CARGS)
#define yod_htable_head(x, i, k, l) 							_yod_htable_head(x, i, k, l __ENV_CARGS)
#define yod_htable_tail(x, i, k, l) 							_yod_htable_tail(x, i, k, l __ENV_CARGS)
#define yod_htable_next(x, i, k, l) 							_yod_htable_next(x, i, k, l __ENV_CARGS)
#define yod_htable_prev(x, i, k, l) 							_yod_htable_prev(x, i, k, l __ENV_CARGS)

#define yod_htable_add_index(x, i, v) 							_yod_htable_add(x, i, NULL, 0, v, 1 __ENV_CARGS)
#define yod_htable_del_index(x, i) 								_yod_htable_del(x, i, NULL, 0 __ENV_CARGS)
#define yod_htable_find_index(x, i) 							_yod_htable_find(x, i, NULL, 0 __ENV_CARGS)

#define yod_htable_head_index(x, i) 							_yod_htable_head(x, i, NULL, NULL __ENV_CARGS)
#define yod_htable_tail_index(x, i) 							_yod_htable_tail(x, i, NULL, NULL __ENV_CARGS)
#define yod_htable_next_index(x, i) 							_yod_htable_next(x, i, NULL, NULL __ENV_CARGS)
#define yod_htable_prev_index(x, i) 							_yod_htable_prev(x, i, NULL, NULL __ENV_CARGS)

#define yod_htable_add_assocl(x, k, l, v) 						_yod_htable_add(x, 0, k, l, v, 1 __ENV_CARGS)
#define yod_htable_del_assocl(x, k, l) 							_yod_htable_del(x, 0, k, l __ENV_CARGS)
#define yod_htable_find_assocl(x, k, l) 						_yod_htable_find(x, 0, k, l __ENV_CARGS)

#define yod_htable_head_assocl(x, k, l) 						_yod_htable_head(x, NULL, k, l __ENV_CARGS)
#define yod_htable_tail_assocl(x, k, l) 						_yod_htable_tail(x, NULL, k, l __ENV_CARGS)
#define yod_htable_next_assocl(x, k, l) 						_yod_htable_next(x, NULL, k, l __ENV_CARGS)
#define yod_htable_prev_assocl(x, k, l) 						_yod_htable_prev(x, NULL, k, l __ENV_CARGS)

#define yod_htable_add_assoc(x, k, v) 							_yod_htable_add(x, 0, k, strlen(k), v, 1 __ENV_CARGS)
#define yod_htable_del_assoc(x, k) 								_yod_htable_del(x, 0, k, strlen(k) __ENV_CARGS)
#define yod_htable_find_assoc(x, k) 							_yod_htable_find(x, 0, k, strlen(k) __ENV_CARGS)

#define yod_htable_head_assoc(x, k) 							_yod_htable_head(x, NULL, k, NULL __ENV_CARGS)
#define yod_htable_tail_assoc(x, k) 							_yod_htable_tail(x, NULL, k, NULL __ENV_CARGS)
#define yod_htable_next_assoc(x, k) 							_yod_htable_next(x, NULL, k, NULL __ENV_CARGS)
#define yod_htable_prev_assoc(x, k) 							_yod_htable_prev(x, NULL, k, NULL __ENV_CARGS)


yod_htable_t *_yod_htable_new(void (*vfree) (void * __ENV_CPARM) __ENV_CPARM);
void _yod_htable_free(yod_htable_t *self __ENV_CPARM);
yod_htable_t *_yod_htable_ref(yod_htable_t *self __ENV_CPARM);
int _yod_htable_reset(yod_htable_t *self __ENV_CPARM);

int _yod_htable_add(yod_htable_t *self, ulong num_key, const char *str_key, size_t key_len, void *value, int force __ENV_CPARM);
int _yod_htable_del(yod_htable_t *self, ulong num_key, const char *str_key, size_t key_len __ENV_CPARM);
void *_yod_htable_find(yod_htable_t *self, ulong num_key, const char *str_key, size_t key_len __ENV_CPARM);

ulong _yod_htable_count(yod_htable_t *self __ENV_CPARM);
void *_yod_htable_head(yod_htable_t *self, ulong *num_key, char **str_key, size_t *key_len __ENV_CPARM);
void *_yod_htable_tail(yod_htable_t *self, ulong *num_key, char **str_key, size_t *key_len __ENV_CPARM);
void *_yod_htable_next(yod_htable_t *self, ulong *num_key, char **str_key, size_t *key_len __ENV_CPARM);
void *_yod_htable_prev(yod_htable_t *self, ulong *num_key, char **str_key, size_t *key_len __ENV_CPARM);

#endif
