#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <errno.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

#include "rbtree.h"
#include "htable.h"
#include "stdlog.h"
#include "server.h"
#include "shttpd.h"


#ifndef _YOD_SHTTPD_DEBUG
#define _YOD_SHTTPD_DEBUG 										0
#endif


#define YOD_SHTTPD_HTML_INDEX 									"index.html"
#define YOD_SHTTPD_HTML_LEN 									256


enum
{
	YOD_SHTTPD_STATE_CONNECT,
	YOD_SHTTPD_STATE_START,
	YOD_SHTTPD_STATE_INPUT,
	YOD_SHTTPD_STATE_OUTPUT,
	YOD_SHTTPD_STATE_CLOSED
};


/* yod_shttpd_t */
struct _yod_shttpd_t
{
	pthread_mutex_t lock;
	ulong count;

	yod_rbtree_t *http_status;
	yod_server_t *server;
	yod_htable_t *routes;

	char *listen;
	char *htdocs;
	short status;

	yod_string_t output;

	yod_shttpd_r hreq;

	yod_shttpd_t *root;
	yod_shttpd_t *next;
	yod_shttpd_t *prev;
};


/* yod_shttpd_u */
typedef union
{
	void *ptr;
	yod_shttpd_fn func;
} yod_shttpd_u;


#define yod_shttpd_connect_cb(x, t, w, a) 						_yod_shttpd_connect_cb(x, t, w, a __ENV_CARGS)
#define yod_shttpd_input_cb(x, t, w, a) 						_yod_shttpd_input_cb(x, t, w, a __ENV_CARGS)
#define yod_shttpd_close_cb(x, t, w, a) 						_yod_shttpd_close_cb(x, t, w, a __ENV_CARGS)
#define yod_shttpd_timeout_cb(x, t, w, a) 						_yod_shttpd_timeout_cb(x, t, w, a __ENV_CARGS)
#define yod_shttpd_request(x, d, l) 							_yod_shttpd_request(x, d, l __ENV_CARGS)
#define yod_shttpd_response(x) 									_yod_shttpd_response(x __ENV_CARGS)
#define yod_shttpd_write(x, r) 									_yod_shttpd_write(x, r __ENV_CARGS)
#define yod_shttpd_clean(x, f) 									_yod_shttpd_clean(x, f __ENV_CARGS)


static int _yod_shttpd_handle_cb(yod_server_t *server, yod_socket_t fd, short what, void *arg __ENV_CPARM);
static int _yod_shttpd_connect_cb(yod_server_t *server, yod_socket_t fd, short what, void *arg __ENV_CPARM);
static int _yod_shttpd_input_cb(yod_server_t *server, yod_socket_t fd, short what, void *arg __ENV_CPARM);
static int _yod_shttpd_close_cb(yod_server_t *server, yod_socket_t fd, short what, void *arg __ENV_CPARM);
static int _yod_shttpd_timeout_cb(yod_server_t *server, yod_socket_t fd, short what, void *arg __ENV_CPARM);

static int _yod_shttpd_request(yod_shttpd_t *self, byte **data, int *len __ENV_CPARM);
static int _yod_shttpd_response(yod_shttpd_t *self __ENV_CPARM);
static int _yod_shttpd_write(yod_shttpd_t *self, yod_shttpd_r *hreq __ENV_CPARM);
static int _yod_shttpd_clean(yod_shttpd_t *self, int force __ENV_CPARM);


/* yod_shttpd_init__ */
static int yod_shttpd_init__ = 0;


