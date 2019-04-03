#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
	#define FD_SETSIZE 2048
	#include <winsock2.h>
	#include <time.h>
#else
	#include <sys/time.h>
	#include <unistd.h>
#endif
#ifdef _WIN32
#elif defined(__CYGWIN__)
	#include <sys/poll.h>
#elif __APPLE__
	#include <sys/types.h>
	#include <sys/event.h>
#else
	#include <sys/epoll.h>
#endif

#include <errno.h>

#include "stdlog.h"
#include "evloop.h"


#ifndef _YOD_EVLOOP_DEBUG
#define _YOD_EVLOOP_DEBUG 										0
#endif

#define YOD_EVLOOP_LOOP_TICK 									10


/* yod_evloop status */
enum
{
	YOD_EVLOOP_STATE_STOPING,
	YOD_EVLOOP_STATE_RUNNING,
	YOD_EVLOOP_STATE_ERROR,
};


/* yod_evloop_t */
struct _yod_evloop_t
{
	pthread_mutex_t lock;

	ulong count;

#if (defined(_WIN32) || defined(__CYGWIN__))
	int pofd;
	yod_evloop_t **data;
	struct pollfd *evfd;
#elif __APPLE__
	int kqfd;
	struct kevent *evfd;
#else
	int epfd;
	struct epoll_event *evfd;
#endif

	yod_socket_t fd;
	short what;
	yod_evloop_fn func;
	void *arg;

	yod_evloop_t *root;
	yod_evloop_t *next;
	yod_evloop_t *prev;
	yod_evloop_t *heap;
};


/** {{{ yod_evloop_t *_yod_evloop_new(__ENV_PARM)
*/
yod_evloop_t *_yod_evloop_new(__ENV_PARM)
{
	yod_evloop_t *self = NULL;

	self = (yod_evloop_t *) malloc(sizeof(yod_evloop_t));
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
		self->fd = 1024;
		self->what = __EVL_LOOP;
		self->func = NULL;
		self->arg = NULL;

		self->root = NULL;
		self->next = NULL;
		self->prev = NULL;
		self->heap = NULL;
	}

#if (defined(_WIN32) || defined(__CYGWIN__))
	self->evfd = (struct pollfd *) calloc(self->fd, sizeof(struct pollfd));
	if (!self->evfd) {
		free(self);

		YOD_STDLOG_ERROR("calloc failed");
		return NULL;
	}
	self->data = (yod_evloop_t **) calloc(self->fd, sizeof(yod_evloop_t *));
	if (!self->data) {
		free(self);

		YOD_STDLOG_ERROR("calloc failed");
		return NULL;
	}
	self->pofd = 0;
#elif __APPLE__
	self->evfd = (struct kevent *) calloc(self->fd, sizeof(struct kevent));
	if (!self->evfd) {
		free(self);

		YOD_STDLOG_ERROR("calloc failed");
		return NULL;
	}
	self->kqfd = kqueue();
	if (self->kqfd == -1) {
		yod_evloop_free(self);

		YOD_STDLOG_ERROR("kqueue failed");
		return NULL;
	}
#else
	self->evfd = (struct epoll_event *) calloc(self->fd, sizeof(struct epoll_event));
	if (!self->evfd) {
		free(self);

		YOD_STDLOG_ERROR("calloc failed");
		return NULL;
	}
	self->epfd = epoll_create(102400);
	if (self->epfd == -1) {
		yod_evloop_free(self);

		YOD_STDLOG_ERROR("epoll_create failed");
		return NULL;
	}
#endif

	self->root = self;

#if (_YOD_SYSTEM_DEBUG && _YOD_EVLOOP_DEBUG)
	yod_stdlog_debug(NULL, "%s(): %p in %s:%d %s",
		__FUNCTION__, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return self;
}
/* }}} */


