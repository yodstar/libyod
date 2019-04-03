#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif
#include <errno.h>

#include "stdlog.h"
#include "thread.h"


#ifndef _YOD_THREAD_DEBUG
#define _YOD_THREAD_DEBUG 										0
#endif


#define YOD_THREAD_NUM_MAX 										1024


/* yod_thwork_t */
typedef struct _yod_thwork_t
{
	yod_evloop_fn func;
	void *arg;

	struct _yod_thwork_t *next;
	struct _yod_thwork_t *prev;
} yod_thwork_t;


// yod_thread_t
struct _yod_thread_t
{
	pthread_mutex_t lock;
#ifndef _WIN32
	pthread_cond_t cond;
#endif

	pthread_t tid;

	ulong count;

	yod_evloop_t *evloop;

#ifndef _WIN32
	int pipe_recv_fd;
	int pipe_send_fd;
#endif

	yod_thwork_t *head;
	yod_thwork_t *tail;
	yod_thwork_t *heap;

	yod_thread_t *root;
	yod_thread_t *next;
	yod_thread_t *prev;
};


#ifdef _WIN32
static unsigned __stdcall _yod_thread_add_cb(void *arg);
#else
static void *_yod_thread_add_cb(void *arg);
#endif

static void _yod_thread_run_cb(yod_evloop_t *evloop, yod_socket_t fd, short what, void *arg __ENV_CPARM);


