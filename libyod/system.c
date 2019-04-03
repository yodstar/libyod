#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#endif
#include <sys/stat.h>
#include <errno.h>

// system ignore
#define _YOD_SYSTEM_IGNORE 										0x00

#include "system.h"
#include "stdlog.h"


#ifndef _YOD_SYSTEM_DEBUG
#define _YOD_SYSTEM_DEBUG 										0
#endif

#define YOD_SYSTEM_MMNODE_MASK 									0x00444F59


#if (_YOD_SYSTEM_DEBUG && (_YOD_SYSTEM_DEBUG & 0x01) == 0)
/* yod_system_mmnode_t */
typedef struct _yod_system_mmnode
{
	ulong size;
	char *info;
	struct _yod_system_mmnode *next;
	struct _yod_system_mmnode *prev;
} yod_system_mmnode_t;


/* yod_system_memory_t */
typedef struct
{
	pthread_mutex_t lock;

	ulong alloc_bytes;
	int alloc_count;
	ulong free_bytes;
	int free_count;
	ulong valid_bytes;

	yod_system_mmnode_t *node;
} yod_system_memory_t;


/* yod_system_memory_self__ */
static yod_system_memory_t yod_system_memory_self__;
static int yod_system_memory_init__ = 0;

static yod_system_memory_t *_yod_system_memory_self();
#endif


/* yod_system_memory_used__ */
static ulong yod_system_memory_used__ = 0;


/** {{{ uint64_t yod_system_memory(short what)
*/
ulong yod_system_memory(short what)
{
#if (_YOD_SYSTEM_DEBUG & 0x02)
	yod_system_memory_t *self = &yod_system_memory_self__;
	yod_system_mmnode_t *node = NULL;
	static uint8_t nlen = 0;

	if (!self) {
		return yod_system_memory_used__;
	}

	if (what == 1) {
		yod_stdlog_dump(NULL,
			"================================================================\n"
			"total alloc %lu bytes in %d blocks\n"
			"total free %lu bytes in %d blocks\n"
			"total leaks %lu bytes in %d blocks\n"
			"total used %lu bytes (max)\n"
			"----------------------------------------------------------------\n",
			self->alloc_bytes, self->alloc_count,
			self->free_bytes, self->free_count,
			(self->alloc_bytes - self->free_bytes), (self->alloc_count - self->free_count),
			self->valid_bytes);

		if (self->alloc_count > self->free_count) {
			for (node = self->node; node != NULL; node = node->next) {
				yod_stdlog_dump(NULL, "%p leaks %lu bytes in %s\n",
					(node + nlen), node->size, node->info);
			}
			yod_stdlog_dump(NULL, "----------------------------------------------------------------\n");
		}

	}
	else if (what == 2) {
		yod_stdlog_dump(NULL,
			"================================================================\n"
			"total alloc %lu bytes in %d blocks\n"
			"total free %lu bytes in %d blocks\n"
			"total used %lu bytes in %d blocks\n"
			"----------------------------------------------------------------\n",
			self->alloc_bytes, self->alloc_count,
			self->free_bytes, self->free_count,
			(self->alloc_bytes - self->free_bytes), (self->alloc_count - self->free_count));
	}

#endif

	return yod_system_memory_used__;

	(void) what;
}
/* }}} */