/** {{{ void _yod_evloop_free(yod_evloop_t *self __ENV_CPARM)
*/
void _yod_evloop_free(yod_evloop_t *self __ENV_CPARM)
{
	yod_evloop_t *root = NULL;

	if (!self || !self->root) {
		return;
	}
	root = self->root;

#if (_YOD_SYSTEM_DEBUG && _YOD_EVLOOP_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): {what=0x%02X} in %s:%d %s",
		__FUNCTION__, self, root->what, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (root->what == YOD_EVLOOP_STATE_RUNNING) {
		yod_evloop_stop(root);
	}

	pthread_mutex_lock(&root->lock);

	root->count = 0;

#if (defined(_WIN32) || defined(__CYGWIN__))
	free(root->data);
#elif __APPLE__
	close(root->kqfd);
#else
	close(root->epfd);
#endif

	free(root->evfd);

	/* event */
	while ((self = root->next) != NULL) {
		root->next = self->next;
		free(self);
	}

	pthread_mutex_unlock(&root->lock);
	pthread_mutex_destroy(&root->lock);

	free(root);
}
/* }}} */


/** {{{ void _yod_evloop_start(yod_evloop_t *self, uint32_t wait __ENV_CPARM)
*/
void _yod_evloop_start(yod_evloop_t *self, uint32_t wait __ENV_CPARM)
{
	yod_evloop_t *root = NULL;
#ifdef __APPLE__
	struct timeval tval;
#endif
	int nfds = 0;
	int i = 0;

#if (_YOD_SYSTEM_DEBUG && _YOD_EVLOOP_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d) in %s:%d %s",
		__FUNCTION__, self, wait, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (!self || !self->root) {
		return;
	}
	root = self->root;

	if (root->what == YOD_EVLOOP_STATE_RUNNING) {
		return;
	}
	root->what = YOD_EVLOOP_STATE_RUNNING;

	if (wait == 0) {
		wait = YOD_EVLOOP_LOOP_TICK;
	}

#ifdef __APPLE__
	tval.tv_sec = 0;
	tval.tv_usec = wait * 1000;
#endif

	while (root->what == YOD_EVLOOP_STATE_RUNNING)
	{
		/* stopping */
		if (root->what != YOD_EVLOOP_STATE_RUNNING) {
			return;
		}

		/* looping */
		if (root->func) {
			root->func(root, -1, __EVL_LOOP, root->arg __ENV_CARGS);
		}

		if (root->count == 0) {
#ifdef _WIN32
			Sleep(YOD_EVLOOP_LOOP_TICK);
#else
			usleep(YOD_EVLOOP_LOOP_TICK * 1000);
#endif
			continue;
		}

		/* delete */
		pthread_mutex_lock(&root->lock);
		while ((self = root->heap) != NULL) {
			root->heap = self->heap;
			if (self->fd > 0) {
				yod_socket_close(self->fd);
			}
			if (self->func) {
				self->func(self, self->fd, __EVL_CLOSE, self->arg __ENV_CARGS);
			}
			if (self->next) {
				self->next->prev = self->prev;
			}
			if (self->prev) {
				self->prev->next = self->next;
			} else {
				root->next = self->next;
			}
			-- root->count;
#if (defined(_WIN32) || defined(__CYGWIN__))
			*(self->data) = NULL;
			self->evfd->fd = 0;
			self->evfd->events = 0;
#endif
			free(self);
		}
		pthread_mutex_unlock(&root->lock);

		/* events */
#if (defined(_WIN32) || defined(__CYGWIN__))
		nfds = poll(root->evfd, root->pofd, wait);
		if (nfds == SOCKET_ERROR) {
#ifdef _WIN32
			if (WSAGetLastError() != WSAENOTSOCK) {
				YOD_STDLOG_ERROR("poll failed");
			}
			Sleep(YOD_EVLOOP_LOOP_TICK);
#else
			YOD_STDLOG_ERROR("poll failed");
			usleep(YOD_EVLOOP_LOOP_TICK * 1000);
#endif
			continue;
		}
		for (i = 0; i < root->pofd; ++i) {
			/* stopping */
			if (root->what != YOD_EVLOOP_STATE_RUNNING) {
				return;
			}
			if (root->evfd[i].fd != 0 && root->evfd[i].revents != 0) {
				if ((self = (yod_evloop_t *) root->data[i]) != NULL) {
					/* __EVL_READ */
					if (root->evfd[i].revents & POLLIN) {
						self->func(self, self->fd, __EVL_READ, self->arg __ENV_CARGS);
					}
					/* __EVL_WRITE */
					if (root->evfd[i].revents & POLLOUT) {
						self->func(self, self->fd, __EVL_WRITE, self->arg __ENV_CARGS);
					}
					/* __EVL_ERROR */
					if (root->evfd[i].revents & (POLLPRI | POLLERR | POLLHUP)) {
						self->func(self, self->fd, __EVL_ERROR, self->arg __ENV_CARGS);
					}
				}
				if (-- nfds == 0) {
					break;
				}
			}
		}
#elif __APPLE__
		nfds = kevent(root->kqfd, NULL, 0, root->evfd, 10240, &tval);
		for (i = 0; i < nfds; ++i) {
			/* stopping */
			if (root->what != YOD_EVLOOP_STATE_RUNNING) {
				return;
			}
			if ((self = (yod_evloop_t *) root->evfd[i].udata) != NULL) {
				/* __EVL_READ */
				if (root->evfd[i].filter == EVFILT_READ) {
					self->func(self, self->fd, __EVL_READ, self->arg __ENV_CARGS);
				}
				/* __EVL_WRITE */
				if (root->evfd[i].filter == EVFILT_WRITE) {
					self->func(self, self->fd, __EVL_WRITE, self->arg __ENV_CARGS);
				}
				/* __EVL_ERROR */
				if (root->evfd[i].flags & EV_ERROR) {
					self->func(self, self->fd, __EVL_ERROR, self->arg __ENV_CARGS);
				}
			}
		}
#else
		nfds = epoll_wait(root->epfd, root->evfd, 10240, wait);
		for (i = 0; i < nfds; i++) {
			/* stopping */
			if (root->what != YOD_EVLOOP_STATE_RUNNING) {
				return;
			}
			if ((self = (yod_evloop_t *) root->evfd[i].data.ptr) != NULL) {
				/* __EVL_READ */
				if (root->evfd[i].events & EPOLLIN) {
					self->func(self, self->fd, __EVL_READ, self->arg __ENV_CARGS);
				}
				/* __EVL_WRITE */
				if (root->evfd[i].events & EPOLLOUT) {
					self->func(self, self->fd, __EVL_WRITE, self->arg __ENV_CARGS);
				}
#ifdef EPOLLRDHUP
				/* __EVL_ERROR */
				if (root->evfd[i].events & EPOLLRDHUP) {
					self->func(self, self->fd, __EVL_ERROR, self->arg __ENV_CARGS);
				}
#endif
				/* __EVL_ERROR */
				if (root->evfd[i].events & (EPOLLPRI | EPOLLERR | EPOLLHUP)) {
					self->func(self, self->fd, __EVL_ERROR, self->arg __ENV_CARGS);
				}
			}
		}
#endif
	}
}
/* }}} */


