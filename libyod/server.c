#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
	#include <winsock2.h>
	#include <process.h>
	#include <time.h>
#else
	#include <sys/time.h>
	#include <unistd.h>
#endif
#include <errno.h>

#include "stdlog.h"
#include "thread.h"
#include "server.h"


#ifndef _YOD_SERVER_DEBUG
#define _YOD_SERVER_DEBUG 										0
#endif

#define YOD_SERVER_LOOP_TICK 									100
#define YOD_SERVER_DATA_STEP 									256
#define YOD_SERVER_DATA_SIZE 									512


enum
{
	YOD_SERVER_STATE_IDLE,
	YOD_SERVER_STATE_LISTENING,
	YOD_SERVER_STATE_STOPING,
};


/* yod_server_t */
struct _yod_server_t
{
	pthread_mutex_t lock;

	ulong count;

	yod_socket_t fd;
	short what;
	yod_server_fn func;
	void *arg;
	uint32_t tick;
	uint64_t msec;

	struct
	{
		byte *ptr;
		int len;
		int size;
	} data;

	yod_evloop_t *evloop;
	yod_thread_t *thread;

	yod_server_t *root;
	yod_server_t *next;
	yod_server_t *prev;
	yod_server_t *heap;
};


#define yod_server_open(x, d, w, f, a, t) 						_yod_server_open(x, d, w, f, a, t __ENV_CARGS)
#define yod_server_destroy(x) 									_yod_server_destroy(x __ENV_CARGS)
#define yod_server_ref(x) 										{ pthread_mutex_lock(&(x)->lock); ++ (x)->count; pthread_mutex_unlock(&(x)->lock); }


static yod_server_t *_yod_server_open(yod_server_t *self, yod_socket_t fd, short what, yod_server_fn func, void *arg, uint32_t tick __ENV_CPARM);
static void _yod_server_destroy(yod_server_t *self __ENV_CPARM);

static void _yod_server_accept_cb(yod_evloop_t *evloop, yod_socket_t fd, short what, void *arg __ENV_CPARM);
static void _yod_server_connect_cb(yod_evloop_t *evloop, yod_socket_t fd, short what, void *arg __ENV_CPARM);
static void _yod_server_handle_cb(yod_evloop_t *evloop, yod_socket_t fd, short what, void *arg __ENV_CPARM);
static void _yod_server_input_cb(yod_evloop_t *evloop, yod_socket_t fd, short what, void *arg __ENV_CPARM);
#ifdef _WIN32
static unsigned __stdcall _yod_server_tick_cb(void *arg);
#else
static void *_yod_server_tick_cb(void *arg);
#endif


/* yod_server_init__ */
static int yod_server_init__ = 0;


/** {{{ yod_server_t *_yod_server_new(int thread_num __ENV_CPARM)
*/
yod_server_t *_yod_server_new(int thread_num __ENV_CPARM)
{
	yod_server_t *self = NULL;
	pthread_t thread;
#ifdef _WIN32
	HANDLE handle = 0;
#else
	pthread_attr_t thattr;
#endif

	if (!yod_server_init__) {
		yod_server_init__ = 1;
	}

	self = (yod_server_t *) malloc(sizeof(yod_server_t));
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
		self->count = 0;

		self->fd = -1;
		self->what = 0;
		self->func = NULL;
		self->arg = NULL;
		self->tick = 0;
		self->msec = 0;

		self->data.ptr = NULL;
		self->data.len = 0;
		self->data.size = 0;

		self->evloop = NULL;
		self->thread = NULL;

		self->root = self;
		self->next = NULL;
		self->prev = NULL;
		self->heap = NULL;
	}

	yod_socket_init();

	// evloop
	self->evloop = yod_evloop_new();
	if (!self->evloop) {
		yod_server_free(self);

		YOD_STDLOG_ERROR("evloop_new failed");
		return NULL;
	}

	// thread
	self->thread = yod_thread_new(thread_num);
	if (!self->thread) {
		yod_server_free(self);

		YOD_STDLOG_ERROR("thread_new failed");
		return NULL;
	}

