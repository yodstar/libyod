#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "stdlog.h"
#include "htable.h"


#ifndef _YOD_HTABLE_DEBUG
#define _YOD_HTABLE_DEBUG 										0
#endif


/* yod_htable_v */
typedef struct _yod_htable_v
{
	ulong num_key;
	char *str_key;
	size_t key_len;
	void *value;

	struct _yod_htable_v *next;
	struct _yod_htable_v *prev;
	struct _yod_htable_v *v_next;
	struct _yod_htable_v *v_prev;
} yod_htable_v;


/* yod_htable_t */
struct _yod_htable_t
{
	pthread_mutex_t lock;

	ulong mask;

	ulong size;
	ulong count;

	ulong is_ref;

	yod_htable_v **nodes;
	yod_htable_v *head;
	yod_htable_v *tail;
	yod_htable_v *curr;

	void (*vfree) (void * __ENV_CPARM);
};


static int _yod_htable_resize(yod_htable_t *self __ENV_CPARM);
static ulong _yod_htable_str_nkey(const char *str_key, size_t key_len);


/** {{{ yod_htable_t *_yod_htable_new(void (*vfree) (void * __ENV_CPARM) __ENV_CPARM)
*/
yod_htable_t *_yod_htable_new(void (*vfree) (void * __ENV_CPARM) __ENV_CPARM)
{
	yod_htable_t *self = NULL;

	self = (yod_htable_t *) malloc(sizeof(yod_htable_t) + 1);
	if (!self) {
		YOD_STDLOG_ERROR("malloc failed");
		return NULL;
	}

	if (pthread_mutex_init(&self->lock, NULL) != 0) {
		free(self);

		YOD_STDLOG_ERROR("pthread_mutex_init failed");
		return NULL;
	}

	{
		self->size = 1 << 3;
		self->mask = self->size - 1;
		self->count = 0;

		self->is_ref = 0;
		self->nodes = NULL;
		self->head = NULL;
		self->tail = NULL;
		self->curr = NULL;
		self->vfree = vfree;
	}

	self->nodes = calloc(self->size, sizeof(yod_htable_v *));
	if (self->nodes == NULL) {
		yod_htable_free(self);

		YOD_STDLOG_ERROR("calloc failed");
		return NULL;
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_HTABLE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): %p in %s:%d %s",
		__FUNCTION__, vfree, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return self;
}
/* }}} */


/** {{{ void _yod_htable_free(yod_htable_t *self __ENV_CPARM)
*/
void _yod_htable_free(yod_htable_t *self __ENV_CPARM)
{
	yod_htable_v *node = NULL;
	yod_htable_v *temp = NULL;
#if (_YOD_HTABLE_DEBUG & 0x02)
	ulong *ht_nodes = NULL;
	ulong ht_count = 0;
	ulong ht_size = 0;
	ulong k = 0;
	ulong i = 0;
#endif

	if (!self) {
		return;
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_HTABLE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): {is_ref=%d, count=%lu} in %s:%d %s",
		__FUNCTION__, self, self->is_ref, self->count, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	pthread_mutex_lock(&self->lock);
	if (self->is_ref) {
		-- self->is_ref;
		pthread_mutex_unlock(&self->lock);
		return;
	}

#if (_YOD_HTABLE_DEBUG & 0x02)
	if (self->count > 0) {
		ht_nodes = (ulong *) calloc(self->size, sizeof(ulong));
		ht_count = self->count;
		ht_size = self->size;
	}
#endif

	/* nodes */
	for (node = self->head; node != NULL;) {
#if (_YOD_HTABLE_DEBUG & 0x02)
		++ ht_nodes[node->num_key & self->mask];
#endif
		temp = node->next;
		node->num_key = 0;
		if (node->str_key) {
			free(node->str_key);
		}
		node->key_len = 0;
		if (self->vfree) {
			self->vfree(node->value __ENV_CARGS);
		}
		free(node);
		node = temp;
	}

	free(self->nodes);

	pthread_mutex_unlock(&self->lock);
	pthread_mutex_destroy(&self->lock);

	free(self);

#if (_YOD_HTABLE_DEBUG & 0x02)
	if (ht_count > 0) {
		yod_stdlog_dump(NULL,
			"----------------------------------------------------------------\n"
			"%d / %d\n"
			"----------------------------------------------------------------\n",
			ht_count, ht_size);

		for (i = 0; i < ht_size; ++i) {
			if (ht_nodes[i] > 0) {
				yod_stdlog_dump(NULL, "[%d] => %d", i, ht_nodes[i]);
				if ((k++ % 10) == 9) {
					yod_stdlog_dump(NULL, "\n");
				} else {
					yod_stdlog_dump(NULL, "\t");
				}
			}
		}
		yod_stdlog_dump(NULL,
			"\n----------------------------------------------------------------\n");
		
		if (ht_nodes) {
			free(ht_nodes);
		}
	}
#endif
}
/* }}} */