/** {{{ yod_shttpd_t *_yod_shttpd_new(yod_server_t *server, const char *listen, const char *htdocs __ENV_CPARM)
*/
yod_shttpd_t *_yod_shttpd_new(yod_server_t *server, const char *listen, const char *htdocs __ENV_CPARM)
{
	yod_shttpd_t *self = NULL;

	if (!server || !listen || !htdocs) {
		return NULL;
	}

	self = (yod_shttpd_t *) malloc(sizeof(yod_shttpd_t));
	if (!self) {
		YOD_STDLOG_ERROR("malloc failed");
		return NULL;
	}

	if (pthread_mutex_init(&self->lock, NULL) != 0) {
		free(self);

		YOD_STDLOG_ERROR("pthread_mutex_init failed");
		return NULL;
	}

	self->count = 0;

	self->http_status = NULL;
	self->server = server;
	self->routes = NULL;

	self->listen = NULL;
	self->htdocs = NULL;
	self->status = 0;

	self->output.ptr = NULL;
	self->output.len = 0;

	/* request */
	self->hreq.method = 0;
	self->hreq.version = 0;
	self->hreq.host.ptr = NULL;
	self->hreq.host.len = 0;
	self->hreq.path.ptr = NULL;
	self->hreq.path.len = 0;
	self->hreq.qstr.ptr = NULL;
	self->hreq.qstr.len = 0;
	self->hreq.keep_alive = 0;
	self->hreq.user_agent.ptr = NULL;
	self->hreq.user_agent.len = 0;
	self->hreq.referer.ptr = NULL;
	self->hreq.referer.len = 0;
	self->hreq.cookie.ptr = NULL;
	self->hreq.cookie.len = 0;

	/* response */
	self->hreq.headers.ptr = NULL;
	self->hreq.headers.len = 0;
	self->hreq.outfile.ptr = NULL;
	self->hreq.outfile.len = 0;
	self->hreq.content.ptr = NULL;
	self->hreq.content.len = 0;
	self->hreq.mime_type = NULL;
	self->hreq.status = 0;

	self->root = self;
	self->next = NULL;
	self->prev = NULL;

	self->http_status = yod_rbtree_new(NULL);
	if (!self->http_status) {
		yod_shttpd_free(self);

		YOD_STDLOG_ERROR("rbtree_new failed");
		return NULL;
	}

	self->routes = yod_htable_new(NULL);
	if (!self->routes) {
		yod_shttpd_free(self);

		YOD_STDLOG_ERROR("htable_new failed");
		return NULL;
	}

	self->listen = strdup(listen);
	self->htdocs = strdup(htdocs);

	/* 1xx */
	yod_rbtree_set(self->http_status, 100, (void *) "Continue");
	yod_rbtree_set(self->http_status, 101, (void *) "Switching Protocols");
	yod_rbtree_set(self->http_status, 102, (void *) "Processing");

	/* 2xx */
	yod_rbtree_set(self->http_status, 200, (void *) "OK");
	yod_rbtree_set(self->http_status, 201, (void *) "Created");
	yod_rbtree_set(self->http_status, 202, (void *) "Accepted");
	yod_rbtree_set(self->http_status, 203, (void *) "Non-Authoritative Information");
	yod_rbtree_set(self->http_status, 204, (void *) "No Content");
	yod_rbtree_set(self->http_status, 205, (void *) "Reset Content");
	yod_rbtree_set(self->http_status, 206, (void *) "Partial Content");

	/* 3xx */
	yod_rbtree_set(self->http_status, 300, (void *) "Multiple Choices");
	yod_rbtree_set(self->http_status, 301, (void *) "Moved Permanently");
	yod_rbtree_set(self->http_status, 302, (void *) "Move temporarily");
	yod_rbtree_set(self->http_status, 303, (void *) "See Other");
	yod_rbtree_set(self->http_status, 304, (void *) "Not Modified");
	yod_rbtree_set(self->http_status, 305, (void *) "Use Proxy");
	yod_rbtree_set(self->http_status, 306, (void *) "Switch Proxy");
	yod_rbtree_set(self->http_status, 307, (void *) "Temporary Redirect");

	/* 4xx */
	yod_rbtree_set(self->http_status, 400, (void *) "Bad Request");
	yod_rbtree_set(self->http_status, 401, (void *) "Unauthorized");
	yod_rbtree_set(self->http_status, 403, (void *) "Forbidden");
	yod_rbtree_set(self->http_status, 404, (void *) "Not Found");
	yod_rbtree_set(self->http_status, 405, (void *) "Method Not Allowed");
	yod_rbtree_set(self->http_status, 406, (void *) "Not Acceptable");
	yod_rbtree_set(self->http_status, 407, (void *) "Proxy Authentication Required");
	yod_rbtree_set(self->http_status, 408, (void *) "Request Timeout");
	yod_rbtree_set(self->http_status, 409, (void *) "Conflict");
	yod_rbtree_set(self->http_status, 410, (void *) "Gone");
	yod_rbtree_set(self->http_status, 411, (void *) "Length Required");
	yod_rbtree_set(self->http_status, 412, (void *) "Precondition Failed");
	yod_rbtree_set(self->http_status, 413, (void *) "Request Entity Too Large");
	yod_rbtree_set(self->http_status, 414, (void *) "Request-URI Too Long");
	yod_rbtree_set(self->http_status, 415, (void *) "Unsupported Media Type");
	yod_rbtree_set(self->http_status, 416, (void *) "Requested Range Not Satisfiable");
	yod_rbtree_set(self->http_status, 417, (void *) "Expectation Failed");
	yod_rbtree_set(self->http_status, 422, (void *) "Unprocessable Entity");
	yod_rbtree_set(self->http_status, 423, (void *) "Locked");
	yod_rbtree_set(self->http_status, 424, (void *) "Failed Dependency");
	yod_rbtree_set(self->http_status, 425, (void *) "Unordered Collection");
	yod_rbtree_set(self->http_status, 426, (void *) "Upgrade Required");
	yod_rbtree_set(self->http_status, 449, (void *) "Retry With");

	/* 5xx */
	yod_rbtree_set(self->http_status, 500, (void *) "Internal Server Error");
	yod_rbtree_set(self->http_status, 501, (void *) "Not Implemented");
	yod_rbtree_set(self->http_status, 502, (void *) "Bad Gateway");
	yod_rbtree_set(self->http_status, 503, (void *) "Service Unavailable");
	yod_rbtree_set(self->http_status, 504, (void *) "Gateway Timeout");
	yod_rbtree_set(self->http_status, 505, (void *) "HTTP Version Not Supported");
	yod_rbtree_set(self->http_status, 506, (void *) "Variant Also Negotiates");
	yod_rbtree_set(self->http_status, 507, (void *) "Insufficient Storage");
	yod_rbtree_set(self->http_status, 509, (void *) "Bandwidth Limit Exceeded");
	yod_rbtree_set(self->http_status, 510, (void *) "Not Extended");

	/* 6xx */
	yod_rbtree_set(self->http_status, 600, (void *) "Unparseable Response Headers");

	if (yod_server_listen(self->server, self->listen, 80, _yod_shttpd_handle_cb, self, 90000) != 0) {
		yod_shttpd_free(self);

		YOD_STDLOG_ERROR("server_listen failed");
		return NULL;
	}

	yod_shttpd_init__ = 1;

#if (_YOD_SYSTEM_DEBUG && _YOD_SHTTPD_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %s, %s): %p in %s:%d %s",
		__FUNCTION__, server, listen, htdocs, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return self;
}
/* }}} */


