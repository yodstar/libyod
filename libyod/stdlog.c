#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#ifdef __ANDROID__
	#include <android/log.h>
#endif

#include "stdlog.h"

#ifndef _YOD_STDLOG_DEBUG
#define _YOD_STDLOG_DEBUG 										0
#endif

#define YOD_STDLOG_BUFFER_MAX 									1024

//#define YOD_STDLOG_FORMAT_ALL 								"[$Y-$M-$D $H:$I:$S $U] $P $N $L $X\n"
#define YOD_STDLOG_FORMAT_ALL 									"[$T] $P $N $L $X\n"

/* yod_stdlog_t */
struct _yod_stdlog_t
{
	pthread_mutex_t lock;

	char *name;
	short level;
	char *format[9];

	yod_stdlog_fn func;
	void *arg;

	yod_stdlog_t *next;
	yod_stdlog_t *prev;
};


/* yod_stdlog_level_key */
static const short yod_stdlog_level_key[] = {
	(0), (1), (2), 2, (3), 3, 3, 3, (4), 4, 4, 4, 4, 4, 4, 4,
	(5),  5,   5,  5,  5,  5, 5, 5,  5,  5, 5, 5, 5, 5, 5, 5,
	(6),  6,   6,  6,  6,  6, 6, 6,  6,  6, 6, 6, 6, 6, 6, 6,
	 6,   6,   6,  6,  6,  6, 6, 6,  6,  6, 6, 6, 6, 6, 6, 6,

	(7),  7,   7,  7,  7,  7, 7, 7,  7,  7, 7, 7, 7, 7, 7, 7,
	 7,   7,   7,  7,  7,  7, 7, 7,  7,  7, 7, 7, 7, 7, 7, 7,
	 7,   7,   7,  7,  7,  7, 7, 7,  7,  7, 7, 7, 7, 7, 7, 7,
	 7,   7,   7,  7,  7,  7, 7, 7,  7,  7, 7, 7, 7, 7, 7, 7,

	(8),  8,   8,  8,  8,  8, 8, 8,  8,  8, 8, 8, 8, 8, 8, 8,
	 8,   8,   8,  8,  8,  8, 8, 8,  8,  8, 8, 8, 8, 8, 8, 8,
	 8,   8,   8,  8,  8,  8, 8, 8,  8,  8, 8, 8, 8, 8, 8, 8,
	 8,   8,   8,  8,  8,  8, 8, 8,  8,  8, 8, 8, 8, 8, 8, 8,

	 8,   8,   8,  8,  8,  8, 8, 8,  8,  8, 8, 8, 8, 8, 8, 8,
	 8,   8,   8,  8,  8,  8, 8, 8,  8,  8, 8, 8, 8, 8, 8, 8,
	 8,   8,   8,  8,  8,  8, 8, 8,  8,  8, 8, 8, 8, 8, 8, 8,
	 8,   8,   8,  8,  8,  8, 8, 8,  8,  8, 8, 8, 8, 8, 8, 8
};


/* yod_stdlog_level_name */
static const char *yod_stdlog_level_name[] = {
	"-", "TRACE", "DEBUG", "INFO", "NOTICE", "WARN", "ERROR", "ALERT", "FATAL", "-"
};


#ifdef WIN32
#undef pthread_mutex_lock
#define pthread_mutex_lock(x) 	{ if (*x == 0) { *x = CreateMutex(NULL, FALSE, NULL); } WaitForSingleObject(*x, INFINITE); }
#endif


#define yod_stdlog_hook_new() 									_yod_stdlog_hook_new(__ENV_ARGS)
#define yod_stdlog_hook_init(x, l) 								_yod_stdlog_hook_init(x, l __ENV_CARGS)
#define yod_stdlog_hook_write(x, l, d) 							_yod_stdlog_hook_write(x, l, d __ENV_CARGS)
#define yod_stdlog_hook_free(x) 								_yod_stdlog_hook_free(x __ENV_CARGS)