/** {{{ yod_htable_t *_yod_htable_ref(yod_htable_t *self __ENV_CPARM)
*/
yod_htable_t *_yod_htable_ref(yod_htable_t *self __ENV_CPARM)
{
	if (!self) {
		return NULL;
	}

	pthread_mutex_lock(&self->lock);
	++ self->is_ref;
	pthread_mutex_unlock(&self->lock);

#if (_YOD_SYSTEM_DEBUG && _YOD_HTABLE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): %d in %s:%d %s",
		__FUNCTION__, self, self->is_ref, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return self;
}
/* }}} */


/** {{{ int _yod_htable_reset(yod_htable_t *self __ENV_CPARM)
*/
int _yod_htable_reset(yod_htable_t *self __ENV_CPARM)
{
	yod_htable_v *node, *temp;

	if (!self) {
		return (-1);
	}

	pthread_mutex_lock(&self->lock);

	self->count = 0;
	self->head = NULL;
	self->tail = NULL;
	self->curr = NULL;

	/* nodes */
	memset(self->nodes, 0, self->size * sizeof(yod_htable_v *));
	for (node = self->head; node != NULL;) {
		temp = node->next;
		node->num_key = 0;
		if (node->str_key) {
			free(node->str_key);
			node->str_key = NULL;
		}
		node->key_len = 0;
		if (self->vfree) {
			self->vfree(node->value __ENV_CARGS);
			node->value = NULL;
		}
		node->next = NULL;
		node->prev = NULL;
		node->v_next = NULL;
		free(node);
		node = temp;
	}

	pthread_mutex_unlock(&self->lock);

#if (_YOD_SYSTEM_DEBUG && _YOD_HTABLE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p) in %s:%d %s",
		__FUNCTION__, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return (0);

}
/* }}} */