#ifdef _WIN32
	handle = (HANDLE) _beginthreadex(NULL, 0, _yod_server_tick_cb, self, 0, &thread);
	if (handle == 0) {
		yod_server_free(self);

		YOD_STDLOG_ERROR("_beginthreadex failed");
		return NULL;
	}
	CloseHandle(handle);
#else
	pthread_attr_init(&thattr);
	if (pthread_attr_setdetachstate(&thattr, PTHREAD_CREATE_DETACHED) != 0) {
		yod_server_free(self);

		YOD_STDLOG_ERROR("pthread_attr_setdetachstate failed");
		return NULL;
	}
	if (pthread_create(&thread, &thattr, _yod_server_tick_cb, self) == -1) {
		yod_server_free(self);

		pthread_attr_destroy(&thattr);
		YOD_STDLOG_ERROR("pthread_create failed");
		return NULL;
	}
	pthread_attr_destroy(&thattr);
#endif

#if (_YOD_SYSTEM_DEBUG && _YOD_SERVER_DEBUG)
	yod_stdlog_debug(NULL, "%s(%d): %p in %s:%d %s",
		__FUNCTION__, thread_num, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return self;
}
/* }}} */


/** {{{ void _yod_server_free(yod_server_t *self __ENV_CPARM)
*/
void _yod_server_free(yod_server_t *self __ENV_CPARM)
{
	yod_server_t *root = NULL;

#if (_YOD_SYSTEM_DEBUG && _YOD_SERVER_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p) in %s:%d %s",
		__FUNCTION__, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (!self || !self->root) {
		return;
	}
	root = self->root;

	yod_server_init__ = 0;

	if (root->what == YOD_SERVER_STATE_LISTENING) {
		yod_server_stop(root);
	}

	pthread_mutex_lock(&root->lock);

	if (root->evloop) {
		yod_evloop_free(root->evloop);
	}

	if (root->thread) {
		yod_thread_free(root->thread);
	}

	while ((self = root->next) != NULL) {
		root->next = self->next;
		if (self->fd > 0) {
			yod_socket_close(self->fd);
		}
		if (self->data.ptr) {
			free(self->data.ptr);
		}
		free(self);
	}

	while ((self = root->heap) != NULL) {
		root->heap = self->heap;
		if (self->data.ptr) {
			free(self->data.ptr);
		}
		free(self);
	}

	pthread_mutex_unlock(&root->lock);
	pthread_mutex_destroy(&root->lock);

	free(root);
}
/* }}} */


/** {{{ void _yod_server_start(yod_server_t *self __ENV_CPARM)
*/
void _yod_server_start(yod_server_t *self __ENV_CPARM)
{
	yod_server_t *root = NULL;

#if (_YOD_SYSTEM_DEBUG && _YOD_SERVER_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p) in %s:%d %s",
		__FUNCTION__, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (!self || !self->root) {
		return;
	}
	root = self->root;

	if (root->what == YOD_SERVER_STATE_LISTENING) {
		YOD_STDLOG_WARN("server listening");
		return;
	}
	root->what = YOD_SERVER_STATE_LISTENING;

	yod_evloop_start(root->evloop, 0);
}
/* }}} */


/** {{{ void _yod_server_stop(yod_server_t *self __ENV_CPARM)
*/
void _yod_server_stop(yod_server_t *self __ENV_CPARM)
{
	yod_server_t *root = NULL;

	if (!self || !self->root) {
		return;
	}
	root = self->root;

	if (root->what == YOD_SERVER_STATE_STOPING) {
		YOD_STDLOG_WARN("server stoping");
		return;
	}
	root->what = YOD_SERVER_STATE_STOPING;

#if (_YOD_SYSTEM_DEBUG && _YOD_SERVER_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p) in %s:%d %s",
		__FUNCTION__, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	yod_evloop_stop(root->evloop);
}
/* }}} */