/** {{{ void _yod_shttpd_free(yod_shttpd_t *self __ENV_CPARM)
*/
void _yod_shttpd_free(yod_shttpd_t *self __ENV_CPARM)
{
	yod_shttpd_t *root = NULL;

#if (_YOD_SYSTEM_DEBUG && _YOD_SHTTPD_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p) in %s:%d %s",
		__FUNCTION__, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (!self || !self->root) {
		return;
	}
	root = self->root;

	yod_shttpd_init__ = 0;

	pthread_mutex_lock(&root->lock);

	while ((self = root->next) != NULL) {
		root->next = self->next;
		yod_shttpd_clean(self, 1);
		pthread_mutex_destroy(&self->lock);
		free(self);
	}

	if (root->http_status) {
		yod_rbtree_free(root->http_status);
	}

	if (root->routes) {
		yod_htable_free(root->routes);
	}

	if (root->listen) {
		free(root->listen);
	}

	if (root->htdocs) {
		free(root->htdocs);
	}

	pthread_mutex_unlock(&root->lock);
	pthread_mutex_destroy(&root->lock);

	free(root);
}
/* }}} */


/** {{{ int _yod_shttpd_route(yod_shttpd_t *self, const char *route, yod_shttpd_fn func __ENV_CPARM)
*/
int _yod_shttpd_route(yod_shttpd_t *self, const char *route, yod_shttpd_fn func __ENV_CPARM)
{
	yod_shttpd_t *root = NULL;
	yod_shttpd_u swap = {0};
	int ret = 0;

	if (!self || !self->root) {
		return (-1);
	}
	root = self->root;

	swap.func = func;
	yod_htable_add_assoc(root->routes, route, swap.ptr);

#if (_YOD_SYSTEM_DEBUG && _YOD_SHTTPD_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %s, %p): %d in %s:%d",
		__FUNCTION__, self, route, func, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ int _yod_shttpd_handle_cb(yod_server_t *server, yod_socket_t fd, short what, void *arg __ENV_CPARM)
*/
static int _yod_shttpd_handle_cb(yod_server_t *server, yod_socket_t fd, short what, void *arg __ENV_CPARM)
{
	yod_shttpd_t *self = NULL;
	int ret = 0;

	if (!server || !fd || !what || !arg) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return (-1);
	}

	self = (yod_shttpd_t *) arg;
	if (!self || !self->root) {
		return (-1);
	}

	if (self->status != YOD_SHTTPD_STATE_CLOSED) {
		switch (what) {
			case __EVS_CONNECT:
				ret = yod_shttpd_connect_cb(server, fd, what, arg);
				break;
			case __EVS_INPUT:
				ret = yod_shttpd_input_cb(server, fd, what, arg);
				break;
			case __EVS_CLOSE:
				self->status = YOD_SHTTPD_STATE_CLOSED;
				ret = yod_shttpd_close_cb(server, fd, what, arg);
				break;
			case __EVS_TIMEOUT:
				ret = yod_shttpd_timeout_cb(server, fd, what, arg);
				break;
		}
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_SHTTPD_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d, 0x%02X, %p): %d in %s:%d",
		__FUNCTION__, server, fd, what, arg, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ int _yod_shttpd_connect_cb(yod_server_t *server, yod_socket_t fd, short what, void *arg __ENV_CPARM)
*/
static int _yod_shttpd_connect_cb(yod_server_t *server, yod_socket_t fd, short what, void *arg __ENV_CPARM)
{
	yod_shttpd_t *root = NULL;
	yod_shttpd_t *self = NULL;

	if (!yod_shttpd_init__) {
		return (-1);
	}

	if (!server || !fd || !what) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return (-1);
	}

	self = (yod_shttpd_t *) arg;
	if (!self || !self->root) {
		return (-1);
	}
	root = self->root;

	self = (yod_shttpd_t *) malloc(sizeof(yod_shttpd_t));
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
		self->count = 1;

		self->http_status = NULL;

		self->server = server;
		self->routes = NULL;

		self->listen = NULL;
		self->htdocs = NULL;
		self->status = YOD_SHTTPD_STATE_CONNECT;

		self->output.ptr = strdup("");
		self->output.len = 0;

		/* request */
		self->hreq.method = 0;
		self->hreq.version = 0;
		self->hreq.host.ptr = strdup("");
		self->hreq.host.len = 0;
		self->hreq.path.ptr = strdup("");
		self->hreq.path.len = 0;
		self->hreq.qstr.ptr = NULL;
		self->hreq.qstr.len = 0;
		self->hreq.keep_alive = 0;
		self->hreq.user_agent.ptr = strdup("");
		self->hreq.user_agent.len = 0;
		self->hreq.referer.ptr = strdup("");
		self->hreq.referer.len = 0;
		self->hreq.cookie.ptr = strdup("");
		self->hreq.cookie.len = 0;

		/* response */
		self->hreq.headers.ptr = strdup("");
		self->hreq.headers.len = 0;
		self->hreq.outfile.ptr = strdup("");
		self->hreq.outfile.len = 0;
		self->hreq.content.ptr = strdup("");
		self->hreq.content.len = 0;
		self->hreq.mime_type = NULL;
		self->hreq.status = 0;
	}

	pthread_mutex_lock(&root->lock);
	{
		self->root = root;
		self->next = root->next;
		self->prev = NULL;
		if (self->next) {
			self->next->prev = self;
		}
		root->next = self;

		++ root->count;
	}
	pthread_mutex_unlock(&root->lock);

	yod_server_setcb(server, _yod_shttpd_handle_cb, self);