/** {{{ int _yod_htable_add(yod_htable_t *self, ulong num_key, const char *str_key, size_t key_len, void *value, int force  __ENV_CPARM)
*/
int _yod_htable_add(yod_htable_t *self, ulong num_key, const char *str_key, size_t key_len, void *value, int force __ENV_CPARM)
{
	yod_htable_v *node = NULL;
	ulong k = 0;

	if (!self) {
		return (-1);
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_HTABLE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %lu, %s, %d, %p) in %s:%d %s",
		__FUNCTION__, self, num_key, str_key, key_len, value, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	pthread_mutex_lock(&self->lock);

	if (self->count >= self->size) {
		_yod_htable_resize(self __ENV_CARGS);
	}

	if (key_len > 0) {
		num_key = _yod_htable_str_nkey(str_key, key_len);
	}

	k = num_key & self->mask;

	for (node = self->nodes[k]; node != NULL; node = node->v_next) {
		if ((node->num_key == num_key) && (node->key_len == key_len) && (memcmp(node->str_key, str_key, key_len) == 0)) {
			if (force) {
				if (self->vfree) {
					self->vfree(node->value __ENV_CARGS);
				}
				node->value = value;
			}
			pthread_mutex_unlock(&self->lock);
			return (force ? 0 : (-1));
		}
	}

	node = (yod_htable_v *) malloc(sizeof(yod_htable_v));
	if (node == NULL) {
		pthread_mutex_unlock(&self->lock);

		YOD_STDLOG_ERROR("malloc failed");
		return (-1);
	}

	{
		node->num_key = num_key;
		if (key_len > 0) {
			node->str_key = (char *) malloc((key_len + 1) * sizeof(char));
			memcpy(node->str_key, str_key, key_len);
			node->str_key[key_len] = '\0';
		} else {
			node->str_key = NULL;
		}
		node->key_len = key_len;
		node->value = value;
		node->next = NULL;
		node->prev = self->tail;
		node->v_next = self->nodes[k];
		node->v_prev = NULL;
		if (node->v_next) {
			node->v_next->v_prev = node;
		}

		self->nodes[k] = node;

		if (self->tail) {
			self->tail->next = node;
		}
		self->tail = node;

		if (!self->head) {
			self->head = node;
		}

		self->count++;
	}

#if (_YOD_HTABLE_DEBUG & 0x02)
	if (force != 0) {
		yod_stdlog_dump(NULL,
			"----------------------------------------------------------------\n"
			"%d / %d\n"
			"----------------------------------------------------------------\n",
			self->count, self->size);

		for (node = self->head; node != NULL; node = node->next) {
			yod_stdlog_dump(NULL,
				"[%04ld] => %p: "
				"{num_key=%lu, str_key=%s, key_len=%d, value=%p, "
				"next=%p, prev=%p, v_next=%p, v_prev=%p}\n",
				(ulong) (node->num_key & self->mask),
				node, node->num_key, node->str_key, node->key_len, node->value,
				node->next, node->prev, node->v_next, node->v_prev);
		}

		yod_stdlog_dump(NULL,
			"\n----------------------------------------------------------------\n");
	}
#endif

	pthread_mutex_unlock(&self->lock);

	return (0);
}
/* }}} */


/** {{{ int _yod_htable_del(yod_htable_t *self, ulong num_key, const char *str_key, size_t key_len __ENV_CPARM)
*/
int _yod_htable_del(yod_htable_t *self, ulong num_key, const char *str_key, size_t key_len __ENV_CPARM)
{
	yod_htable_v *node = NULL;
	ulong k = 0;

	if (!self) {
		return (-1);
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_HTABLE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %lu, %s, %d) in %s:%d %s",
		__FUNCTION__, self, num_key, str_key, key_len, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (key_len > 0) {
		num_key = _yod_htable_str_nkey(str_key, key_len);
	}

	pthread_mutex_lock(&self->lock);

	k = num_key & self->mask;

	for (node = self->nodes[k]; node != NULL; node = node->v_next) {
		if ((node->num_key == num_key) && (node->key_len == key_len) && (memcmp(node->str_key, str_key, key_len) == 0)) {
			/* tail node */
			if (!node->next) {
				self->tail = node->prev;
				if (self->tail) {
					self->tail->next = NULL;
				}
			} else {
				node->next->prev = node->prev;
			}
			
			/* head node */
			if (!node->prev) {
				self->head = node->next;
				if (self->head) {
					self->head->prev = NULL;
				}
			} else {
				node->prev->next = node->next;
			}
			
			if (node->v_next) {
				node->v_next->v_prev = node->v_prev;
			}

			if (!node->v_prev) {
				self->nodes[k] = node->v_next;
			} else {
				node->v_prev->v_next = node->v_next;
			}

			if (node->str_key) {
				free(node->str_key);
			}

			if (self->vfree) {
				self->vfree(node->value __ENV_CARGS);
			}
			free(node);

			self->count--;
			break;
		}
	}

#if (_YOD_HTABLE_DEBUG & 0x02)
	if (force != 0) {
		yod_stdlog_dump(NULL,
			"----------------------------------------------------------------\n"
			"%d / %d\n"
			"----------------------------------------------------------------\n",
			self->count, self->size);

		for (node = self->head; node != NULL; node = node->next) {
			yod_stdlog_dump(NULL,
				"[%04ld] => %p: "
				"{num_key=%lu, str_key=%s, key_len=%d, value=%p, "
				"next=%p, prev=%p, v_next=%p, v_prev=%p}\n",
				(ulong) (node->num_key & self->mask),
				node, node->num_key, node->str_key, node->key_len, node->value,
				node->next, node->prev, node->v_next, node->v_prev);
		}

		yod_stdlog_dump(NULL,
			"\n----------------------------------------------------------------\n");
	}
#endif

	pthread_mutex_unlock(&self->lock);

	return (0);
}
/* }}} */