/** {{{ int _yod_server_listen(yod_server_t *self, const char *server_ip, uint16_t port,
	yod_server_fn func, void *arg, uint32_t timeout __ENV_CPARM)
*/
int _yod_server_listen(yod_server_t *self, const char *server_ip, uint16_t port,
	yod_server_fn func, void *arg, uint32_t timeout __ENV_CPARM)
{
	yod_server_t *root = NULL;
	yod_socket_t fd = 0;
	int ret = -1;

#if (_YOD_SYSTEM_DEBUG && _YOD_SERVER_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %s, %d, %p, %p, %d) in %s:%d %s",
		__FUNCTION__, self, server_ip, port, func, arg, timeout, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (!self || !self->root || !server_ip) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return (-1);
	}
	root = self->root;

	if ((fd = yod_socket_listen(server_ip, port)) != INVALID_SOCKET) {
		if ((self = yod_server_open(root, fd, 0, func, arg, timeout)) != NULL) {
			yod_socket_set_reuseable(fd);
			yod_socket_set_nonblock(fd);
			
			if ((ret = yod_evloop_add(root->evloop, fd, __EVL_READ, _yod_server_accept_cb, self, NULL)) != 0) {
				yod_server_destroy(self);
			}
		}
	}

	return ret;
}
/* }}} */


/** {{{ int _yod_server_connect(yod_server_t *self, const char *server_ip, uint16_t port,
	yod_server_fn func, void *arg, uint32_t timeout __ENV_CPARM)
*/
int _yod_server_connect(yod_server_t *self, const char *server_ip, uint16_t port,
	yod_server_fn func, void *arg, uint32_t timeout __ENV_CPARM)
{
	yod_server_t *root = NULL;
	yod_socket_t fd = 0;
	int ret = -1;

#if (_YOD_SYSTEM_DEBUG && _YOD_SERVER_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %s, %hu, %p, %p, %u) in %s:%d %s",
		__FUNCTION__, self, server_ip, port, func, arg, timeout, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (!self || !self->root || !server_ip) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return (-1);
	}
	root = self->root;

	if ((fd = yod_socket_connect(server_ip, port)) != INVALID_SOCKET) {
		if ((self = yod_server_open(root, fd, __EVS_CONNECT, func, arg, timeout)) != NULL) {
			yod_socket_set_nonblock(fd);
			yod_socket_set_nodelay(fd);

			if ((ret = yod_thread_run(root->thread, _yod_server_connect_cb, self)) != 0) {
				yod_server_destroy(self);
			}
		}
	}

	return ret;
}
/* }}} */


/** {{{ int _yod_server_tick(yod_server_t *self, yod_server_fn func, void *arg, uint32_t tick __ENV_CPARM)
*/
int _yod_server_tick(yod_server_t *self, yod_server_fn func, void *arg, uint32_t tick __ENV_CPARM)
{

#if (_YOD_SYSTEM_DEBUG && _YOD_SERVER_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %p, %p, %u) in %s:%d %s",
		__FUNCTION__, self, func, arg, tick, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (!self || !self->root) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return (-1);
	}

	if (yod_server_open(self, -1, __EVS_TIMEOUT, func, arg, tick) != NULL) {
		return (0);
	}

	return (-1);
}
/* }}} */