/** {{{ void _yod_evloop_stop(yod_evloop_t *self __ENV_CPARM)
*/
void _yod_evloop_stop(yod_evloop_t *self __ENV_CPARM)
{
	if (!self || !self->root) {
		return;
	}

	self->root->what = YOD_EVLOOP_STATE_STOPING;

#if (_YOD_SYSTEM_DEBUG && _YOD_EVLOOP_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): {what=0x%02X} in %s:%d %s",
		__FUNCTION__, self, self->root->what, __ENV_TRACE);
#else
	__ENV_VOID
#endif
}
/* }}} */


/** {{{ int _yod_evloop_loop(yod_evloop_t *self, yod_evloop_fn func, void *arg __ENV_CPARM)
*/
int _yod_evloop_loop(yod_evloop_t *self, yod_evloop_fn func, void *arg __ENV_CPARM)
{
	yod_evloop_t *root = NULL;

	if (!self || !self->root) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return (-1);
	}
	root = self->root;

	root->func = func;
	root->arg = arg;

#if (_YOD_SYSTEM_DEBUG && _YOD_EVLOOP_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %p, %p): 0 in %s:%d %s",
		__FUNCTION__, self, func, arg, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return (0);
}
/* }}} */


/** {{{ int _yod_evloop_add(yod_evloop_t *self, yod_socket_t fd, short what,
	yod_evloop_fn func, void *arg, yod_evloop_t **evl __ENV_CPARM)
*/
int _yod_evloop_add(yod_evloop_t *self, yod_socket_t fd, short what,
	yod_evloop_fn func, void *arg, yod_evloop_t **evl __ENV_CPARM)
{
	yod_evloop_t *root = NULL;
#if (defined(_WIN32) || defined(__CYGWIN__))
	int i = 0;
#elif __APPLE__
	struct kevent evfd;
#else
	struct epoll_event evfd;
#endif
	void *ptr = NULL;
	int ret = 0;

	if (!self || !self->root) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return (-1);
	}
	root = self->root;

	if ((what & (__EVL_READ | __EVL_WRITE)) == 0) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid what");
		return (-1);
	}

	if (fd <= 0)
	{
		errno = EBADF;
		YOD_STDLOG_WARN("evloop failed");
		return (-1);
	}

	pthread_mutex_lock(&root->lock);