/** {{{ void *_yod_htable_find(yod_htable_t *self, ulong num_key, const char *str_key, size_t key_len __ENV_CPARM)
*/
void *_yod_htable_find(yod_htable_t *self, ulong num_key, const char *str_key, size_t key_len __ENV_CPARM)
{
	yod_htable_v *node = NULL;
	void *value = NULL;
	ulong k;

#if (_YOD_SYSTEM_DEBUG && _YOD_HTABLE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %lu, %s, %d) in %s:%d %s",
		__FUNCTION__, self, num_key, str_key, key_len, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (!self) {
		return NULL;
	}

	if (key_len > 0) {
		num_key = _yod_htable_str_nkey(str_key, key_len);
	}

	pthread_mutex_lock(&self->lock);

	k = num_key & self->mask;

	for (node = self->nodes[k]; node != NULL; node = node->v_next) {
		if ((node->num_key == num_key) && (node->key_len == key_len) && (memcmp(node->str_key, str_key, key_len) == 0)) {
			value = node->value;
			break;
		}
	}

#if (_YOD_HTABLE_DEBUG & 0x02)
	yod_stdlog_dump(NULL,
		"----------------------------------------------------------------\n"
		"%d / %d\n"
		"----------------------------------------------------------------\n",
		self->count, self->size);

	for (node = self->head; node != NULL; node = node->next) {
		yod_stdlog_dump(NULL,
			"[%04ld] => %p: "
			"{num_key=%lu, str_key=%s, key_len=%d, value=%p, "
			"next=%p, prev=%p, v_next=%p, v_prev=%p}\n",
			(ulong) (node->num_key & self->mask),
			node, node->num_key, node->str_key, node->key_len, node->value,
			node->next, node->prev, node->v_next, node->v_prev);
	}

	yod_stdlog_dump(NULL,
		"\n----------------------------------------------------------------\n");
#endif

	pthread_mutex_unlock(&self->lock);

	return value;
}
/* }}} */