/** {{{ byte *_yod_server_recv(yod_server_t *self, int *len __ENV_CPARM)
*/
byte *_yod_server_recv(yod_server_t *self, int *len __ENV_CPARM)
{
	byte *ret = NULL;

	if (!self || !self->root || !self->evloop) {
		errno = EINVAL;
		return NULL;
	}

	if (len) {
		*len = self->data.len;
	}
	ret = self->data.ptr;

#if (_YOD_SYSTEM_DEBUG && _YOD_SERVER_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d): %p in %s:%d %s",
		__FUNCTION__, self, (len ? *len : -1), ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ int _yod_server_send(yod_server_t *self, byte *data, int len __ENV_CPARM)
*/
int _yod_server_send(yod_server_t *self, byte *data, int len __ENV_CPARM)
{
	char *ptr = NULL;
#ifdef _WIN32
	int num = 5000;
#else
	int num = 10000;
#endif
	int ret = -1;

#if (_YOD_SYSTEM_DEBUG && _YOD_SERVER_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %p, %d) in %s:%d %s",
		__FUNCTION__, self, data, len, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (!self || !self->root) {
		errno = EINVAL;
		return (-1);
	}

	if (self->fd > 0) {
		ptr = (char *) data;
		while (len > 0) {
			if ((ret = yod_socket_send(self->fd, ptr, len)) < 0) {
				yod_server_close(self);
				break;
			}
			ptr += ret;
			len -= ret;
			if (--num < 0) {
				YOD_STDLOG_WARN("send failed");
				break;
			}
#ifdef _WIN32
			Sleep(1);
#else
			usleep(500); 
#endif
			if (!self->evloop) {
				break;
			}
		}
		if (len == 0) {
			ret = 0;
		}
	}
	return ret;
}
/* }}} */


/** {{{ void _yod_server_close(yod_server_t *self __ENV_CPARM)
*/
void _yod_server_close(yod_server_t *self __ENV_CPARM)
{
	if (!self || !self->root) {
		return;
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_SERVER_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p) in %s:%d %s",
		__FUNCTION__, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (self->root == self) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return;
	}

	self->what = 0;

	if (self->evloop) {
		yod_evloop_del(self->evloop, __EVL_ALL);
		self->evloop = NULL;
	}
}
/* }}} */


/** {{{ int _yod_server_setcb(yod_server_t *self, yod_server_fn func, void *arg __ENV_CPARM)
*/
int _yod_server_setcb(yod_server_t *self, yod_server_fn func, void *arg __ENV_CPARM)
{
#if (_YOD_SYSTEM_DEBUG && _YOD_SERVER_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %p, %p) in %s:%d %s",
		__FUNCTION__, self, func, arg, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (!self || !self->root) {
		errno = EINVAL;
		return (-1);
	}

	if (self == self->root) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid server");
		return (-1);
	}

	self->func = func;
	self->arg = arg;

	return (0);
}
/* }}} */


/** {{{ ulong _yod_server_count(yod_server_t *self __ENV_CPARM)
*/
ulong _yod_server_count(yod_server_t *self __ENV_CPARM)
{
	if (!self || !self->root) {
		return 0;
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_THREAD_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): %lu in %s:%d %s",
		__FUNCTION__, self, self->root->count, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return self->root->count;
}
/* }}} */


/** {{{ char *_yod_server_dump(yod_server_t *self)
*/
char *_yod_server_dump(yod_server_t *self)
{
	char *ret = NULL;

	return ret;

	(void) self;
}
/* }}} */


/** {{{ static yod_server_t *_yod_server_open(yod_server_t *self, yod_socket_t fd, short what,
	yod_server_fn func, void *arg, uint32_t tick __ENV_CPARM)
*/
static yod_server_t *_yod_server_open(yod_server_t *self, yod_socket_t fd, short what,
	yod_server_fn func, void *arg, uint32_t tick __ENV_CPARM)
{
	yod_server_t *root = NULL;

	if (!self || !self->root) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return NULL;
	}
	root = self->root;

	pthread_mutex_lock(&root->lock);
	{
		if ((self = root->heap) != NULL) {
			root->heap = self->heap;
		}
		else {
			self = (yod_server_t *) malloc(sizeof(yod_server_t));
			if (!self) {
				pthread_mutex_unlock(&root->lock);

				YOD_STDLOG_ERROR("malloc failed");
				return NULL;
			}

			if (pthread_mutex_init(&self->lock, NULL) != 0) {
				pthread_mutex_unlock(&root->lock);
				free(self);

				YOD_STDLOG_ERROR("pthread_mutex_init failed");
				return NULL;
			}

			self->data.ptr = NULL;
			self->data.size = 0;

			++ root->tick;
		}

		self->count = 1;
		self->fd = fd;
		self->what = what;
		if (what != 0 && tick != 0) {
			self->what |= __EVS_TIMEOUT;
		}
		self->func = func;
		self->arg = arg;
		self->tick = tick;
		self->msec = yod_common_nowtime() + tick;

		self->data.len = 0;

		self->evloop = NULL;
		self->thread = NULL;

		self->root = root;
		self->next = root->next;
		self->prev = NULL;
		self->heap = NULL;
		if (self->next) {
			self->next->prev = self;
		}
		root->next = self;
		++ root->count;
	}
	pthread_mutex_unlock(&root->lock);