#if (_YOD_SYSTEM_DEBUG)
/** {{{ void *_yod_system_malloc(size_t size __ENV_CPARM)
*/
void *_yod_system_malloc(size_t size __ENV_CPARM)
{
#if (_YOD_SYSTEM_DEBUG & 0x01)
	char *ret = NULL;
	if ((ret = (char *) malloc((size + 8) * sizeof(char))) != NULL) {
		*((uint32_t *) ret) = YOD_SYSTEM_MMNODE_MASK;
		*((uint32_t *) (ret + 4)) = (uint32_t) (size + 8);
		yod_system_memory_used__ += (ulong) (size + 8);
		ret += 8;
	}
	return (void *) ret;
#else
	yod_system_memory_t *self = NULL;
	yod_system_mmnode_t *node = NULL;
	char *ret = NULL;
	char info[256];
	int snp = 0;

	self = _yod_system_memory_self();
	if (!self) {
		return NULL;
	}

	pthread_mutex_lock(&self->lock);

	if ((snp = snprintf(info, sizeof(info), "%s:%d %s", __ENV_TRACE)) == -1) {
		snp = 0;
	}

	if ((ret = (char *) malloc(size + 5 + snp + sizeof(yod_system_mmnode_t))) != NULL) {
		*((uint32_t *) ret) = YOD_SYSTEM_MMNODE_MASK;

		node = (yod_system_mmnode_t *) (ret + 4);
		node->size = (ulong) size;
		node->info = (char *) (ret + size + 4 + sizeof(yod_system_mmnode_t));
		memcpy(node->info, info, snp + 1);
		node->next = self->node;
		node->prev = NULL;
		if (node->next) {
			node->next->prev = node;
		}

		self->alloc_bytes += (ulong) node->size;
		++ self->alloc_count;
		if (self->alloc_bytes - self->free_bytes > self->valid_bytes) {
			self->valid_bytes = self->alloc_bytes - self->free_bytes;
		}
		self->node = node;

		yod_system_memory_used__ += node->size;

		ret += 4 + sizeof(yod_system_mmnode_t);
	}

	pthread_mutex_unlock(&self->lock);

#if (_YOD_SYSTEM_DEBUG & 0x04)
	yod_common_debug(NULL, "%s(%d): %p in %s:%d %s",
		__FUNCTION__, size, ret, __ENV_TRACE);
#endif

	return (void *) ret;
#endif

	__ENV_VOID
}
/* }}} */


/** {{{ void *_yod_system_calloc(size_t num, size_t size __ENV_CPARM)
*/
void *_yod_system_calloc(size_t num, size_t size __ENV_CPARM)
{
#if (_YOD_SYSTEM_DEBUG & 0x01)
	char *ret = NULL;
	if ((ret = (char *) calloc((num * size) + 8, sizeof(char))) != NULL) {
		*((uint32_t *) ret) = YOD_SYSTEM_MMNODE_MASK;
		*((uint32_t *) (ret + 4)) = (uint32_t) ((num * size) + 8);
		yod_system_memory_used__ += (ulong) (num * size) + 8;
		ret += 8;
	}
	return (void *) ret;
#else
	yod_system_memory_t *self = NULL;
	yod_system_mmnode_t *node = NULL;
	char *ret = NULL;
	char info[256];
	int snp = 0;

	self = _yod_system_memory_self();
	if (!self) {
		return NULL;
	}

	pthread_mutex_lock(&self->lock);

	if ((snp = snprintf(info, sizeof(info), "%s:%d %s", __ENV_TRACE)) == -1) {
		snp = 0;
	}

	if ((ret = (char *) calloc((num * size) + 5 + snp + sizeof(yod_system_mmnode_t), sizeof(char))) != NULL) {
		*((uint32_t *) ret) = YOD_SYSTEM_MMNODE_MASK;

		node = (yod_system_mmnode_t *) (ret + 4);
		node->size = (ulong) (num * size);
		node->info = (char *) (ret + node->size + 4 + sizeof(yod_system_mmnode_t));
		memcpy(node->info, info, snp + 1);
		node->next = self->node;
		node->prev = NULL;
		if (node->next) {
			node->next->prev = node;
		}

		self->alloc_bytes += (ulong) node->size;
		++ self->alloc_count;
		if (self->alloc_bytes - self->free_bytes > self->valid_bytes) {
			self->valid_bytes = self->alloc_bytes - self->free_bytes;
		}
		self->node = node;

		yod_system_memory_used__ += node->size;

		ret += 4 + sizeof(yod_system_mmnode_t);
	}

	pthread_mutex_unlock(&self->lock);

#if (_YOD_SYSTEM_DEBUG & 0x04)
	yod_common_debug(NULL, "%s(%d, %d): %p in %s:%d %s",
		__FUNCTION__, num, size, ret, __ENV_TRACE);
#endif

	return (void *) ret;
#endif

	__ENV_VOID
}
/* }}} */