/** {{{ yod_thread_t *_yod_thread_new(int thread_num __ENV_CPARM)
*/
yod_thread_t *_yod_thread_new(int thread_num __ENV_CPARM)
{
	yod_thread_t *self = NULL;
	int i = 0;

	if (thread_num < 1) {
		return NULL;
	}

	self = (yod_thread_t *) malloc(sizeof(yod_thread_t));
	if (!self) {
		YOD_STDLOG_ERROR("malloc failed");
		return NULL;
	}

	if (pthread_mutex_init(&self->lock, NULL) != 0) {
		free(self);

		YOD_STDLOG_ERROR("pthread_mutex_init failed");
		return NULL;
	}

#ifndef _WIN32
	pthread_cond_init(&self->cond, NULL);
#endif

	{
		self->tid = 0;
		self->count = 0;
		self->evloop = NULL;

#ifndef _WIN32
		self->pipe_recv_fd = -1;
		self->pipe_send_fd = -1;
#endif

		self->head = NULL;
		self->tail = NULL;
		self->heap = NULL;

		self->root = self;
		self->next = NULL;
		self->prev = NULL;
	}

	if (thread_num > YOD_THREAD_NUM_MAX) {
		thread_num = YOD_THREAD_NUM_MAX;
	}

	for (i = thread_num; i > 0; --i) {
		if (yod_thread_add(self) != 0) {
			-- thread_num;
		}
	}

#ifdef _WIN32
	while (self->count < (size_t) thread_num) {
		Sleep(1);
	}
#else
	pthread_mutex_lock(&self->lock);
	while (self->count < thread_num) {
		pthread_cond_wait(&self->cond, &self->lock);
	}
	pthread_mutex_unlock(&self->lock);
#endif

#if (_YOD_SYSTEM_DEBUG && _YOD_THREAD_DEBUG)
	yod_stdlog_debug(NULL, "%s(%d): %p in %s:%d %s",
		__FUNCTION__, thread_num, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

#if (_YOD_THREAD_DEBUG &0x02)
	{
		char *dump = yod_thread_dump(self);
		yod_stdlog_dump(NULL, "%s\n%s\n%s\n%s%s\n",
			__LOG_LINE_1, __FUNCTION__, __LOG_LINE_1, dump, __LOG_LINE_1);
		free(dump);
	}
#endif

	return self;
}
/* }}} */


/** {{{ void _yod_thread_free(yod_thread_t *self __ENV_CPARM)
*/
void _yod_thread_free(yod_thread_t *self __ENV_CPARM)
{
	yod_thread_t *root = NULL;
	yod_thwork_t *work = NULL;

#if (_YOD_SYSTEM_DEBUG && _YOD_THREAD_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p) in %s:%d %s",
		__FUNCTION__, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (!self || !self->root) {
		return;
	}
	root = self->root;

	pthread_mutex_lock(&root->lock);

	while ((self = root->next) != NULL) {
		root->next = self->next;
		self->count = 0;
		if (self->evloop) {
			yod_evloop_free(self->evloop);
		}
#ifndef _WIN32
		close(self->pipe_recv_fd);
		close(self->pipe_send_fd);
#endif
		while ((work = self->heap) != NULL) {
			self->heap = work->next;
			free(work);
		}
		while ((work = self->head) != NULL) {
			self->head = work->next;
			free(work);
		}
		free(self);
	}

	root->count = 0;

#ifndef _WIN32
	pthread_cond_destroy(&root->cond);
#endif

	pthread_mutex_unlock(&root->lock);
	pthread_mutex_destroy(&root->lock);

	free(root);
}
/* }}} */


/** {{{ yod_thread_t *_yod_thread_new(yod_thpool_t *thpool __ENV_CPARM)
*/
int _yod_thread_add(yod_thread_t *self __ENV_CPARM)
{
	yod_thread_t *root = NULL;
#ifdef _WIN32
	HANDLE handle = 0;
#else
	pthread_attr_t thattr;
	int fd[2];
#endif

	if (!self || !self->root) {
		return (-1);
	}
	root = self->root;

	self = (yod_thread_t *) malloc(sizeof(yod_thread_t));
	if (!self) {
		YOD_STDLOG_ERROR("malloc failed");
		return (-1);
	}

	if (pthread_mutex_init(&self->lock, NULL) != 0) {
		free(self);

		YOD_STDLOG_ERROR("pthread_mutex_init failed");
		return (-1);
	}

	{
		self->tid = 0;
		self->count = 0;
#ifndef _WIN32
		self->pipe_recv_fd = -1;
		self->pipe_send_fd = -1;
#endif
		self->head = NULL;
		self->tail = NULL;
		self->heap = NULL;
		self->root = root;
		self->next = NULL;
		self->prev = NULL;
	}

	self->evloop = yod_evloop_new();
	if (!self->evloop) {
		YOD_STDLOG_ERROR("evloop_new failed");
		goto e_failed;
	}

#ifndef _WIN32
	if (pipe(fd)) {
		YOD_STDLOG_ERROR("pipe failed");
		goto e_failed;
	}

	self->pipe_recv_fd = fd[0];
	self->pipe_send_fd = fd[1];
#endif

#ifdef _WIN32
	if (yod_evloop_loop(self->evloop, _yod_thread_run_cb, self) != 0)
#else
	if (yod_evloop_add(self->evloop, fd[0], __EVL_READ, _yod_thread_run_cb, self, NULL) != 0)
#endif
	{
		YOD_STDLOG_ERROR("evloop_add failed");
		goto e_failed;
	}

#ifdef _WIN32
	handle = (HANDLE) _beginthreadex(NULL, 0, _yod_thread_add_cb, self, 0, &self->tid);
	if (handle == 0) {
		YOD_STDLOG_ERROR("_beginthreadex failed");
		goto e_failed;
	}
	CloseHandle(handle);
#else
	pthread_attr_init(&thattr);
	if (pthread_attr_setdetachstate(&thattr, PTHREAD_CREATE_DETACHED) != 0) {
		YOD_STDLOG_ERROR("pthread_attr_setdetachstate failed");
		goto e_failed;
	}
	if (pthread_create(&self->tid, &thattr, _yod_thread_add_cb, self) == -1) {
		pthread_attr_destroy(&thattr);
		YOD_STDLOG_ERROR("pthread_create failed");
		goto e_failed;
	}
	pthread_attr_destroy(&thattr);
#endif

#if (_YOD_SYSTEM_DEBUG && _YOD_THREAD_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): %p in %s:%d %s",
		__FUNCTION__, root, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

#if (_YOD_THREAD_DEBUG &0x02)
	{
		char *dump = yod_thread_dump(root);
		yod_stdlog_dump(NULL, "%s\n%s\n%s\n%s%s\n",
			__LOG_LINE_1, __FUNCTION__, __LOG_LINE_1, dump, __LOG_LINE_1);
		free(dump);
	}
#endif

	return (0);

e_failed:

	if (self->evloop) {
		yod_evloop_free(self->evloop);
	}

#ifndef _WIN32
	if (self->pipe_recv_fd > 0) {
		close(self->pipe_recv_fd);
	}

	if (self->pipe_send_fd > 0) {
		close(self->pipe_send_fd);
	}
#endif

	pthread_mutex_destroy(&self->lock);
	free(self);

#if (_YOD_SYSTEM_DEBUG && _YOD_THREAD_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): -1 in %s:%d %s",
		__FUNCTION__, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return (-1);
}
/* }}} */


/** {{{ int _yod_thread_run(yod_thread_t *self, yod_evloop_fn func, void *arg __ENV_CPARM)
*/
int _yod_thread_run(yod_thread_t *self, yod_evloop_fn func, void *arg __ENV_CPARM)
{
	yod_thread_t *root = NULL;
	yod_thwork_t *work = NULL;
#ifndef _WIN32
	char buffer[1];
#endif

	if (!self || !self->root) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return (-1);
	}
	root = self->root;

	pthread_mutex_lock(&root->lock);

	if (!root->prev) {
		root->prev = root->next;
	}

	self = root->prev;
	if (!self) {
		pthread_mutex_unlock(&root->lock);

		YOD_STDLOG_WARN("thread failed");
		return (-1);
	}

	pthread_mutex_lock(&self->lock);
	{
		if ((work = self->heap) != NULL) {
			self->heap = work->next;
		}
		else {
			work = (yod_thwork_t *) malloc(sizeof(yod_thwork_t));
			if (!work) {
				pthread_mutex_unlock(&self->lock);
				pthread_mutex_unlock(&root->lock);

				YOD_STDLOG_ERROR("malloc failed");
				return (-1);
			}
		}

		work->func = func;
		work->arg = arg;

		work->next = self->head;
		work->prev = NULL;
		if (work->next) {
			work->next->prev = work;
		}
		self->head = work;
		if (!self->tail) {
			self->tail = work;
		}
		root->prev = self->next;

		++ self->count;
	}
	pthread_mutex_unlock(&self->lock);

#ifndef _WIN32
	buffer[0] = __EVL_LOOP;
	if (write(self->pipe_send_fd, buffer, 1) == -1) {
		YOD_STDLOG_ERROR("write failed");
	}
#endif

	pthread_mutex_unlock(&root->lock);

#if (_YOD_SYSTEM_DEBUG && _YOD_THREAD_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %p, %p): 0 in %s:%d %s",
		__FUNCTION__, self, func, arg, __ENV_TRACE);