#if (_YOD_SYSTEM_DEBUG && _YOD_SERVER_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d, %p, %p, %p, %u): %p in %s:%d %s",
		__FUNCTION__, self, fd, what, func, arg, tick, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return self;
}
/* }}} */


/** {{{ static void _yod_server_destroy(yod_server_t *self __ENV_CPARM)
*/
static void _yod_server_destroy(yod_server_t *self __ENV_CPARM)
{
	yod_server_t *root = NULL;

	if (!self || !self->root) {
		return;
	}
	root = self->root;

	if (self == root) {
		errno = EINVAL;
		return;
	}

	pthread_mutex_lock(&self->lock);
	{
		if (self->count > 0) {
			-- self->count;
		}

		if (self->count == 0) {
			pthread_mutex_lock(&root->lock);

			if (self->next) {
				self->next->prev = self->prev;
			}
			if (self->prev) {
				self->prev->next = self->next;
			} else {
				root->next = self->next;
			}
			-- root->count;

			self->fd = 0;
			self->what = 0;
			self->func = NULL;
			self->arg = NULL;

			self->evloop = NULL;
			self->thread = NULL;

			self->heap = root->heap;
			root->heap = self;

#if (_YOD_SYSTEM_DEBUG && _YOD_SERVER_DEBUG)
			yod_stdlog_debug(NULL, "%s(%p) in %s:%d %s",
				__FUNCTION__, self, __ENV_TRACE);
#else
			__ENV_VOID
#endif

			pthread_mutex_unlock(&root->lock);
		}
	}
	pthread_mutex_unlock(&self->lock);

}
/* }}} */


/** {{{ static void _yod_server_accept_cb(yod_evloop_t *evloop, yod_socket_t fd,
	short what, void *arg __ENV_CPARM)
*/
static void _yod_server_accept_cb(yod_evloop_t *evloop, yod_socket_t fd,
	short what, void *arg __ENV_CPARM)
{
	yod_server_t *root = NULL;
	yod_server_t *self = NULL;
	yod_server_t *conn = NULL;
	yod_socket_t sockfd = 0;

#if (_YOD_SYSTEM_DEBUG && _YOD_SERVER_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d, 0x%02X, %p) in %s:%d %s",
		__FUNCTION__, evloop, fd, what, arg, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (!evloop || !fd || !what) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return;
	}

	if ((what & __EVL_READ) == 0) {
		return;
	}

	self = (yod_server_t *) arg;
	if (!self || !self->root) {
		return;
	}
	root = self->root;

	while ((sockfd = yod_socket_accept(fd)) > 0) {
		conn = yod_server_open(root, sockfd, __EVS_CONNECT, self->func, self->arg, self->tick);
		if (conn && (yod_thread_run(root->thread, _yod_server_connect_cb, conn) != 0)) {
			yod_server_destroy(conn);
		}
	}

}
/* }}} */