/** {{{ void *_yod_system_realloc(void *ptr, size_t size __ENV_CPARM)
*/
void *_yod_system_realloc(void *ptr, size_t size __ENV_CPARM)
{
#if (_YOD_SYSTEM_DEBUG & 0x01)
	char *ret = NULL;
	if (!ptr) {
		ret = (char *) realloc(NULL, (size + 8) * sizeof(char));
		yod_system_memory_used__ += (ulong) size;
	} else {
		if (*((uint32_t *) ((char *) ptr - 8)) != YOD_SYSTEM_MMNODE_MASK) {
			yod_common_error(NULL, "realloc failed, errno=%d in %s:%d %s",
				__ENV_ERRNO, __ENV_TRACE);
			return NULL;
		}
		ret = (char *) realloc((char *) ptr - 8, (size + 8) * sizeof(char));
		yod_system_memory_used__ += (ulong) (size - *((uint32_t *) (ret + 4)));
	}
	if (ret != NULL) {
		*((uint32_t *) ret) = YOD_SYSTEM_MMNODE_MASK;
		*((uint32_t *) (ret + 4)) = (uint32_t) (size + 8);
		ret += 8;
	}
	return (void *) ret;
#else
	yod_system_memory_t *self = NULL;
	yod_system_mmnode_t *node = NULL;
	char *ret = NULL;
	char info[256];
	int snp = 0;

	self = _yod_system_memory_self();
	if (!self) {
		return NULL;
	}

	pthread_mutex_lock(&self->lock);

	if ((snp = snprintf(info, sizeof(info), "%s:%d %s", __ENV_TRACE)) == -1) {
		snp = 0;
	}

	if (!ptr) {
		ret = (char *) realloc(NULL, size + 5 + snp + sizeof(yod_system_mmnode_t));
	} else {
		ret = (char *) ptr - 4 - sizeof(yod_system_mmnode_t);
		if (*((uint32_t *) ret) != YOD_SYSTEM_MMNODE_MASK) {
			pthread_mutex_unlock(&self->lock);

			yod_common_error(NULL, "realloc failed, errno=%d in %s:%d %s",
				__ENV_ERRNO, __ENV_TRACE);
			return NULL;
		}
		ret = (char *) realloc(ret, size + 5 + snp + sizeof(yod_system_mmnode_t));
	}

	if (ret != NULL) {
		*((uint32_t *) ret) = YOD_SYSTEM_MMNODE_MASK;

		node = (yod_system_mmnode_t *) (ret + 4);
		if (!ptr) {
			node->size = 0;
			node->info = NULL;
			node->next = self->node;
			node->prev = NULL;
			if (node->next) {
				node->next->prev = node;
			}
			self->node = node;
			self->alloc_count ++;
		}

		if (size < node->size) {
			self->free_bytes += (ulong) (node->size - size);
		} else {
			self->alloc_bytes += (ulong) (size - node->size);
			yod_system_memory_used__ += (ulong) (size - node->size);
		}
		if (self->alloc_bytes - self->free_bytes > self->valid_bytes) {
			self->valid_bytes = self->alloc_bytes - self->free_bytes;
		}

		node->size = (ulong) size;
		node->info = (char *) (ret + size + 4 + sizeof(yod_system_mmnode_t));
		memcpy(node->info, info, snp + 1);
		if (node->next) {
			node->next->prev = node;
		}
		if (node->prev) {
			node->prev->next = node;
		} else {
			self->node = node;
		}

		ret += 4 + sizeof(yod_system_mmnode_t);
	}

	pthread_mutex_unlock(&self->lock);

#if (_YOD_SYSTEM_DEBUG & 0x04)
	yod_common_debug(NULL, "%s(%p, %d): %p in %s:%d %s",
		__FUNCTION__, ptr, size, ret, __ENV_TRACE);
#endif

	return (void *) ret;
#endif

	__ENV_VOID
}
/* }}} */