#if (defined(_WIN32) || defined(__CYGWIN__))
	if (root->pofd + 1 > root->fd) {
		if ((ptr = realloc(root->evfd, (root->fd + 1024) * sizeof(struct pollfd))) == NULL) {
			YOD_STDLOG_ERROR("realloc failed");
			return (-1);
		}
		memset((char *) ptr + root->fd * sizeof(struct pollfd), 0, 1024 * sizeof(struct pollfd));
		root->evfd = (struct pollfd *) ptr;
		if ((ptr = realloc(root->data, (root->fd + 1024) * sizeof(yod_evloop_t *))) == NULL) {
			YOD_STDLOG_ERROR("realloc failed");
			return (-1);
		}
		memset((char *) ptr + root->fd * sizeof(yod_evloop_t *), 0, 1024 * sizeof(yod_evloop_t *));
		root->data = (yod_evloop_t **) ptr;
		for (i = 0; i < root->pofd; ++i) {
			if ((self = root->data[i]) != NULL) {
				self->evfd = &root->evfd[i];
				self->data = &root->data[i];
			}
		}
		root->fd += 1024;
	}
#elif __APPLE__
	if (root->count + 1 > root->fd) {
		if ((ptr = realloc(root->evfd, (root->fd + 1024) * sizeof(struct kevent))) == NULL) {
			YOD_STDLOG_ERROR("realloc failed");
			return (-1);
		}
		memset((char *) ptr + root->fd * sizeof(struct kevent), 0, 1024 * sizeof(struct kevent));
		root->evfd = (struct kevent *) ptr;
		root->fd += 1024;
	}
#else
	if (root->count + 1 > root->fd) {
		if ((ptr = realloc(root->evfd, (root->fd + 1024) * sizeof(struct epoll_event))) == NULL) {
			YOD_STDLOG_ERROR("realloc failed");
			return (-1);
		}
		memset((char *) ptr + root->fd * sizeof(struct epoll_event), 0, 1024 * sizeof(struct epoll_event));
		root->evfd = (struct epoll_event *) ptr;
		root->fd += 1024;
	}
#endif
	pthread_mutex_unlock(&root->lock);

	self = (yod_evloop_t *) malloc(sizeof(yod_evloop_t));
	if (!self) {
		YOD_STDLOG_ERROR("malloc failed");
		return (-1);
	}

	self->count = 0;
	self->evfd = NULL;
	self->fd = fd;
	self->what = what;
	self->func = func;
	self->arg = arg;

	pthread_mutex_lock(&root->lock);

	self->root = root;
	self->next = root->next;
	self->prev = NULL;
	self->heap = NULL;

#if (defined(_WIN32) || defined(__CYGWIN__))
	for (i = 0; i < root->fd; ++i) {
		if (root->evfd[i].fd == 0) {
			break;
		}
	}
	self->evfd = &root->evfd[i];
	self->evfd->fd = fd;
	self->evfd->events = 0;
	/* __EVL_READ */
	if ((what & __EVL_READ) != 0) {
		self->evfd->events |= POLLIN;
	}
	/* __EVL_WRITE */
	if ((what & __EVL_WRITE) != 0) {
		self->evfd->events |= POLLOUT;
	}
	root->data[i] = self;
	self->data = &root->data[i];
	root->pofd = i + 1;
	self->pofd = i;
#elif __APPLE__
	/* __EVL_READ */
	if ((what & __EVL_READ) != 0) {
		EV_SET(&evfd, fd, EVFILT_READ, EV_ADD, 0, 0, self);
		kevent(root->kqfd, &evfd, 1, NULL, 0, NULL);
	}
	/* __EVL_WRITE */
	if ((what & __EVL_WRITE) != 0) {
		EV_SET(&evfd, fd, EVFILT_WRITE, EV_ADD, 0, 0, self);
		kevent(root->kqfd, &evfd, 1, NULL, 0, NULL);
	}