/** {{{ static void _yod_server_connect_cb(yod_evloop_t *evloop, yod_socket_t fd,
	short what, void *arg __ENV_CPARM)
*/
static void _yod_server_connect_cb(yod_evloop_t *evloop, yod_socket_t fd,
	short what, void *arg __ENV_CPARM)
{
	yod_server_t *root = NULL;
	yod_server_t *self = NULL;

#if (_YOD_SYSTEM_DEBUG && _YOD_SERVER_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d, 0x%02X, %p) in %s:%d",
		__FUNCTION__, evloop, fd, what, arg, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (!evloop || !fd || !what) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return;
	}

	self = (yod_server_t *) arg;
	if (!self || !self->root) {
		YOD_STDLOG_WARN("invalid argument");
		return;
	}
	root = self->root;

	if (root->what == YOD_SERVER_STATE_STOPING) {
		yod_server_destroy(self);
		YOD_STDLOG_WARN("server stoping");
		return;
	}

	/* data */
	if (!self->data.ptr) {
		self->data.ptr = (byte *) malloc(YOD_SERVER_DATA_STEP * sizeof(byte));
		self->data.len = 0;
		self->data.size = YOD_SERVER_DATA_STEP;
	}

	yod_socket_set_nonblock(self->fd);
	yod_socket_set_nodelay(self->fd);

	if (yod_evloop_add(evloop, self->fd, __EVL_READ, _yod_server_handle_cb, self, &self->evloop) != 0) {
		yod_server_destroy(self);
		return;
	}

	if (self->func) {
		self->func(self, self->fd, __EVS_CONNECT, self->arg __ENV_CARGS);
	}

}
/* }}} */


/** {{{ static void _yod_server_handle_cb(yod_evloop_t *evloop, yod_socket_t fd,
	short what, void *arg __ENV_CPARM)
*/
static void _yod_server_handle_cb(yod_evloop_t *evloop, yod_socket_t fd,
	short what, void *arg __ENV_CPARM)
{
	yod_server_t *root = NULL;
	yod_server_t *self = NULL;

#if (_YOD_SYSTEM_DEBUG && _YOD_SERVER_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d, 0x%02X, %p) in %s:%d",
		__FUNCTION__, evloop, fd, what, arg, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (!evloop || !fd || !what) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return;
	}

	self = (yod_server_t *) arg;
	if (!self || !self->root) {
		errno = EINVAL;
		return;
	}
	root = self->root;

	/* __EVL_CLOSE */
	if (what & __EVL_CLOSE) {
		if (self->func) {
			self->func(self, self->fd, __EVS_CLOSE, self->arg __ENV_CARGS);
		}
		yod_server_destroy(self);
		return;
	}

	if (!self->evloop) {
		return;
	}

	/* stopping */
	if (root->what == YOD_SERVER_STATE_STOPING) {
		yod_server_close(self);

		YOD_STDLOG_WARN("server stoping");
		return;
	}

	/* timeout */
	if (self->tick > 0) {
		self->msec = yod_common_nowtime() + self->tick;
	}

	/* __EVL_READ */
	if (what & __EVL_READ) {
		yod_server_ref(self);
		if (yod_thread_run(root->thread, _yod_server_input_cb, self) != 0) {
			yod_server_destroy(self);
		}
	}

	/* __EVL_ERROR */
	if (what & __EVL_ERROR) {
		yod_server_close(self);
	}

}
/* }}} */