#else
	__ENV_VOID
#endif

#if (_YOD_THREAD_DEBUG &0x02)
	{
		char *dump = yod_thread_dump(root);
		yod_stdlog_dump(NULL, "%s\n%s\n%s\n%s%s\n",
			__LOG_LINE_1, __FUNCTION__, __LOG_LINE_1, dump, __LOG_LINE_1);
		free(dump);
	}
#endif

	return (0);
}
/* }}} */


/** {{{ ulong _yod_thread_count(yod_thread_t *self __ENV_CPARM)
*/
ulong _yod_thread_count(yod_thread_t *self __ENV_CPARM)
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


/** {{{ char *_yod_thread_dump(yod_thread_t *self)
*/
char *_yod_thread_dump(yod_thread_t *self)
{
	yod_thread_t *root = NULL;
	char *ret = NULL;

	if (!self || !self->root) {
		return NULL;
	}
	root = self->root;

	return ret;
}
/* }}} */


#ifdef _WIN32
/** {{{ static unsigned __stdcall _yod_thread_add_cb(void *arg)
*/
static unsigned __stdcall _yod_thread_add_cb(void *arg)
#else
/** {{{ static void *_yod_thread_add_cb(void *arg)
*/
static void *_yod_thread_add_cb(void *arg)
#endif
{
	yod_thread_t *root = NULL;
	yod_thread_t *self = NULL;

	self = (yod_thread_t *) arg;
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

	pthread_mutex_lock(&root->lock);
	{
		self->next = root->next;
		self->prev = NULL;
		if (self->next) {
			self->next->prev = self;
		}
		root->next = self;
		++ root->count;
#ifndef _WIN32
		pthread_cond_signal(&root->cond);
#endif
	}
	pthread_mutex_unlock(&root->lock);

#if (_YOD_SYSTEM_DEBUG && _YOD_THREAD_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p) in %s:%d",
		__FUNCTION__, self, __ENV_TRACE2);