#else
	evfd.events = EPOLLET | EPOLLPRI | EPOLLERR | EPOLLHUP;
	evfd.data.fd = fd;
	evfd.data.ptr = self;
	/* __EVL_READ */
	if ((what & __EVL_READ) != 0) {
		evfd.events |= EPOLLIN;
	}
	/* __EVL_WRITE */
	if ((what & __EVL_WRITE) != 0) {
		evfd.events |= EPOLLOUT;
	}

	if (epoll_ctl(root->epfd, EPOLL_CTL_ADD, fd, &evfd) != 0) {
		ret = -1;

		YOD_STDLOG_ERROR("epoll_ctl failed");
	}
#endif

	if (ret != 0) {
		pthread_mutex_unlock(&root->lock);
		free(self);
		return (-1);
	}

	if (self->next) {
		self->next->prev = self;
	}
	root->next = self;

	++ root->count;

	pthread_mutex_unlock(&root->lock);

	if (evl) {
		*evl = self;
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_EVLOOP_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d, 0x%02X, %p, %p, %p): %d, count=%d in %s:%d %s",
		__FUNCTION__, root, fd, what, func, arg, self, ret, root->count, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ int _yod_evloop_set(yod_evloop_t *self, short what, void *arg __ENV_CPARM)
*/
int _yod_evloop_set(yod_evloop_t *self, short what, void *arg __ENV_CPARM)
{
	yod_evloop_t *root = NULL;
#if (defined(_WIN32) || defined(__CYGWIN__))

#elif __APPLE__
	struct kevent evfd;
#else
	struct epoll_event evfd;
#endif
	int ret = 0;

	if (!self || !self->root) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return (-1);
	}
	root = self->root;

	if (self == root) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid evloop");
		return (-1);
	}

	if (what != 0) {
		if ((self->what & what) == what || !(what & (__EVL_READ | __EVL_WRITE))) {
			errno = EINVAL;
			YOD_STDLOG_WARN("invalid what");
			return (-1);
		}

		pthread_mutex_lock(&root->lock);

#if (defined(_WIN32) || defined(__CYGWIN__))
		/* __EVL_READ */
		if ((what & __EVL_READ) != 0) {
			self->evfd->events |= POLLIN;
		}
		/* __EVL_WRITE */
		if ((what & __EVL_WRITE) != 0) {
			self->evfd->events |= POLLOUT;
		}
#elif __APPLE__
		/* __EVL_READ */
		if ((what & __EVL_READ) != 0) {
			EV_SET(&evfd, self->fd, EVFILT_READ, EV_ADD, 0, 0, self);
			kevent(root->kqfd, &evfd, 1, NULL, 0, NULL);
		}
		/* __EVL_WRITE */
		if ((what & __EVL_WRITE) != 0) {
			EV_SET(&evfd, self->fd, EVFILT_WRITE, EV_ADD, 0, 0, self);
			kevent(root->kqfd, &evfd, 1, NULL, 0, NULL);
		}
#else
		evfd.events = EPOLLET | EPOLLPRI | EPOLLERR | EPOLLHUP;
		evfd.data.fd = self->fd;
		evfd.data.ptr = self;
		/* __EVL_READ */
		if ((self->what & __EVL_READ) != 0) {
			evfd.events |= EPOLLIN;
		}
		/* __EVL_WRITE */
		if ((self->what & __EVL_WRITE) != 0) {
			evfd.events |= EPOLLOUT;
		}

		if (epoll_ctl(root->epfd, EPOLL_CTL_MOD, self->fd, &evfd) != 0) {
			ret = -1;

			YOD_STDLOG_ERROR("epoll_ctl failed");
		}
#endif

		self->what |= what;

		pthread_mutex_unlock(&root->lock);
	}
	else {
		self->arg = arg;
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_EVLOOP_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, 0x%02X, %p): %d, count=%d in %s:%d %s",
		__FUNCTION__, self, what, arg, ret, root->count, __ENV_TRACE);
#else
	__ENV_VOID
#endif

#if (_YOD_EVLOOP_DEBUG & 0x02)
	{
		char *dump = yod_evloop_dump(root);
		yod_stdlog_dump(NULL, "%s\n%s\n%s\n%s%s\n",
			__LOG_LINE_1, __FUNCTION__, __LOG_LINE_1, dump, __LOG_LINE_1);
		free(dump);
	}