/** {{{ ulong _yod_htable_count(yod_htable_t *self __ENV_CPARM)
*/
ulong _yod_htable_count(yod_htable_t *self __ENV_CPARM)
{
	if (!self) {
		return 0;
	}

#if (_YOD_SYSTEM_DEBUG && (_YOD_HTABLE_DEBUG & 0x02))
	yod_stdlog_debug(NULL, "%s(%p): %lu in %s:%d %s",
		__FUNCTION__, self, self->count, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return self->count;
}
/* }}} */


/** {{{ void *_yod_htable_head(yod_htable_t *self, ulong *num_key, char **str_key, size_t *key_len __ENV_CPARM)
*/
void *_yod_htable_head(yod_htable_t *self, ulong *num_key, char **str_key, size_t *key_len __ENV_CPARM)
{
	void *value = NULL;

	if (!self) {
		return NULL;
	}

	pthread_mutex_lock(&self->lock);
	if (self->head) {
		self->curr = self->head;
		if (num_key) {
			*num_key = self->curr->num_key;
		}
		if (str_key) {
			*str_key = self->curr->str_key;
		}
		if (key_len) {
			*key_len = self->curr->key_len;
		}
		value = self->curr->value;
	}
	pthread_mutex_unlock(&self->lock);

#if (_YOD_SYSTEM_DEBUG && (_YOD_HTABLE_DEBUG & 0x02))
	yod_stdlog_debug(NULL, "%s(%p, %lu, %s, %d): %p in %s:%d %s",
		__FUNCTION__, self, (num_key ? *num_key : 0), (str_key ? *str_key : NULL),
		(key_len ? *key_len : 0), value, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return value;
}
/* }}} */


/** {{{ void *_yod_htable_tail(yod_htable_t *self, ulong *num_key, char **str_key, size_t *key_len __ENV_CPARM)
*/
void *_yod_htable_tail(yod_htable_t *self, ulong *num_key, char **str_key, size_t *key_len __ENV_CPARM)
{
	void *value = NULL;

	if (!self) {
		return NULL;
	}

	pthread_mutex_lock(&self->lock);
	if (self->tail) {
		self->curr = self->tail;
		if (num_key) {
			*num_key = self->curr->num_key;
		}
		if (str_key) {
			*str_key = self->curr->str_key;
		}
		if (key_len) {
			*key_len = self->curr->key_len;
		}
		value = self->curr->value;
	}
	pthread_mutex_unlock(&self->lock);

#if (_YOD_SYSTEM_DEBUG && (_YOD_HTABLE_DEBUG & 0x02))
	yod_stdlog_debug(NULL, "%s(%p, %lu, %s, %d): %p in %s:%d %s",
		__FUNCTION__, self, (num_key ? *num_key : 0), (str_key ? *str_key : NULL),
		(key_len ? *key_len : 0), value, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return value;
}
/* }}} */


/** {{{ void *_yod_htable_next(yod_htable_t *self, ulong *num_key, char **str_key, size_t *key_len __ENV_CPARM)
*/
void *_yod_htable_next(yod_htable_t *self, ulong *num_key, char **str_key, size_t *key_len __ENV_CPARM)
{
	void *value = NULL;

	if (!self) {
		return NULL;
	}

	pthread_mutex_lock(&self->lock);
	if (self->curr && self->curr->next) {
		self->curr = self->curr->next;
		if (num_key) {
			*num_key = self->curr->num_key;
		}
		if (str_key) {
			*str_key = self->curr->str_key;
		}
		if (key_len) {
			*key_len = self->curr->key_len;
		}
		value = self->curr->value;
	}
	pthread_mutex_unlock(&self->lock);

#if (_YOD_SYSTEM_DEBUG && (_YOD_HTABLE_DEBUG & 0x02))
	yod_stdlog_debug(NULL, "%s(%p, %lu, %s, %d): %p in %s:%d %s",
		__FUNCTION__, self, (num_key ? *num_key : 0), (str_key ? *str_key : NULL),
		(key_len ? *key_len : 0), value, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return value;
}
/* }}} */


/** {{{ void *_yod_htable_prev(yod_htable_t *self, ulong *num_key, char **str_key, size_t *key_len __ENV_CPARM)
*/
void *_yod_htable_prev(yod_htable_t *self, ulong *num_key, char **str_key, size_t *key_len __ENV_CPARM)
{
	void *value = NULL;

	if (!self) {
		return NULL;
	}

	pthread_mutex_lock(&self->lock);
	if (self->curr && self->curr->prev) {
		self->curr = self->curr->prev;
		if (num_key) {
			*num_key = self->curr->num_key;
		}
		if (str_key) {
			*str_key = self->curr->str_key;
		}
		if (key_len) {
			*key_len = self->curr->key_len;
		}
		value = self->curr->value;
	}
	pthread_mutex_unlock(&self->lock);

#if (_YOD_SYSTEM_DEBUG && (_YOD_HTABLE_DEBUG & 0x02))
	yod_stdlog_debug(NULL, "%s(%p, %lu, %s, %d): %p in %s:%d %s",
		__FUNCTION__, self, (num_key ? *num_key : 0), (str_key ? *str_key : NULL),
		(key_len ? *key_len : 0), value, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return value;
}
/* }}} */


/** {{{ int _yod_htable_resize(yod_htable_t *self __ENV_CPARM)
*/
static int _yod_htable_resize(yod_htable_t *self __ENV_CPARM)
{
	yod_htable_v **nodes = NULL;
	yod_htable_v *node = NULL;
	ulong k = 0;

	if (!self) {
		return (-1);
	}

	if ((self->size << 1) <= 0) {
		return (-1);
	}

	if (self->count < self->size) {
		return (0);
	}

#if (_YOD_HTABLE_DEBUG & 0x02)
	yod_stdlog_dump(NULL,
		"----------------------------------------------------------------\n"
		"%d / %d\n"
		"----------------------------------------------------------------\n",
		self->count, self->size);

	for (k = 0; k < self->size; ++k) {
		for (node = self->nodes[k]; node != NULL; node = node->v_next) {
			yod_stdlog_dump(NULL,
				"[%04ld] => %p: "
				"{num_key=%lu, str_key=%s, key_len=%d, value=%p, "
				"next=%p, prev=%p, v_next=%p, v_prev=%p}\n",
				(ulong) k, node,
				node->num_key, node->str_key, node->key_len, node->value,
				node->next, node->prev, node->v_next, node->v_prev);
		}
	}

	yod_stdlog_dump(NULL,
		"\n----------------------------------------------------------------\n");
#endif

	nodes = realloc(self->nodes, (self->size << 1) * sizeof(yod_htable_v *));
	if (!nodes) {
		pthread_mutex_unlock(&self->lock);

		YOD_STDLOG_ERROR("realloc failed");
		return (-1);
	}

	self->size = (self->size << 1);
	self->mask = (uint32_t) (self->size - 1);
	self->nodes = nodes;

	memset(self->nodes, 0, self->size * sizeof(yod_htable_v *));
	for (node = self->head; node != NULL; node = node->next) {
		k = node->num_key & self->mask;
		node->v_next = self->nodes[k];
		node->v_prev = NULL;
		if (node->v_next) {
			node->v_next->v_prev = node;
		}
		self->nodes[k] = node;
	}

#if (_YOD_HTABLE_DEBUG & 0x02)
	yod_stdlog_dump(NULL,
		"----------------------------------------------------------------\n"
		"%d / %d\n"
		"----------------------------------------------------------------\n",
		self->count, self->size);

	for (k = 0; k < self->size; ++k) {
		for (node = self->nodes[k]; node != NULL; node = node->v_next) {
			yod_stdlog_dump(NULL,
				"[%04ld] => %p: "
				"{num_key=%lu, str_key=%s, key_len=%d, value=%p, "
				"next=%p, prev=%p, v_next=%p, v_prev=%p}\n",
				(ulong) k, node,
				node->num_key, node->str_key, node->key_len, node->value,
				node->next, node->prev, node->v_next, node->v_prev);
		}
	}

	yod_stdlog_dump(NULL,
		"\n----------------------------------------------------------------\n");
#endif

#if (_YOD_SYSTEM_DEBUG && _YOD_HTABLE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): {size=%lu, count=%lu} in %s:%d %s",
		__FUNCTION__, self, self->size, self->count, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return (0);
}
/* }}} */


/** {{{ static ulong _yod_htable_str_nkey(const char *str_key, size_t key_len)
*/
static ulong _yod_htable_str_nkey(const char *str_key, size_t key_len)
{
	register ulong num_key = 5381;

	for (; key_len >= 8; key_len -= 8) {
		num_key = ((num_key << 5) + num_key) + *str_key++;
		num_key = ((num_key << 5) + num_key) + *str_key++;
		num_key = ((num_key << 5) + num_key) + *str_key++;
		num_key = ((num_key << 5) + num_key) + *str_key++;
		num_key = ((num_key << 5) + num_key) + *str_key++;
		num_key = ((num_key << 5) + num_key) + *str_key++;
		num_key = ((num_key << 5) + num_key) + *str_key++;
		num_key = ((num_key << 5) + num_key) + *str_key++;
	}
	switch (key_len) {
		case 7: num_key = ((num_key << 5) + num_key) + *str_key++;
		case 6: num_key = ((num_key << 5) + num_key) + *str_key++;
		case 5: num_key = ((num_key << 5) + num_key) + *str_key++;
		case 4: num_key = ((num_key << 5) + num_key) + *str_key++;
		case 3: num_key = ((num_key << 5) + num_key) + *str_key++;
		case 2: num_key = ((num_key << 5) + num_key) + *str_key++;
		case 1: num_key = ((num_key << 5) + num_key) + *str_key++; break;
		case 0: break;
	}

	return num_key;
}
/* }}} */