#if (_YOD_SYSTEM_DEBUG && _YOD_SHTTPD_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d, 0x%02X, %p): 0 in %s:%d",
		__FUNCTION__, server, fd, what, arg, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return (0);
}
/* }}} */


/** {{{ int _yod_shttpd_input_cb(yod_server_t *server, yod_socket_t fd, short what, void *arg __ENV_CPARM)
*/
static int _yod_shttpd_input_cb(yod_server_t *server, yod_socket_t fd, short what, void *arg __ENV_CPARM)
{
	yod_shttpd_t *self = NULL;
	int offset = 0;
	byte *data = NULL;
	int len = 0;
	int ret = 0;

	if (!yod_shttpd_init__) {
		return (-1);
	}

	if (!server || !fd || !what) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return (-1);
	}

	self = (yod_shttpd_t *) arg;
	if (!self || !self->root || self->server != server) {
		return (-1);
	}

	pthread_mutex_lock(&self->lock);
	++ self->count;
	pthread_mutex_unlock(&self->lock);

	data = yod_server_recv(server, &len);
	while ((offset = yod_shttpd_request(self, &data, &len)) > 0) {
		if (self->count > 0) {
			yod_shttpd_response(self);
		}
		ret += offset;
	}

	yod_shttpd_close_cb(server, fd, what, arg);

#if (_YOD_SYSTEM_DEBUG && _YOD_SHTTPD_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d, 0x%02X, %p): %d in %s:%d",
		__FUNCTION__, server, fd, what, arg, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ int _yod_shttpd_close_cb(yod_server_t *server, yod_socket_t fd, short what, void *arg __ENV_CPARM)
*/
static int _yod_shttpd_close_cb(yod_server_t *server, yod_socket_t fd, short what, void *arg __ENV_CPARM)
{
	yod_shttpd_t *root = NULL;
	yod_shttpd_t *self = NULL;

	if (!yod_shttpd_init__) {
		return (-1);
	}

	if (!server || !fd || !what) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return (-1);
	}

	self = (yod_shttpd_t *) arg;
	if (!self || !self->root) {
		return (-1);
	}
	root = self->root;

	if (self == root) {
		errno = EINVAL;
		return (-1);
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_SHTTPD_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d, 0x%02X, %p): %lu in %s:%d",
		__FUNCTION__, server, fd, what, arg, self->count, __ENV_TRACE);
#else
	__ENV_VOID
#endif

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

			pthread_mutex_unlock(&root->lock);

			yod_shttpd_clean(self, 1);
			
			pthread_mutex_unlock(&self->lock);
			pthread_mutex_destroy(&self->lock);

			free(self);
			return (0);
		}
	}
	pthread_mutex_unlock(&self->lock);

	return (0);
}
/* }}} */


/** {{{ int _yod_shttpd_timeout_cb(yod_server_t *server, yod_socket_t fd, short what, void *arg __ENV_CPARM)
*/
static int _yod_shttpd_timeout_cb(yod_server_t *server, yod_socket_t fd, short what, void *arg __ENV_CPARM)
{
	yod_shttpd_t *self = NULL;

	if (!yod_shttpd_init__) {
		return (-1);
	}

	if (!server || !fd || !what) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return (-1);
	}

	self = (yod_shttpd_t *) arg;
	if (!self || !self->root || self->server != server) {
		return (-1);
	}

	yod_server_close(server);

#if (_YOD_SYSTEM_DEBUG && _YOD_SHTTPD_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d, 0x%02X, %p): 0 in %s:%d",
		__FUNCTION__, server, fd, what, arg, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return (0);
}
/* }}} */