/** {{{ static void _yod_server_input_cb(yod_evloop_t *evloop, yod_socket_t fd,
	short what, void *arg __ENV_CPARM)
*/
static void _yod_server_input_cb(yod_evloop_t *evloop, yod_socket_t fd,
	short what, void *arg __ENV_CPARM)
{
	yod_server_t *root = NULL;
	yod_server_t *self = NULL;
	byte *ptr = NULL;
	char data[YOD_SERVER_DATA_SIZE];
	int offset = 0;
	int size = 0;
	int ret = 0;

#if (_YOD_SYSTEM_DEBUG && _YOD_SERVER_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d, 0x%02X, %p) in %s:%d",
		__FUNCTION__, evloop, fd, what, arg, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (!evloop || !fd || !what) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return;
	}

	self = (yod_server_t *) arg;
	if (!self || !self->root) {
		return;
	}
	root = self->root;

	/* closing */
	if (!self->evloop) {
		yod_server_destroy(self);
		return;
	}

	/* stopping */
	if (root->what == YOD_SERVER_STATE_STOPING) {
		yod_server_close(self);
		yod_server_destroy(self);

		YOD_STDLOG_WARN("server stoping");
		return;
	}

	pthread_mutex_lock(&self->lock);
	{
		while ((ret = yod_socket_recv(self->fd, data, sizeof(data))) > 0) {
			if (self->data.len + ret > self->data.size) {
				size = self->data.size;
				while (self->data.len + ret > size) {
					size += YOD_SERVER_DATA_STEP;
				}

				ptr = (byte *) realloc(self->data.ptr, size * sizeof(byte));
				if (!ptr) {
					YOD_STDLOG_WARN("realloc failed");

					continue;
				}
				self->data.ptr = ptr;
				self->data.size = size;
			}

			ptr = self->data.ptr + self->data.len;
			self->data.len += ret;
			memcpy(ptr, data, ret);
			ptr[ret] = '\0';

			/* __EVS_INPUT */
			if (self->func) {
				offset = self->func(self, self->fd, __EVS_INPUT, self->arg __ENV_CARGS);
				if (offset > 0 && offset < self->data.len) {
					ptr = self->data.ptr + offset;
					self->data.len -= offset;
					memcpy(self->data.ptr, ptr, self->data.len);
					self->data.ptr[self->data.len] = '\0';
				}
				else if (offset != 0) {
					self->data.len = 0;
				}
			}
			else {
				self->data.len = 0;
			}

		}

		/* closed */
		if (ret == 0) {
			if (self->evloop) {
				yod_evloop_del(self->evloop, __EVL_ALL);
				self->evloop = NULL;
			}
		}
	}
	pthread_mutex_unlock(&self->lock);

	yod_server_destroy(self);
}
/* }}} */


#ifdef _WIN32
/** {{{ static unsigned __stdcall _yod_server_tick_cb(void *arg)
*/
static unsigned __stdcall _yod_server_tick_cb(void *arg)
#else
/** {{{ static void *_yod_server_tick_cb(void *arg)
*/
static void *_yod_server_tick_cb(void *arg)
#endif
{
	yod_server_t *root = NULL;
	yod_server_t *self = NULL;
	uint64_t msec = 0;

	self = (yod_server_t *) arg;
	if (!self || !self->root) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");

#ifdef _WIN32
		return 0;
#else
		return NULL;
#endif
	}
	root = self->root;

	while (yod_server_init__) {
		if (root->count > 0) {
			msec = yod_common_nowtime();
			for (self = root->next; self != NULL; self = self->next) {
				if (self->evloop || self->fd == -1) {
					if (self->msec > msec) {
						continue;
					}
					self->msec += self->tick;
					if ((self->what & __EVS_TIMEOUT) != 0) {
						if (self->func) {
							self->func(self, self->fd, __EVS_TIMEOUT, self->arg __ENV_CARGS);
						}
					}
				}
			}
			pthread_mutex_lock(&root->lock);
			if (root->msec < msec) {
				while (root->tick > root->count * 2) {
					if ((self = root->heap) != NULL) {
						root->heap = self->heap;
						-- root->tick;
						pthread_mutex_destroy(&self->lock);
						if (self->data.ptr) {
							free(self->data.ptr);
						}
						free(self);
					}
				}
				root->msec = msec + 60000;
			}
			pthread_mutex_unlock(&root->lock);
		}
#ifdef _WIN32
		Sleep(YOD_SERVER_LOOP_TICK);
#else
		usleep(YOD_SERVER_LOOP_TICK * 1000);
#endif
	}

#ifdef _WIN32
	return 0;
#else
	return NULL;
#endif
}
/* }}} */