#endif

	return ret;
}
/* }}} */


/** {{{ int _yod_evloop_del(yod_evloop_t *self, short what __ENV_CPARM)
*/
int _yod_evloop_del(yod_evloop_t *self, short what __ENV_CPARM)
{
	yod_evloop_t *root = NULL;
#if (defined(_WIN32) || defined(__CYGWIN__))

#elif __APPLE__
	struct kevent evfd;
#else
	struct epoll_event evfd;
#endif
	int ret = 0;

	if (!self || !self->root) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return (-1);
	}
	root = self->root;

	if (self == root) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid evloop");
		return (-1);
	}

	if ((self->what & what) == 0) {
		return (0);
	}

	pthread_mutex_lock(&root->lock);

	self->what &= ~what;

#if (defined(_WIN32) || defined(__CYGWIN__))
	if ((self->what & (__EVL_READ | __EVL_WRITE)) != 0) {
		self->evfd->events = 0;
		/* __EVL_READ */
		if ((what & __EVL_READ) != 0) {
			self->evfd->events |= POLLIN;
		}
		/* __EVL_WRITE */
		if ((what & __EVL_WRITE) != 0) {
			self->evfd->events |= POLLOUT;
		}
	}
#elif __APPLE__
	/* __EVL_READ */
	if ((what & __EVL_READ) != 0) {
		EV_SET(&evfd, self->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
		kevent(root->kqfd, &evfd, 1, NULL, 0, NULL);
	}
	/* __EVL_WRITE */
	if ((what & __EVL_WRITE) != 0) {
		EV_SET(&evfd, self->fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
		kevent(root->kqfd, &evfd, 1, NULL, 0, NULL);
	}
#else
	if ((self->what & (__EVL_READ | __EVL_WRITE)) != 0) {
		evfd.events = EPOLLET | EPOLLPRI | EPOLLERR | EPOLLHUP;
		evfd.data.fd = self->fd;
		evfd.data.ptr = self;
		/* __EVL_READ */
		if ((self->what & __EVL_READ) != 0) {
			evfd.events |= EPOLLIN;
		}
		/* __EVL_WRITE */
		if ((self->what & __EVL_WRITE) != 0) {
			evfd.events |= EPOLLOUT;
		}

		if (epoll_ctl(root->epfd, EPOLL_CTL_MOD, self->fd, &evfd) != 0) {
			ret = -1;

			YOD_STDLOG_ERROR("epoll_ctl failed");
		}
	} else {
		if (epoll_ctl(root->epfd, EPOLL_CTL_DEL, self->fd, NULL) != 0) {
			ret = -1;

			YOD_STDLOG_ERROR("epoll_ctl failed");
		}
	}
#endif

	if ((self->what & (__EVL_READ | __EVL_WRITE)) == 0) {
		self->heap = root->heap;
		root->heap = self;
	}

	pthread_mutex_unlock(&root->lock);

#if (_YOD_SYSTEM_DEBUG && _YOD_EVLOOP_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, 0x%02X): %d, count=%d in %s:%d %s",
		__FUNCTION__, self, what, ret, root->count, __ENV_TRACE);
#else
	__ENV_VOID
#endif

#if (_YOD_EVLOOP_DEBUG & 0x02)
	{
		char *dump = yod_evloop_dump(root);
		yod_stdlog_dump(NULL, "%s\n%s\n%s\n%s%s\n",
			__LOG_LINE_1, __FUNCTION__, __LOG_LINE_1, dump, __LOG_LINE_1);
		free(dump);
	}
#endif

	return ret;
}
/* }}} */


/** {{{ ulong _yod_evloop_count(yod_evloop_t *self __ENV_CPARM)
*/
ulong _yod_evloop_count(yod_evloop_t *self __ENV_CPARM)
{
	if (!self || !self->root) {
		return 0;
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_EVLOOP_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): %lu in %s:%d %s",
		__FUNCTION__, self, self->root->count, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return self->root->count;
}
/* }}} */


/** {{{ char *_yod_evloop_dump(yod_evloop_t *self)
*/
char *_yod_evloop_dump(yod_evloop_t *self)
{
	yod_evloop_t *root = NULL;
	char *ret = NULL;

	if (!self || !self->root) {
		return NULL;
	}
	root = self->root;

	return ret;
}
/* }}} */