/** {{{ static int _yod_shttpd_request(yod_shttpd_t *self, byte **data, int *len __ENV_CPARM)
*/
static int _yod_shttpd_request(yod_shttpd_t *self, byte **data, int *len __ENV_CPARM)
{
	char *token = NULL;
	int data_len = 0;
	char *line = NULL;
	int line_len = 0;
	char *ptr = NULL;
	int ret = 0;

	if (!yod_shttpd_init__) {
		return (-1);
	}

	if (!self || !data || !len) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return (-1);
	}

	if (*len <= 0) {
		return (-1);
	}

	if ((line = strtok_r((char *) *data, "\n", &token)) != NULL) {

		line_len = (int) strlen(line) + 1;
		data_len += line_len;

		/* http_version */
		if (strncmp((line + line_len - 10), "HTTP/1.1", 8) == 0) {
			self->hreq.version = YOD_SHTTPD_HTTP_1_1;
		}
		else if (strncmp((line + line_len - 10), "HTTP/1.0", 8) == 0) {
			self->hreq.version = YOD_SHTTPD_HTTP_1_0;
		}
		else {
			self->hreq.status = 600;

			YOD_STDLOG_WARN("invalid version");
			goto e_failed;
		}

		/* method */
		if (strncmp(line, "OPTIONS", 7) == 0) {
			self->hreq.method = YOD_SHTTPD_METHOD_OPTIONS;
			line_len -= 19;
			line += 8;
		}
		else if (strncmp(line, "GET", 3) == 0) {
			self->hreq.method = YOD_SHTTPD_METHOD_GET;
			line_len -= 15;
			line += 4;
		}
		else if (strncmp(line, "HEAD", 4) == 0) {
			self->hreq.method = YOD_SHTTPD_METHOD_HEAD;
			line_len -= 16;
			line += 5;
		}
		else if (strncmp(line, "POST", 4) == 0) {
			self->hreq.method = YOD_SHTTPD_METHOD_POST;
			line_len -= 16;
			line += 5;
		}
		else if (strncmp(line, "PUT", 3) == 0) {
			self->hreq.method = YOD_SHTTPD_METHOD_PUT;
			line_len -= 15;
			line += 4;
		}
		else if (strncmp(line, "DELETE", 6) == 0) {
			self->hreq.method = YOD_SHTTPD_METHOD_DELETE;
			line_len -= 18;
			line += 7;
		}
		else if (strncmp(line, "TRACE", 5) == 0) {
			self->hreq.method = YOD_SHTTPD_METHOD_TRACE;
			line_len -= 17;
			line += 6;
		}
		else if (strncmp(line, "CONNECT", 7) == 0) {
			self->hreq.method = YOD_SHTTPD_METHOD_CONNECT;
			line_len -= 19;
			line += 8;
		}
		else {
			self->hreq.method = YOD_SHTTPD_METHOD_UNKNOWN;
			self->hreq.status = 600;

			YOD_STDLOG_WARN("invalid method");
			goto e_failed;
		}

		/* path */
		if (self->hreq.path.len < line_len) {
			ptr = (char *) realloc(self->hreq.path.ptr, (line_len + 1) * sizeof(char));
			if (!ptr) {
				self->hreq.status = 500;

				YOD_STDLOG_ERROR("realloc failed");
				goto e_failed;
			}
			self->hreq.path.ptr = ptr;
			self->hreq.path.len = line_len;
		}
		memcpy(self->hreq.path.ptr, line, line_len);
		self->hreq.path.ptr[line_len] = '\0';

		yod_common_urldecode(self->hreq.path.ptr, line_len + 1, self->hreq.path.ptr);
		if ((ptr = strchr(self->hreq.path.ptr, '?')) != NULL) {
			*(ptr ++) = '\0';
			self->hreq.qstr.ptr = ptr;
			self->hreq.qstr.len = strlen(ptr);
		}

		while ((line = strtok_r(NULL, "\n", &token)) != NULL) {

			line_len = (int) strlen(line) + 1;
			data_len += line_len;

			if (strncasecmp(line, "Host: ", 6) == 0) {
				line_len = (int) strlen(line + 6);
				if (self->hreq.host.len < line_len) {
					ptr = (char *) realloc(self->hreq.host.ptr, (line_len + 1) * sizeof(char));
					if (!ptr) {
						self->hreq.status = 500;

						YOD_STDLOG_ERROR("realloc failed");
						goto e_failed;
					}
					self->hreq.host.ptr = ptr;
					self->hreq.host.len = line_len;
				}
				memcpy(self->hreq.host.ptr, line + 6, line_len);
				self->hreq.host.ptr[line_len] = '\0';
			}
			else if (strncasecmp(line, "Connection: keep-alive", 22) == 0) {
				self->hreq.keep_alive = 1;
			}
			else if (strncasecmp(line, "User-Agent: ", 12) == 0) {
				line_len = (int) strlen(line + 12);
				if (self->hreq.user_agent.len < line_len) {
					ptr = (char *) realloc(self->hreq.user_agent.ptr, (line_len + 1) * sizeof(char));
					if (!ptr) {
						self->hreq.status = 500;

						YOD_STDLOG_ERROR("realloc failed");
						goto e_failed;
					}
					self->hreq.user_agent.ptr = ptr;
					self->hreq.user_agent.len = line_len;
				}
				memcpy(self->hreq.user_agent.ptr, line + 12, line_len);
				self->hreq.user_agent.ptr[line_len] = '\0';
			}
			else if (strncasecmp(line, "Referer: ", 9) == 0) {
				line_len = (int) strlen(line + 9);
				if (self->hreq.referer.len < line_len) {
					ptr = (char *) realloc(self->hreq.referer.ptr, (line_len + 1) * sizeof(char));
					if (!ptr) {
						self->hreq.status = 500;

						YOD_STDLOG_ERROR("realloc failed");
						goto e_failed;
					}
					self->hreq.referer.ptr = ptr;
					self->hreq.referer.len = line_len;
				}
				memcpy(self->hreq.referer.ptr, line + 9, line_len);
				self->hreq.referer.ptr[line_len] = '\0';
			}
			else if (strncasecmp(line, "Cookie: ", 8) == 0) {
				line_len = (int) strlen(line + 8);
				if (self->hreq.cookie.len < line_len) {
					ptr = (char *) realloc(self->hreq.cookie.ptr, (line_len + 1) * sizeof(char));
					if (!ptr) {
						self->hreq.status = 500;

						YOD_STDLOG_ERROR("realloc failed");
						goto e_failed;
					}
					self->hreq.cookie.ptr = ptr;
					self->hreq.cookie.len = line_len;
				}
				memcpy(self->hreq.cookie.ptr, line + 8, line_len);
				self->hreq.cookie.ptr[line_len] = '\0';
			}
		}

		ret = data_len;
	}
	else {
		self->hreq.status = 600;
		ret = -1;
	}