static int _yod_stdlog_hook_cb(short what, short level, char *data, void *arg __ENV_CPARM);
static int _yod_stdlog_hook_init(yod_stdlog_t *self, short level __ENV_CPARM);
static int _yod_stdlog_hook_write(yod_stdlog_t *self, short level, char *data __ENV_CPARM);
static int _yod_stdlog_hook_free(yod_stdlog_t *self __ENV_CPARM);

static int yod_stdlog_write_log(yod_stdlog_t *self, short level, char *data, size_t data_len);


/* yod_stdlog_root__ */
static yod_stdlog_t yod_stdlog_root__ = {
#ifdef _WIN32
	0,
#else
	PTHREAD_MUTEX_INITIALIZER,
#endif
	NULL,
	__LOG_TRACE,
	{
		(char *) YOD_STDLOG_FORMAT_ALL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
	},
	_yod_stdlog_hook_cb,
	NULL,
	NULL,
	NULL
};


/** {{{ yod_stdlog_t *_yod_stdlog_new(const char *name, short level,
	yod_stdlog_fn func, void *arg __ENV_CPARM)
*/
yod_stdlog_t *_yod_stdlog_new(const char *name, short level,
	yod_stdlog_fn func, void *arg __ENV_CPARM)
{
	yod_stdlog_t *root = &yod_stdlog_root__;
	yod_stdlog_t *self = NULL;
	int i = 0;

	self = (yod_stdlog_t *) malloc(sizeof(yod_stdlog_t));
	if (!self) {
		YOD_STDLOG_ERROR("malloc failed");
		return NULL;
	}

	if (pthread_mutex_init(&self->lock, NULL) != 0) {
		free(self);

		YOD_STDLOG_ERROR("pthread_mutex_init failed");
		return NULL;
	}

	for (i = 0; i < 9; ++i) {
		self->format[i] = NULL;
	}

	self->name = name ? strdup(name) : NULL;
	self->level = level;

	self->func = func;
	self->arg = arg;

	pthread_mutex_lock(&root->lock);
	self->next = root->next;
	self->prev = NULL;
	if (self->next) {
		self->next->prev = self;
	}
	root->next = self;
	pthread_mutex_unlock(&root->lock);

	if (self->func) {
		self->func(YOD_STDLOG_STATE_INIT, self->level, NULL, self->arg __ENV_CARGS);
	} else {
		_yod_stdlog_hook_cb(YOD_STDLOG_STATE_INIT, self->level, NULL, self->arg __ENV_CARGS);
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_STDLOG_DEBUG)
	yod_stdlog_debug(NULL, "%s(%s, %d): %p in %s:%d %s",
		__FUNCTION__, name, level, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return self;
}
/* }}} */


/** {{{ void _yod_stdlog_free(yod_stdlog_t *self __ENV_CPARM)
*/
void _yod_stdlog_free(yod_stdlog_t *self __ENV_CPARM)
{
	yod_stdlog_t *root = &yod_stdlog_root__;
	int i = 0;
	
	if (!self) {
		return;
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_STDLOG_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p) in %s:%d %s",
		__FUNCTION__, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	pthread_mutex_lock(&self->lock);

	pthread_mutex_lock(&root->lock);

	if (self->next) {
		self->next->prev = self->prev;
	}
	if (self->prev) {
		self->prev->next = self->next;
	} else {
		root->next = self->next;
	}

	pthread_mutex_unlock(&root->lock);

	if (self->func) {
		self->func(YOD_STDLOG_STATE_FREE, self->level, NULL, self->arg __ENV_CARGS);
	} else {
		_yod_stdlog_hook_cb(YOD_STDLOG_STATE_FREE, self->level, NULL, self->arg __ENV_CARGS);
	}

	for (i = 0; i < 9; ++i) {
		if (self->format[i]) {
			free(self->format[i]);
		}
	}

	if (self->name) {
		free(self->name);
	}

	pthread_mutex_unlock(&self->lock);
	pthread_mutex_destroy(&self->lock);

	free(self);
}
/* }}} */


/** {{{ int _yod_stdlog_level(yod_stdlog_t *self, short level __ENV_CPARM)
*/
int _yod_stdlog_level(yod_stdlog_t *self, short level __ENV_CPARM)
{
#if (_YOD_SYSTEM_DEBUG && _YOD_STDLOG_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d) in %s:%d %s",
		__FUNCTION__, self, level, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (!self) {
		self = &yod_stdlog_root__;
	}

	self->level = level;

	return (0);
}
/* }}} */


/** {{{ int _yod_stdlog_format(yod_stdlog_t *self, short level, char *format __ENV_CPARM)
*/
int _yod_stdlog_format(yod_stdlog_t *self, short level, char *format __ENV_CPARM)
{
	short level_key = 0;

	if (!format) {
		return (-1);
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_STDLOG_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d, %s) in %s:%d %s",
		__FUNCTION__, self, level, format, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (!self) {
		self = &yod_stdlog_root__;
	}

	if (yod_stdlog_level_key[level]) {
		level_key = yod_stdlog_level_key[level];
		if (self->format[level_key]) {
			free(self->format[level_key]);
		}
		self->format[level_key] = format ? strdup(format) : NULL;
	}

	return (0);
}
/* }}} */


/** {{{ void _yod_stdlog_destroy(__ENV_PARM)
*/
void _yod_stdlog_destroy(__ENV_PARM)
{
	yod_stdlog_t *root = &yod_stdlog_root__;
	yod_stdlog_t *self = NULL;
	int i = 0;

#if (_YOD_SYSTEM_DEBUG && _YOD_STDLOG_DEBUG)
	yod_stdlog_debug(NULL, "%s() in %s:%d %s", __FUNCTION__, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	while ((self = root->next) != NULL) {
		root->next = self->next;

		if (self->func) {
			self->func(YOD_STDLOG_STATE_FREE, self->level, NULL, self->arg __ENV_CARGS);
		} else {
			_yod_stdlog_hook_cb(YOD_STDLOG_STATE_FREE, self->level, NULL, self->arg __ENV_CARGS);
		}

		for (i = 0; i < 9; ++i) {
			if (self->format[i]) {
				free(self->format[i]);
			}
		}

		if (self->name) {
			free(self->name);
		}

		pthread_mutex_destroy(&self->lock);

		free(self);
	}

#ifdef _WIN32
	pthread_mutex_destroy(&root->lock);
#endif
}
/* }}} */


/** {{{ int yod_stdlog_write(yod_stdlog_t *self, short level, const char *fmt, ...)
*/
int yod_stdlog_write(yod_stdlog_t *self, short level, const char *fmt, ...)
{
	char buf[YOD_STDLOG_BUFFER_MAX];
	char *data = NULL;
	va_list args;
	int ret = 0;

	if (!self) {
		self = &yod_stdlog_root__;
	}

	if ((self->level > 0) && (self->level <= level)) {
		va_start(args, fmt);
#ifdef _WIN32
		ret = vsnprintf_s(buf, YOD_STDLOG_BUFFER_MAX, _TRUNCATE, fmt, args);
#else
		ret = vsnprintf(buf, YOD_STDLOG_BUFFER_MAX, fmt, args);
#endif
		va_end(args);

		if (ret > 0) {
			data = (char *) malloc((ret + 1) * sizeof(char));
			if (!data) {
				YOD_STDLOG_ERROR("malloc failed");
				return (-1);
			}
			memcpy(data, buf, ret);
			data[ret] = '\0';

			ret = yod_stdlog_write_log(self, level, data, ret);
			free(data);
		}
	}

	return ret;
}
/* }}} */


/** {{{ int yod_stdlog_hex(yod_stdlog_t *self, short level, const char *str, byte *blob, size_t len)
*/
int yod_stdlog_hex(yod_stdlog_t *self, short level, const char *str, byte *blob, size_t len)
{
	char *data = NULL;
	int ret = 0;
	size_t i = 0;
	int n = 0;

	if (!self) {
		self = &yod_stdlog_root__;
	}

	if ((self->level > 0) && (self->level <= level)) {
		if (str) {
			n = (int) strlen(str);
		}
		ret = (int) (n + len * 3);
		data = (char *) malloc((ret + 1) * sizeof(char));
		if (!data) {
			YOD_STDLOG_ERROR("malloc failed");
			return (-1);
		}

		if (n > 0) {
			memcpy(data, str, n);
		}
		while (i < len) {
			*(data + n + i * 3) = yod_common_byte2hex(blob[i] >> 4, 1);
			*(data + n + i * 3 + 1) = yod_common_byte2hex(blob[i] & 0x0F, 1);
			*(data + n + i * 3 + 2) = 32;
			++ i;
		}
		data[ret] = '\0';

		ret = yod_stdlog_write_log(self, level, data, ret);
		free(data);
	}

	return ret;
}
/* }}} */


/** {{{ int yod_stdlog_fatal(yod_stdlog_t *self, const char *fmt, ...)
*/
int yod_stdlog_fatal(yod_stdlog_t *self, const char *fmt, ...)
{
	char buf[YOD_STDLOG_BUFFER_MAX];
	char *data = NULL;
	va_list args;
	int ret = 0;

	if (!self) {
		self = &yod_stdlog_root__;
	}

	if ((self->level > 0) && (self->level <= YOD_STDLOG_LEVEL_FATAL)) {
		va_start(args, fmt);
#ifdef _WIN32
		ret = vsnprintf_s(buf, YOD_STDLOG_BUFFER_MAX, _TRUNCATE, fmt, args);
#else
		ret = vsnprintf(buf, YOD_STDLOG_BUFFER_MAX, fmt, args);
#endif
		va_end(args);

		if (ret > 0) {
			data = (char *) malloc((ret + 1) * sizeof(char));
			if (!data) {
				YOD_STDLOG_ERROR("malloc failed");
				return (-1);
			}
			memcpy(data, buf, ret);
			data[ret] = '\0';

			ret = yod_stdlog_write_log(self, YOD_STDLOG_LEVEL_FATAL, data, ret);
			free(data);
		}
	}

	return ret;
}
/* }}} */


/** {{{ int yod_stdlog_alert(yod_stdlog_t *self, const char *fmt, ...)
*/
int yod_stdlog_alert(yod_stdlog_t *self, const char *fmt, ...)
{
	char buf[YOD_STDLOG_BUFFER_MAX];
	char *data = NULL;
	va_list args;
	int ret = 0;

	if (!self) {
		self = &yod_stdlog_root__;
	}

	if ((self->level > 0) && (self->level <= YOD_STDLOG_LEVEL_ALERT)) {
		va_start(args, fmt);
#ifdef _WIN32
		ret = vsnprintf_s(buf, YOD_STDLOG_BUFFER_MAX, _TRUNCATE, fmt, args);
#else
		ret = vsnprintf(buf, YOD_STDLOG_BUFFER_MAX, fmt, args);
#endif
		va_end(args);

		if (ret > 0) {
			data = (char *) malloc((ret + 1) * sizeof(char));
			if (!data) {
				YOD_STDLOG_ERROR("malloc failed");
				return (-1);
			}
			memcpy(data, buf, ret);
			data[ret] = '\0';

			ret = yod_stdlog_write_log(self, YOD_STDLOG_LEVEL_ALERT, data, ret);
			free(data);
		}
	}

	return ret;
}
/* }}} */



/** {{{ int yod_stdlog_error(yod_stdlog_t *self, const char *fmt, ...)
*/
int yod_stdlog_error(yod_stdlog_t *self, const char *fmt, ...)
{
	char buf[YOD_STDLOG_BUFFER_MAX];
	char *data = NULL;
	va_list args;
	int ret = 0;

	if (!self) {
		self = &yod_stdlog_root__;
	}

	if ((self->level > 0) && (self->level <= YOD_STDLOG_LEVEL_ERROR)) {
		va_start(args, fmt);
#ifdef _WIN32
		ret = vsnprintf_s(buf, YOD_STDLOG_BUFFER_MAX, _TRUNCATE, fmt, args);
#else
		ret = vsnprintf(buf, YOD_STDLOG_BUFFER_MAX, fmt, args);
#endif
		va_end(args);

		if (ret > 0) {
			data = (char *) malloc((ret + 1) * sizeof(char));
			if (!data) {
				YOD_STDLOG_ERROR("malloc failed");
				return (-1);
			}
			memcpy(data, buf, ret);
			data[ret] = '\0';

			ret = yod_stdlog_write_log(self, YOD_STDLOG_LEVEL_ERROR, data, ret);
			free(data);
		}
	}

	return ret;
}
/* }}} */



/** {{{ int yod_stdlog_warn(yod_stdlog_t *self, const char *fmt, ...)
*/
int yod_stdlog_warn(yod_stdlog_t *self, const char *fmt, ...)
{
	char buf[YOD_STDLOG_BUFFER_MAX];
	char *data = NULL;
	va_list args;
	int ret = 0;

	if (!self) {
		self = &yod_stdlog_root__;
	}

	if ((self->level > 0) && (self->level <= YOD_STDLOG_LEVEL_WARN)) {
		va_start(args, fmt);
#ifdef _WIN32
		ret = vsnprintf_s(buf, YOD_STDLOG_BUFFER_MAX, _TRUNCATE, fmt, args);
#else
		ret = vsnprintf(buf, YOD_STDLOG_BUFFER_MAX, fmt, args);
#endif
		va_end(args);

		if (ret > 0) {
			data = (char *) malloc((ret + 1) * sizeof(char));
			if (!data) {
				YOD_STDLOG_ERROR("malloc failed");
				return (-1);
			}
			memcpy(data, buf, ret);
			data[ret] = '\0';

			ret = yod_stdlog_write_log(self, YOD_STDLOG_LEVEL_WARN, data, ret);
			free(data);
		}
	}

	return ret;
}
/* }}} */



/** {{{ int yod_stdlog_notice(yod_stdlog_t *self, const char *fmt, ...)
*/
int yod_stdlog_notice(yod_stdlog_t *self, const char *fmt, ...)
{
	char buf[YOD_STDLOG_BUFFER_MAX];
	char *data = NULL;
	va_list args;
	int ret = 0;

	if (!self) {
		self = &yod_stdlog_root__;
	}

	if ((self->level > 0) && (self->level <= YOD_STDLOG_LEVEL_NOTICE)) {
		va_start(args, fmt);
#ifdef _WIN32
		ret = vsnprintf_s(buf, YOD_STDLOG_BUFFER_MAX, _TRUNCATE, fmt, args);
#else
		ret = vsnprintf(buf, YOD_STDLOG_BUFFER_MAX, fmt, args);
#endif
		va_end(args);

		if (ret > 0) {
			data = (char *) malloc((ret + 1) * sizeof(char));
			if (!data) {
				YOD_STDLOG_ERROR("malloc failed");
				return (-1);
			}
			memcpy(data, buf, ret);
			data[ret] = '\0';

			ret = yod_stdlog_write_log(self, YOD_STDLOG_LEVEL_NOTICE, data, ret);
			free(data);
		}
	}

	return ret;
}
/* }}} */



/** {{{ int yod_stdlog_info(yod_stdlog_t *self, const char *fmt, ...)
*/
int yod_stdlog_info(yod_stdlog_t *self, const char *fmt, ...)
{
	char buf[YOD_STDLOG_BUFFER_MAX];
	char *data = NULL;
	va_list args;
	int ret = 0;

	if (!self) {
		self = &yod_stdlog_root__;
	}

	if ((self->level > 0) && (self->level <= YOD_STDLOG_LEVEL_INFO)) {
		va_start(args, fmt);
#ifdef _WIN32
		ret = vsnprintf_s(buf, YOD_STDLOG_BUFFER_MAX, _TRUNCATE, fmt, args);
#else
		ret = vsnprintf(buf, YOD_STDLOG_BUFFER_MAX, fmt, args);
#endif
		va_end(args);

		if (ret > 0) {
			data = (char *) malloc((ret + 1) * sizeof(char));
			if (!data) {
				YOD_STDLOG_ERROR("malloc failed");
				return (-1);
			}
			memcpy(data, buf, ret);
			data[ret] = '\0';

			ret = yod_stdlog_write_log(self, YOD_STDLOG_LEVEL_INFO, data, ret);
			free(data);
		}
	}

	return ret;
}
/* }}} */



/** {{{ int yod_stdlog_debug(yod_stdlog_t *self, const char *fmt, ...)
*/
int yod_stdlog_debug(yod_stdlog_t *self, const char *fmt, ...)
{
	char buf[YOD_STDLOG_BUFFER_MAX];
	char *data = NULL;
	va_list args;
	int ret = 0;

	if (!self) {
		self = &yod_stdlog_root__;
	}

	if ((self->level > 0) && (self->level <= YOD_STDLOG_LEVEL_DEBUG)) {
		va_start(args, fmt);
#ifdef _WIN32
		ret = vsnprintf_s(buf, YOD_STDLOG_BUFFER_MAX, _TRUNCATE, fmt, args);
#else
		ret = vsnprintf(buf, YOD_STDLOG_BUFFER_MAX, fmt, args);
#endif
		va_end(args);

		if (ret > 0) {
			data = (char *) malloc((ret + 1) * sizeof(char));
			if (!data) {
				YOD_STDLOG_ERROR("malloc failed");
				return (-1);
			}
			memcpy(data, buf, ret);
			data[ret] = '\0';

			ret = yod_stdlog_write_log(self, YOD_STDLOG_LEVEL_DEBUG, data, ret);
			free(data);
		}
	}

	return ret;
}
/* }}} */


/** {{{ int yod_stdlog_trace(yod_stdlog_t *self, const char *fmt, ...)
*/
int yod_stdlog_trace(yod_stdlog_t *self, const char *fmt, ...)
{
	char buf[YOD_STDLOG_BUFFER_MAX];
	char *data = NULL;
	va_list args;
	int ret = 0;

	if (!self) {
		self = &yod_stdlog_root__;
	}

	if ((self->level > 0) && (self->level <= YOD_STDLOG_LEVEL_TRACE)) {
		va_start(args, fmt);
#ifdef _WIN32
		ret = vsnprintf_s(buf, YOD_STDLOG_BUFFER_MAX, _TRUNCATE, fmt, args);
#else
		ret = vsnprintf(buf, YOD_STDLOG_BUFFER_MAX, fmt, args);
#endif
		va_end(args);

		if (ret > 0) {
			data = (char *) malloc((ret + 1) * sizeof(char));
			if (!data) {
				YOD_STDLOG_ERROR("malloc failed");
				return (-1);
			}
			memcpy(data, buf, ret);
			data[ret] = '\0';

			ret = yod_stdlog_write_log(self, YOD_STDLOG_LEVEL_TRACE, data, ret);
			free(data);
		}
	}

	return ret;
}
/* }}} */


/** {{{ int yod_stdlog_dump(yod_stdlog_t *self, const char *fmt, ...)
*/
int yod_stdlog_dump(yod_stdlog_t *self, const char *fmt, ...)
{
	char buf[YOD_STDLOG_BUFFER_MAX];
	char *data = NULL;
	va_list args;
	int ret = 0;

	if (!self) {
		self = &yod_stdlog_root__;
	}

	if (self->level > 0) {
		va_start(args, fmt);
#ifdef _WIN32
		ret = vsnprintf_s(buf, YOD_STDLOG_BUFFER_MAX, _TRUNCATE, fmt, args);
#else
		ret = vsnprintf(buf, YOD_STDLOG_BUFFER_MAX, fmt, args);
#endif
		va_end(args);

		if (ret > 0) {
			data = (char *) malloc((ret + 1) * sizeof(char));
			if (!data) {
				YOD_STDLOG_ERROR("malloc failed");
				return (-1);
			}
			memcpy(data, buf, ret);
			data[ret] = '\0';

			ret = yod_stdlog_write_log(self, 0xFFFF, data, ret);
			free(data);
		}
	}

	return ret;
}
/* }}} */


/** {{{ static int _yod_stdlog_hook_cb(short what, short level, char *data, void *arg __ENV_CPARM)
*/
static int _yod_stdlog_hook_cb(short what, short level, char *data, void *arg __ENV_CPARM)
{
	yod_stdlog_t *self = arg;
	int ret = 0;

	if (what == YOD_STDLOG_STATE_INIT) {
		ret = yod_stdlog_hook_init(self, level);
	}
	else if (what == YOD_STDLOG_STATE_WRITE) {
		ret = yod_stdlog_hook_write(self, level, data);
	}
	else if (what == YOD_STDLOG_STATE_FREE) {
		ret = yod_stdlog_hook_free(self);
	}

	__ENV_VOID
	return ret;
}
/* }}} */


/** {{{ static int _yod_stdlog_hook_init(yod_stdlog_t *self, short level __ENV_CPARM)
*/
static int _yod_stdlog_hook_init(yod_stdlog_t *self, short level __ENV_CPARM)
{
	if (!self) {
		return (-1);
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_STDLOG_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d) in %s:%d %s",
		__FUNCTION__, self, level, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	(void) level;

	return (0);
}
/* }}} */


/** {{{ static int _yod_stdlog_hook_free(yod_stdlog_t *self __ENV_CPARM)
*/
static int _yod_stdlog_hook_free(yod_stdlog_t *self __ENV_CPARM)
{
#if (_YOD_SYSTEM_DEBUG && _YOD_STDLOG_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p) in %s:%d %s",
		__FUNCTION__, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (!self) {
		return (-1);
	}

	return (0);
}
/* }}} */


/** {{{ static int _yod_stdlog_hook_write(yod_stdlog_t *self, short level, char *data __ENV_CPARM)
*/
static int _yod_stdlog_hook_write(yod_stdlog_t *self, short level, char *data __ENV_CPARM)
{
	int ret = 0;

	if (!data) {
		return (-1);
	}

	if ((level & 0x00FF) == 0) {
		return 0;
	}

#ifdef __ANDROID__
	switch (level) {
		case __LOG_FATAL:
			__android_log_write(ANDROID_LOG_FATAL, "FATAL", data);
			break;

		case __LOG_ALERT:
			__android_log_write(ANDROID_LOG_ERROR, "ALERT", data);
			break;
			
		case __LOG_ERROR:
			__android_log_write(ANDROID_LOG_ERROR, "ERROR", data);
			break;
			
		case __LOG_WARN:
			__android_log_write(ANDROID_LOG_WARN, "WARN", data);
			break;
			
		case __LOG_NOTICE:
			__android_log_write(ANDROID_LOG_WARN, "NOTICE", data);
			break;
			
		case __LOG_INFO:
			__android_log_write(ANDROID_LOG_INFO, "INFO", data);
			break;
			
		case __LOG_DEBUG:
			__android_log_write(ANDROID_LOG_DEBUG, "DEBUG", data);
			break;
			
		case __LOG_TRACE:
			__android_log_write(ANDROID_LOG_VERBOSE, "TRACE", data);
			break;

		default:
			__android_log_write(ANDROID_LOG_VERBOSE, "DUMP", data);
			break;
	}
#elif defined(IOS)
	NSLog(@"%s", data);
#else
	if (level >= __LOG_WARN && level <= __LOG_FATAL) {
		ret = fputs(data, stderr);
		fflush(stderr);
	}

	ret = fputs(data, stdout);
	fflush(stdout);
#endif

	(void) self;

	__ENV_VOID

	return ret;
}
/* }}} */


/** {{{ static yod_stdlog_write_log(yod_stdlog_t *self, short level, char *data, size_t data_len)
*/
static int yod_stdlog_write_log(yod_stdlog_t *self, short level, char *data, size_t data_len)
{
	char buf[YOD_STDLOG_BUFFER_MAX] = {0};
	short level_key = 0;
	char *format = NULL;
	char *temp = NULL;
	size_t temp_len = 0;
	int ret = 0;

	if ((level & 0x00FF) == 0) {
		return 0;
	}
	else if (level < 0x0000 || level > 0x00FF) {
		pthread_mutex_lock(&self->lock);
		ret = _yod_stdlog_hook_cb(YOD_STDLOG_STATE_WRITE, level, data, self->arg __ENV_CARGS);
		pthread_mutex_unlock(&self->lock);
		return ret;
	}

	if (self->level > level) {
		return (0);
	}

	if (yod_stdlog_level_key[level]) {
		level_key = yod_stdlog_level_key[level];
	}

	pthread_mutex_lock(&self->lock);

	if (self->format[level_key]) {
		format = self->format[level_key];
	}
	else if(self->format[0]) {
		format = self->format[0];
	}
	else {
		format = (char *) YOD_STDLOG_FORMAT_ALL;
	}

	/* parse format to buffer */
	{
		char stime[28] = {0};
		char thread[19] = {0};
		char *level = NULL;
		int flag = 0;

		yod_common_strtime(stime);

		snprintf(thread, sizeof(thread), "%09lX", (ulong) pthread_self());

		if (yod_stdlog_level_name[level_key]) {
			level = (char *) yod_stdlog_level_name[level_key];
		}

		temp = buf;
		temp_len = 0;

		while (*format != 0) {
			if (temp_len > YOD_STDLOG_BUFFER_MAX) {
				break;
			}

			if (flag == 1) {
				switch (*format) {
					case 'T': // time
						memcpy(temp, stime, 26);
						temp_len += 26;
						temp += 26;
						break;

					case 'Y': // year
						memcpy(temp, stime, 4);
						temp_len += 4;
						temp += 4;
						break;

					case 'M': // month
						memcpy(temp, stime + 5, 2);
						temp_len += 2;
						temp += 2;
						break;

					case 'D': // day
						memcpy(temp, stime + 8, 2);
						temp_len += 2;
						temp += 2;
						break;

					case 'H': // hour
						memcpy(temp, stime + 11, 2);
						temp_len += 2;
						temp += 2;
						break;

					case 'I': // minute
						memcpy(temp, stime + 14, 2);
						temp_len += 2;
						temp += 2;
						break;

					case 'S': // second
						memcpy(temp, stime + 17, 2);
						temp_len += 2;
						temp += 2;
						break;

					case 'U': // second
						memcpy(temp, stime + 20, 6);
						temp_len += 2;
						temp += 2;
						break;

					case 'N': // name
						if (self->name) {
							ret = (int) strlen(self->name);
							memcpy(temp, self->name, ret);
							temp_len += ret;
							temp += ret;
						} else {
							*temp++ = '-';
							++ temp_len;
						}
						break;

					case 'L': // level
						if (level) {
							ret = (int) strlen(level);
							memcpy(temp, level, ret);
							temp_len += ret;
							temp += ret;
						} else {
							*temp++ = '-';
							++ temp_len;
						}
						break;

					case 'P': // thread
						ret = (int) strlen(thread);
						if (ret > 0) {
							memcpy(temp, thread, ret);
							temp_len += ret;
							temp += ret;
						}
						break;

					case 'X': // data
						if (data) {
							if (temp_len + data_len > YOD_STDLOG_BUFFER_MAX) {
								data_len = YOD_STDLOG_BUFFER_MAX - temp_len;
							}
							memcpy(temp, data, data_len);
							temp_len += data_len;
							temp += data_len;
						}
						break;

					default :
						*temp++ = '$';
						*temp++ = *format;
						++ temp_len;
						break;
				}

				flag = 0;
			}
			else {
				if (*format == '$') {
					flag = 1;
				} else {
					*temp++ = *format;
					++ temp_len;
				}
			}

			++ format;
		}
		*temp = '\0';
		++ temp_len;
	}

	if (self->func) {
		ret = self->func(YOD_STDLOG_STATE_WRITE, level, buf, self->arg __ENV_CARGS);
	} else {
		ret = _yod_stdlog_hook_cb(YOD_STDLOG_STATE_WRITE, level, buf, self->arg __ENV_CARGS);
	}

	pthread_mutex_unlock(&self->lock);

	return ret;
}
/* }}} */