/** {{{ char *_yod_system_strndup(const char *str, size_t len __ENV_CPARM)
*/
char *_yod_system_strndup(const char *str, size_t len __ENV_CPARM)
{
#if (_YOD_SYSTEM_DEBUG & 0x01)
	char *ret = NULL;
	if ((ret = (char *) malloc((len + 9) * sizeof(char))) != NULL) {
		*((uint32_t *) ret) = YOD_SYSTEM_MMNODE_MASK;
		*((uint32_t *) (ret + 4)) = (uint32_t) (len + 9);
		yod_system_memory_used__ += (ulong) (len + 9);
		ret += 8;
		memcpy(ret, str, len);
		ret[len] = '\0';
	}
	return ret;
#else
	yod_system_memory_t *self = NULL;
	yod_system_mmnode_t *node = NULL;
	size_t size = 0;
	char *ret = NULL;
	char info[256];
	int snp = 0;

	self = _yod_system_memory_self();
	if (!self) {
		return NULL;
	}

	pthread_mutex_lock(&self->lock);

	size = (len + 1) * sizeof(char);

	if ((snp = snprintf(info, sizeof(info), "%s:%d %s", __ENV_TRACE)) == -1) {
		snp = 0;
	}

	if ((ret = (char *) malloc(size + 5 + snp + sizeof(yod_system_mmnode_t))) != NULL) {
		*((uint32_t *) ret) = YOD_SYSTEM_MMNODE_MASK;
		
		node = (yod_system_mmnode_t *) (ret + 4);
		node->size = (ulong) len;
		node->info = (char *) (ret + size + 4 + sizeof(yod_system_mmnode_t));
		memcpy(node->info, info, snp + 1);
		node->next = self->node;
		node->prev = NULL;
		if (node->next) {
			node->next->prev = node;
		}

		self->alloc_bytes += (ulong) node->size;
		++ self->alloc_count;
		if (self->alloc_bytes - self->free_bytes > self->valid_bytes) {
			self->valid_bytes = self->alloc_bytes - self->free_bytes;
		}
		self->node = node;

		yod_system_memory_used__ += node->size;

		ret += 4 + sizeof(yod_system_mmnode_t);
		memcpy(ret, str, len);
		ret[len] = '\0';

	}

	pthread_mutex_unlock(&self->lock);

#if (_YOD_SYSTEM_DEBUG & 0x04)
	yod_common_debug(NULL, "%s(%p, %d): %p in %s:%d %s",
		__FUNCTION__, str, len, ret, __ENV_TRACE);
#endif

	return (void *) ret;
#endif

	__ENV_VOID
}
/* }}} */


/** {{{ int _yod_system_assert(void *ptr __ENV_CPARM)
*/
int _yod_system_assert(void *ptr __ENV_CPARM)
{
	uint32_t mask = 0;
	int ret = 0;

	if (!ptr) {
		return (0);
	}

#if (_YOD_SYSTEM_DEBUG & 0x01)
	mask = *((uint32_t *) ((char *) ptr - 8));
#else
	mask = *((uint32_t *) ((char *) ptr - 4 - sizeof(yod_system_mmnode_t)));
#endif

	if (mask != YOD_SYSTEM_MMNODE_MASK) {
		yod_common_error(NULL, "memory failed, (%p): [0x%08X] in %s:%d %s",
			ptr, mask, __ENV_TRACE);
		ret = mask;
	}

	return ret;
}
/* }}} */


#if ((_YOD_SYSTEM_DEBUG & 0x01) == 0)
/** {{{ static yod_system_memory_t *_yod_system_memory_self()
*/
static yod_system_memory_t *_yod_system_memory_self()
{
	yod_system_memory_t *self = &yod_system_memory_self__;

	if (!yod_system_memory_init__) {
		yod_system_memory_init__ = 1;

		self->alloc_bytes = 0;
		self->alloc_count = 0;
		self->free_bytes = 0;
		self->free_count = 0;
		self->valid_bytes = 0;
		self->node = NULL;
	}

	if (yod_system_memory_init__ < 2) {
		if (pthread_mutex_init(&self->lock, NULL) != 0) {
			yod_system_memory_init__ = 0;
			return NULL;
		}
		yod_system_memory_init__ = 2;
	}

	return self;
}
/* }}} */
#endif
#endif