#endif

#if (_YOD_THREAD_DEBUG &0x02)
	{
		char *dump = yod_thread_dump(self);
		yod_stdlog_dump(NULL, "%s\n%s\n%s\n%s%s\n",
			__LOG_LINE_1, __FUNCTION__, __LOG_LINE_1, dump, __LOG_LINE_1);
		free(dump);
	}
#endif

	yod_evloop_start(self->evloop, 0);

#ifdef _WIN32
	return 0;
#else
	return NULL;
#endif
}
/* }}} */


/** {{{ static void _yod_thread_run_cb(yod_evloop_t *evloop, yod_socket_t fd,
	short what, void *arg __ENV_CPARM)
*/
static void _yod_thread_run_cb(yod_evloop_t *evloop, yod_socket_t fd,
	short what, void *arg __ENV_CPARM)
{
	yod_thread_t *self = NULL;
	yod_thwork_t *work = NULL;
#ifndef _WIN32
	char buffer[1];
#endif

	if (!evloop || !fd || !what) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return;
	}

	if ((what & __EVL_ERROR) != 0) {
		return;
	}

	self = (yod_thread_t *) arg;
	if (!self || !self->root) {
		errno = EINVAL;
		YOD_STDLOG_WARN("thread failed");
		return;
	}

	/* events */
	while (self->count > 0)
	{

#if (_YOD_SYSTEM_DEBUG && _YOD_THREAD_DEBUG)
		yod_stdlog_debug(NULL, "%s(%d, 0x%02X, %p): {count=%lu} in %s:%d",
			__FUNCTION__, fd, what, arg, self->count, __ENV_TRACE);
#endif

#if (_YOD_THREAD_DEBUG &0x02)
		{
			char *dump = yod_thread_dump(self);
			yod_stdlog_dump(NULL, "%s\n%s\n%s\n%s%s\n",
				__LOG_LINE_1, __FUNCTION__, __LOG_LINE_1, dump, __LOG_LINE_1);
			free(dump);
		}
#endif

#ifndef _WIN32
		if (read(self->pipe_recv_fd, buffer, 1) == -1) {
			YOD_STDLOG_WARN("read failed");
		}
#endif

		pthread_mutex_lock(&self->lock);

		if ((work = self->tail) != NULL) {
			if (work->next) {
				work->next->prev = work->prev;
			}
			if (work->prev) {
				work->prev->next = work->next;
			}
			self->tail = work->prev;
			if (!self->tail) {
				self->head = NULL;
			}
			
			if (self->count > 0) {
				-- self->count;
			}

			if (work->func) {
				work->func(evloop, -1, __EVL_LOOP, work->arg __ENV_CARGS);
			}

			work->next = self->heap;
			self->heap = work;
		}

		pthread_mutex_unlock(&self->lock);
	}

	__ENV_VOID
}
/* }}} */