e_failed:

#if (_YOD_SYSTEM_DEBUG && _YOD_SHTTPD_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %p, %d): %d in %s:%d",
		__FUNCTION__, self, (data ? *data : 0), (len ? *len : 0), ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	*data += data_len;
	*len -= data_len;

	return ret;
}
/* }}} */


/** {{{ static int _yod_shttpd_response(yod_shttpd_t *self __ENV_CPARM)
*/
static int _yod_shttpd_response(yod_shttpd_t *self __ENV_CPARM)
{
	yod_shttpd_t *root = NULL;
	yod_shttpd_u swap = {0};
	char outfile[_MAX_PATH];
	char outpath[_MAX_PATH];
	char *ptr = NULL;
	size_t len = 0;
	struct stat st;
	int ret = 0;

	if (!yod_shttpd_init__) {
		return (-1);
	}

	if (!self || !self->root) {
		return (-1);
	}
	root = self->root;

	/* convpath */
	yod_common_convpath(self->hreq.path.ptr, outfile);

	/* routing */
	if ((swap.ptr = yod_htable_find_assoc(root->routes, outfile)) != NULL) {
		swap.func(&self->hreq __ENV_CARGS);
		goto e_failed;
	}

	/* document */
	ret = snprintf(outpath, sizeof(outpath), "%s%s", root->htdocs, outfile);
	if (ret == -1) {
		self->hreq.status = 500;

		YOD_STDLOG_WARN("snprintf failed");
 		goto e_failed;
	}

#ifdef _WIN32
	if (_fullpath(outfile, outpath, _MAX_PATH) != NULL && access(outfile, 0) == 0)
#else
	if (realpath(outpath, outfile) != NULL)
#endif
	{
 		if (stat(outfile, &st) != 0) {
			self->hreq.status = 500;

 			YOD_STDLOG_WARN("_stat failed");
 			goto e_failed;
 		}
 		/* folder */
 		else if (S_IFDIR & st.st_mode) {
			ret = snprintf(outpath, sizeof(outpath), "%s/%s", outfile, YOD_SHTTPD_HTML_INDEX);
			if (ret == -1) {
				self->hreq.status = 500;

				YOD_STDLOG_WARN("snprintf failed");
				goto e_failed;
			}

#ifdef _WIN32
			if (_fullpath(outfile, outpath, _MAX_PATH) != NULL && access(outfile, 0) == 0)
#else
			if (realpath(outpath, outfile) != NULL)
#endif
			{
				if (stat(outfile, &st) != 0) {
					self->hreq.status = 500;

		 			YOD_STDLOG_WARN("_stat failed");
		 			goto e_failed;
		 		}
		 		else if (S_IFDIR & st.st_mode) { /* folder */
		 			self->hreq.status = 403;
		 		}
		 		else if (S_IFREG & st.st_mode) { /* outfile */
					self->hreq.status = 200;
					len = strlen(outfile);
					if (self->hreq.outfile.len < len) {
						ptr = (char *) realloc(self->hreq.outfile.ptr, (len + 1) * sizeof(char));
						if (!ptr) {
							self->hreq.status = 500;

							YOD_STDLOG_ERROR("realloc failed");
							goto e_failed;
						}
						self->hreq.outfile.ptr = ptr;
						self->hreq.outfile.len = len;
					}
					memcpy(self->hreq.outfile.ptr, outfile, len);
					self->hreq.outfile.ptr[len] = '\0';
					self->hreq.mime_type = strrchr(outfile, '.');
				}
			}
			else {
				self->hreq.status = 404;
			}
		}
		/* outfile */
		else if (S_IFREG & st.st_mode) {
			self->hreq.status = 200;
			len = strlen(outfile);
			if (self->hreq.outfile.len < len) {
				ptr = (char *) realloc(self->hreq.outfile.ptr, (len + 1) * sizeof(char));
				if (!ptr) {
					self->hreq.status = 500;

					YOD_STDLOG_ERROR("realloc failed");
					goto e_failed;
				}
				self->hreq.outfile.ptr = ptr;
				self->hreq.outfile.len = len;
			}
			memcpy(self->hreq.outfile.ptr, outfile, len);
			self->hreq.outfile.ptr[len] = '\0';
			self->hreq.mime_type = strrchr(outfile, '.');
		}

		/* mime_type */
		if (self->hreq.mime_type) {
			if (strncasecmp(self->hreq.mime_type, ".bmp\0", 5) == 0) {
				self->hreq.mime_type = YOD_SHTTPD_MIME_BMP;
			}
			else if (strncasecmp(self->hreq.mime_type, ".gif\0", 5) == 0) {
				self->hreq.mime_type = YOD_SHTTPD_MIME_GIF;
			}
			else if (strncasecmp(self->hreq.mime_type, ".jpg\0", 5) == 0) {
				self->hreq.mime_type = YOD_SHTTPD_MIME_JPEG;
			}
			else if (strncasecmp(self->hreq.mime_type, ".jpeg\0", 6) == 0) {
				self->hreq.mime_type = YOD_SHTTPD_MIME_JPEG;
			}
			else if (strncasecmp(self->hreq.mime_type, ".png\0", 5) == 0) {
				self->hreq.mime_type = YOD_SHTTPD_MIME_PNG;
			}
			else if (strncasecmp(self->hreq.mime_type, ".ico\0", 5) == 0) {
				self->hreq.mime_type = YOD_SHTTPD_MIME_ICO;
			}
			else if (strncasecmp(self->hreq.mime_type, ".css\0", 5) == 0) {
				self->hreq.mime_type = YOD_SHTTPD_MIME_CSS;
			}
			else if (strncasecmp(self->hreq.mime_type, ".htm\0", 5) == 0) {
				self->hreq.mime_type = YOD_SHTTPD_MIME_HTML;
			}
			else if (strncasecmp(self->hreq.mime_type, ".html\0", 6) == 0) {
				self->hreq.mime_type = YOD_SHTTPD_MIME_HTML;
			}
			else if (strncasecmp(self->hreq.mime_type, ".js\0", 4) == 0) {
				self->hreq.mime_type = YOD_SHTTPD_MIME_JS;
			}
			else if (strncasecmp(self->hreq.mime_type, ".json\0", 6) == 0) {
				self->hreq.mime_type = YOD_SHTTPD_MIME_JSON;
			}
			else if (strncasecmp(self->hreq.mime_type, ".txt\0", 5) == 0) {
				self->hreq.mime_type = YOD_SHTTPD_MIME_TXT;
			}
			else if (strncasecmp(self->hreq.mime_type, ".xml\0", 5) == 0) {
				self->hreq.mime_type = YOD_SHTTPD_MIME_XML;
			}
			else if (strncasecmp(self->hreq.mime_type, ".zip\0", 5) == 0) {
				self->hreq.mime_type = YOD_SHTTPD_MIME_ZIP;
			}
			else {
				self->hreq.mime_type = NULL;
			}
		}
	}
	else {
		self->hreq.status = 404;
	}

e_failed:

	ret = yod_shttpd_write(self, &self->hreq);
	if (self->hreq.keep_alive != 1) {
		yod_server_close(self->server);
	}
	yod_shttpd_clean(self, 0);

#if (_YOD_SYSTEM_DEBUG && _YOD_SHTTPD_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): %d in %s:%d",
		__FUNCTION__, self, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ static int _yod_shttpd_write(yod_shttpd_t *self, yod_shttpd_r *hreq __ENV_CPARM)
*/
static int _yod_shttpd_write(yod_shttpd_t *self, yod_shttpd_r *hreq __ENV_CPARM)
{
	char http_head[YOD_SHTTPD_HTML_LEN];
	char http_body[YOD_SHTTPD_HTML_LEN];

	yod_shttpd_t *root = NULL;
	char *http_status = NULL;
	yod_string_t headers = {0};
	yod_string_t content = {0};
	yod_string_t output = {0};
	FILE *fp = NULL;
	char num[12];
	int ret = 0;

#if (_YOD_SYSTEM_DEBUG && _YOD_SHTTPD_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %p) in %s:%d",
		__FUNCTION__, self, hreq, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (!self || !self->root || !self->server) {
		return (-1);
	}
	root = self->root;

	hreq->status = hreq->status ? hreq->status : 200;

	/* outfile */
	if (strlen(hreq->outfile.ptr) != 0) {
#ifdef _WIN32
		if (fopen_s(&fp, hreq->outfile.ptr, "rb") == 0)
#else
		if ((fp = fopen(hreq->outfile.ptr, "rb")) != NULL)
#endif
		{
			fseek(fp, 0, SEEK_END);
			content.len = (size_t) ftell(fp);
			if (hreq->content.len < content.len) {
				content.ptr = (char *) realloc(hreq->content.ptr, (content.len + 1) * sizeof(char));
				if (!content.ptr) {
					YOD_STDLOG_ERROR("realloc failed");
					return (-1);
				}
				hreq->content.ptr = content.ptr;
				hreq->content.len = content.len;
			}
			if (content.len > 0) {
				fseek(fp, 0, SEEK_SET);
				if (fread(hreq->content.ptr, content.len, 1, fp) != 1) {
					hreq->status = 500;

					YOD_STDLOG_ERROR("fread failed");
				}
			}
			hreq->content.ptr[content.len] = '\0';

			fclose(fp);
		}
		else {
			hreq->mime_type = NULL;
			hreq->status = 404;
		}
	}

	http_status = (char *) yod_rbtree_find(root->http_status, hreq->status);
	if (!http_status) {
		YOD_STDLOG_WARN("rbtree_find failed");
		return (-1);
	}

	/* content */
	if (hreq->status != 200) {
		ret = snprintf(http_body, sizeof(http_body),
			"<html>\r\n"
			"<head><title>%d %s</title></head>\r\n"
			"<body bgcolor=\"white\">\r\n"
			"<center><h1>%d %s</h1></center>\r\n"
			"<hr><center>shttpd/%s</center>\r\n"
			"</body>\r\n"
			"</html>",
			hreq->status, http_status, hreq->status, http_status, YOD_SHTTPD_VERSION);

		if (ret == -1) {
			YOD_STDLOG_ERROR("snprintf failed");
			goto e_failed;
		}
		content.ptr = http_body;
		content.len = ret;
		ret = 0;
	}
	else {
		content.ptr = hreq->content.ptr;
		content.len = strlen(content.ptr);
	}

	/* headers */
	if (snprintf(num, sizeof(num), "%lu", (ulong) content.len) != -1) {
		ret = snprintf(http_head, sizeof(http_head),
			"HTTP/1.1 %d %s\r\n"
			"Connection: %s\r\n"
			"Server: shttpd/%s\r\n"
			"Content-Type: %s\r\n"
			"Content-Length: %s\r\n"
			"%s"
			"\r\n",
			hreq->status, http_status, (hreq->keep_alive ? "keep-alive" : "close"), YOD_SHTTPD_VERSION,
			(hreq->mime_type ? hreq->mime_type : YOD_SHTTPD_MIME_HTML), num, hreq->headers.ptr);

		if (ret == -1) {
			YOD_STDLOG_ERROR("snprintf failed");
			goto e_failed;
		}
		headers.ptr = http_head;
		headers.len = ret;

		output.ptr = self->output.ptr;
		output.len = headers.len + content.len;
		if (output.len > self->output.len) {
			output.ptr = (char *) realloc(output.ptr, output.len * sizeof(char));
			if (!output.ptr) {
				YOD_STDLOG_ERROR("malloc failed");
				goto e_failed;
			}
			self->output.ptr = output.ptr;
			self->output.len = output.len;
		}

		memcpy(output.ptr, headers.ptr, headers.len);
		memcpy(output.ptr + headers.len, content.ptr, content.len);

		if (yod_server_send(self->server, (byte *) output.ptr, (int) output.len) == SOCKET_ERROR) {
			self->server = NULL;
		}

		ret = 0;
	}

e_failed:

	return ret;
}
/* }}} */


/** {{{ static int _yod_shttpd_clean(yod_shttpd_t *self, int force __ENV_CPARM)
*/
static int _yod_shttpd_clean(yod_shttpd_t *self, int force __ENV_CPARM)
{
	if (!self || !self->root) {
		return (-1);
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_SHTTPD_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d) in %s:%d",
		__FUNCTION__, self, force, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (self->output.ptr) {
		if (!force) {
			self->output.ptr[0] = '\0';
		} else {
			free(self->output.ptr);
		}
	}

	/* request */
	self->hreq.method = 0;
	self->hreq.version = 0;

	if (self->hreq.host.ptr) {
		if (!force) {
			self->hreq.host.ptr[0] = '\0';
		} else {
			free(self->hreq.host.ptr);
		}
	}

	if (self->hreq.path.ptr) {
		if (!force) {
			self->hreq.path.ptr[0] = '\0';
		} else {
			free(self->hreq.path.ptr);
		}
	}

	self->hreq.keep_alive = 0;

	if (self->hreq.user_agent.ptr) {
		if (!force) {
			self->hreq.user_agent.ptr[0] = '\0';
		} else {
			free(self->hreq.user_agent.ptr);
		}
	}

	if (self->hreq.referer.ptr) {
		if (!force) {
			self->hreq.referer.ptr[0] = '\0';
		} else {
			free(self->hreq.referer.ptr);
		}
	}

	if (self->hreq.cookie.ptr) {
		if (!force) {
			self->hreq.cookie.ptr[0] = '\0';
		} else {
			free(self->hreq.cookie.ptr);
		}
	}

	/* response */
	if (self->hreq.headers.ptr) {
		if (!force) {
			self->hreq.headers.ptr[0] = '\0';
		} else {
			free(self->hreq.headers.ptr);
		}
	}

	if (self->hreq.outfile.ptr) {
		if (!force) {
			self->hreq.outfile.ptr[0] = '\0';
		} else {
			free(self->hreq.outfile.ptr);
		}
	}

	if (self->hreq.content.ptr) {
		if (!force) {
			self->hreq.content.ptr[0] = '\0';
		} else {
			free(self->hreq.content.ptr);
		}
	}

	self->hreq.mime_type = NULL;

	self->hreq.status = 0;

	return (0);
}
/* }}} */