/** {{{ void _yod_system_free(void *ptr __ENV_CPARM)
*/
void _yod_system_free(void *ptr __ENV_CPARM)
{
#if (_YOD_SYSTEM_DEBUG & 0x01)
	char *ret = NULL;
	if (!ptr) {
		yod_common_error(NULL, "null pointer, errno=%d in %s:%d %s",
			__ENV_ERRNO, __ENV_TRACE);
		return;
	}
	ret = ((char *) ptr - 8);
	if (*((uint32_t *) ret) != YOD_SYSTEM_MMNODE_MASK) {
		yod_common_error(NULL, "free failed, errno=%d in %s:%d %s",
			__ENV_ERRNO, __ENV_TRACE);
	}
	yod_system_memory_used__ -= *((uint32_t *) (ret + 4));
	*((uint32_t *) (ret + 4)) = 0;
	free(ret);
#elif (_YOD_SYSTEM_DEBUG)
	yod_system_memory_t *self = NULL;
	yod_system_mmnode_t *node = NULL;
	size_t size = 0;
	char *ret = NULL;

	if (!ptr) {
		yod_common_error(NULL, "null pointer, errno=%d in %s:%d %s",
			__ENV_ERRNO, __ENV_TRACE);
		return;
	}

	self = _yod_system_memory_self();
	if (!self) {
		free(ptr);
		return;
	}

	pthread_mutex_lock(&self->lock);

	ret = (char *) ptr - 4 - sizeof(yod_system_mmnode_t);
	if (*((uint32_t *) ret) != YOD_SYSTEM_MMNODE_MASK) {
		pthread_mutex_unlock(&self->lock);
		yod_common_error(NULL, "free failed, errno=%d in %s:%d %s",
			__ENV_ERRNO, __ENV_TRACE);
		free(ptr);
		return;
	}
	*((uint32_t *) ret) = 0;

	node = (yod_system_mmnode_t *) (ret + 4);
	size = node->size;
	if (node->next) {
		node->next->prev = node->prev;
	}
	if (node->prev) {
		node->prev->next = node->next;
	} else {
		self->node = node->next;
	}

	self->free_bytes += (ulong) node->size;
	++ self->free_count;
	if (self->alloc_bytes - self->free_bytes > self->valid_bytes) {
		self->valid_bytes = self->alloc_bytes - self->free_bytes;
	}

	yod_system_memory_used__ -= node->size;

	if (!self->node) {
		if (yod_system_memory_init__ > 1) {
			yod_system_memory_init__ = 1;
			pthread_mutex_unlock(&self->lock);
			pthread_mutex_destroy(&self->lock);
		}
		free(ret);
		return;
	}

#ifdef _CYGWIN
	printf("<%s> in %s:%d %s\n", node->info, __ENV_TRACE);
#endif

	free(ret);

	pthread_mutex_unlock(&self->lock);

#if (_YOD_SYSTEM_DEBUG & 0x04)
	yod_common_debug(NULL, "%s(%p): %d in %s:%d %s",
		__FUNCTION__, ptr, size, __ENV_TRACE);
#endif

	if (yod_system_memory_init__ < 2) {
		yod_system_memory(1);
	}

	(void) size;

#else
	free(ptr);
#endif

	__ENV_VOID
}
/* }}} */


#ifdef _WIN32
/** {{{ char *_yod_system_strndup_s(const char *str, size_t len)
*/
char *_yod_system_strndup_s(const char *str, size_t len)
{
	char *ret = NULL;
#if (_YOD_SYSTEM_DEBUG)
	ret = (char *) _yod_system_malloc((len + 1) * sizeof(char) __ENV_CARGS);
#else
	ret = (char *) malloc((len + 1) * sizeof(char));
#endif
	if (!ret) {
		yod_common_error(NULL, "malloc pointer, errno=%d in %s:%d %s",
			__ENV_ERRNO, __ENV_TRACE3);
		return NULL;
	}
	memcpy(ret, str, len);
	ret[len] = '\0';
	return ret;
}
/* }}} */
#endif
