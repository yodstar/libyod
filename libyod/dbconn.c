#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <errno.h>

#include "stdlog.h"
#include "dbconn.h"


#ifndef _YOD_DBCONN_DEBUG
#define _YOD_DBCONN_DEBUG 											0
#endif


#define YOD_DBCONN_SQL_LEN 											5120


enum
{
	YOD_DBCONN_STATE_IDLE,
	YOD_DBCONN_STATE_HOLD,
};


/* yod_dbconn_t */
struct _yod_dbconn_t
{
	pthread_mutex_t lock;

	ulong count;
	char *dburl;
	short state;

	ulong lastid;
	ulong rowsnum;

	MYSQL link;
	yod_dbconn_s *stmt;
	MYSQL_BIND *bind;

	yod_dbconn_t *root;
	yod_dbconn_t *next;
	yod_dbconn_t *prev;
};

/* yod_dbconn_s */
struct _yod_dbconn_s
{
	uint count;
	short state;

	MYSQL *link;
	MYSQL_RES *meta;
	MYSQL_STMT *stmt;
	MYSQL_BIND *bind;
	yod_string_t *data;
};


#define _yod_dbconn_stmt_fetch 										_yod_dbconn_fetch


static int _yod_dbconn_bind_dburl(char *dburl, char **host, uint *port, char **user, char **passwd, char **dbname);
static int _yod_dbconn_stmt_query(yod_dbconn_t *self, const char *sql, ulong len, va_list args __ENV_CPARM);
static int _yod_dbconn_stmt_clean(yod_dbconn_s *stmt);


// yod_dbconn_init__
static int yod_dbconn_init__ = 0;


/** {{{ yod_dbconn_t *_yod_dbconn_new(const char *dburl __ENV_CPARM)
*/
yod_dbconn_t *_yod_dbconn_new(const char *dburl __ENV_CPARM)
{
	yod_dbconn_t *self = NULL;

	if (!dburl) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return NULL;
	}

	if (!yod_dbconn_init__) {
		++ yod_dbconn_init__;
		if (mysql_library_init(0, NULL, NULL)) {
			YOD_STDLOG_WARN("mysql_library_init failed");
			return NULL;
		}
	}

	self = (yod_dbconn_t *) malloc(sizeof(yod_dbconn_t));
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
	self->dburl = strdup(dburl);
	self->state = 0;

	self->lastid = 0;
	self->rowsnum = 0;

	self->stmt = NULL;
	self->bind = NULL;

	self->root = self;
	self->next = NULL;
	self->prev = NULL;

#if (_YOD_SYSTEM_DEBUG && _YOD_DBCONN_DEBUG)
	yod_stdlog_debug(NULL, "%s(%s): %p in %s:%d %s",
		__FUNCTION__, dburl, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return self;
}
/* }}} */


/** {{{ void _yod_dbconn_free(yod_dbconn_t *self __ENV_CPARM)
*/
void _yod_dbconn_free(yod_dbconn_t *self __ENV_CPARM)
{
	yod_dbconn_t *root = NULL;
	yod_dbconn_t *conn = NULL;

#if (_YOD_SYSTEM_DEBUG && _YOD_DBCONN_DEBUG)
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

	while ((conn = root->next) != NULL) {
		root->next = conn->next;
		pthread_mutex_lock(&conn->lock);
		if (conn->dburl) {
			free(conn->dburl);
		}
		if (conn->stmt) {
			if (conn->stmt->bind) {
				free(conn->stmt->bind);
			}
			if (conn->stmt->data) {
				free(conn->stmt->data);
			}
			free(conn->stmt);
		}
		if (conn->bind) {
			free(conn->bind);
		}
		mysql_close(&conn->link);
		pthread_mutex_unlock(&conn->lock);
		pthread_mutex_destroy(&conn->lock);
		free(conn);
	}

	if (root->dburl) {
		free(root->dburl);
	}

	pthread_mutex_unlock(&root->lock);
	pthread_mutex_destroy(&root->lock);

	if (yod_dbconn_init__) {
		-- yod_dbconn_init__;
		if (yod_dbconn_init__ == 0) {
			mysql_library_end();
		}
	}

	free(root);
}
/* }}} */


/** {{{ yod_dbconn_t *_yod_dbconn_open(yod_dbconn_t *self __ENV_CPARM)
*/
yod_dbconn_t *_yod_dbconn_open(yod_dbconn_t *self __ENV_CPARM)
{
	yod_dbconn_t *root = NULL;
	yod_dbconn_t *conn = NULL;

	char reconn = 1;
	char *host, *user, *passwd, *dbname;
	uint port = 0;

	if (!self || !self->root) {
		return NULL;
	}
	root = self->root;

	pthread_mutex_lock(&root->lock);
	for (conn = root->next; conn != NULL; conn = conn->next) {
		pthread_mutex_lock(&conn->lock);
		if (conn->state == YOD_DBCONN_STATE_IDLE) {
			conn->state = YOD_DBCONN_STATE_HOLD;
			if (mysql_ping(&conn->link) != 0) {
				conn->state = YOD_DBCONN_STATE_IDLE;
			}
			pthread_mutex_unlock(&conn->lock);
			if (conn->state == YOD_DBCONN_STATE_IDLE) {
				pthread_mutex_unlock(&root->lock);
				return NULL;
			}
			break;
		}
		pthread_mutex_unlock(&conn->lock);
	}
	pthread_mutex_unlock(&root->lock);

	if (!conn) {
		conn = (yod_dbconn_t *) malloc(sizeof(yod_dbconn_t));
		if (!conn) {
			YOD_STDLOG_ERROR("malloc failed");
			return NULL;
		}

		conn->dburl = strdup(root->dburl);
		if (_yod_dbconn_bind_dburl(conn->dburl, &host, &port, &user, &passwd, &dbname) != 0) {
			free(conn->dburl);
			free(conn);

			YOD_STDLOG_ERROR("dburl failed");
			return NULL;
		}

		if (pthread_mutex_init(&conn->lock, NULL) != 0) {
			free(conn->dburl);
			free(conn);

			YOD_STDLOG_ERROR("pthread_mutex_init failed");
			return NULL;
		}

		pthread_mutex_lock(&root->lock);
		{
			conn->count = 0;
			mysql_init(&conn->link);
			mysql_options(&conn->link, MYSQL_OPT_RECONNECT, (char *)&reconn);
			mysql_options(&conn->link, MYSQL_SET_CHARSET_NAME, (char *)"utf8");
			if (mysql_real_connect(&conn->link, host, user, passwd, dbname, port, NULL, 0)) {
				conn->state = YOD_DBCONN_STATE_HOLD;
				conn->lastid = 0;
				conn->rowsnum = 0;
				conn->stmt = NULL;
				conn->bind = NULL;
				conn->root = root;
				conn->next = root->next;
				conn->prev = NULL;
				if (conn->next) {
					conn->next->prev = conn;
				}
				root->next = conn;
				++ root->count;
			}
			else {
				yod_stdlog_error(NULL, "%s (%lu), errno=%d in %s:%d %s",
					mysql_error(&conn->link), root->count, mysql_errno(&conn->link), __ENV_TRACE3);
				mysql_close(&conn->link);
				pthread_mutex_destroy(&conn->lock);
				free(conn->dburl);
				free(conn);
				conn = NULL;
			}
		}
		pthread_mutex_unlock(&root->lock);

	}

#if (_YOD_SYSTEM_DEBUG && _YOD_DBCONN_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): %p in %s:%d %s",
		__FUNCTION__, self, conn, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return conn;
}
/* }}} */


/** {{{ int _yod_dbconn_close(yod_dbconn_t *self __ENV_CPARM)
*/
int _yod_dbconn_close(yod_dbconn_t *self __ENV_CPARM)
{
	if (!self || !self->root) {
		return (-1);
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_DBCONN_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p) in %s:%d %s",
		__FUNCTION__, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	pthread_mutex_lock(&self->lock);
	self->state = YOD_DBCONN_STATE_IDLE;
	pthread_mutex_unlock(&self->lock);

	return (0);
}
/* }}} */


/** {{{ ulong _yod_dbconn_lastid(yod_dbconn_t *self __ENV_CPARM)
*/
ulong _yod_dbconn_lastid(yod_dbconn_t *self __ENV_CPARM)
{
	ulong ret = 0;

	if (!self || !self->root) {
		return (0);
	}
	ret = self->lastid;

#if (_YOD_SYSTEM_DEBUG && _YOD_DBCONN_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): %lu in %s:%d %s",
		__FUNCTION__, self, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ ulong _yod_dbconn_rowsnum(yod_dbconn_t *self __ENV_CPARM)
*/
ulong _yod_dbconn_rowsnum(yod_dbconn_t *self __ENV_CPARM)
{
	ulong ret = 0;

	if (!self || !self->root) {
		return (0);
	}
	ret = self->rowsnum;

#if (_YOD_SYSTEM_DEBUG && _YOD_DBCONN_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): %lu in %s:%d %s",
		__FUNCTION__, self, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ int yod_dbconn_escape(yod_dbconn_t *self, char *out, const char *str, size_t len)
*/
ulong yod_dbconn_escape(yod_dbconn_t *self, char *out, const char *str, size_t len)
{
	ulong ret = 0;

	if (self && out && str) {
		ret = mysql_real_escape_string(&self->link, out, str, (ulong) len);
	}
	return ret;
}
/* }}} */


/** {{{ int _yod_dbconn_begin(yod_dbconn_t *self __ENV_CPARM)
*/
int _yod_dbconn_begin(yod_dbconn_t *self __ENV_CPARM)
{
	int ret = 0;

	if (!self || !self->root) {
		return (-1);
	}
	ret = mysql_autocommit(&self->link, 0);

#if (_YOD_SYSTEM_DEBUG && _YOD_DBCONN_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): %d in %s:%d %s",
		__FUNCTION__, self, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ int _yod_dbconn_commit(yod_dbconn_t *self __ENV_CPARM)
*/
int _yod_dbconn_commit(yod_dbconn_t *self __ENV_CPARM)
{
	int ret = 0;

	if (!self || !self->root) {
		return (-1);
	}
	ret = mysql_commit(&self->link);
	mysql_autocommit(&self->link, 1);

#if (_YOD_SYSTEM_DEBUG && _YOD_DBCONN_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): %d in %s:%d %s",
		__FUNCTION__, self, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ int _yod_dbconn_rollback(yod_dbconn_t *self __ENV_CPARM)
*/
int _yod_dbconn_rollback(yod_dbconn_t *self __ENV_CPARM)
{
	int ret = 0;

	if (!self || !self->root) {
		return (-1);
	}
	ret = mysql_rollback(&self->link);
	mysql_autocommit(&self->link, 1);

#if (_YOD_SYSTEM_DEBUG && _YOD_DBCONN_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): %d in %s:%d %s",
		__FUNCTION__, self, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ int yod_dbconn_execute(yod_dbconn_t *self, const char *sql, ulong len, ...)
*/
int yod_dbconn_execute(yod_dbconn_t *self, const char *sql, ulong len, ...)
{
	va_list args;
	int ret = 0;

	if (!self || !self->root) {
		return (-1);
	}

	va_start(args, len);
	ret = _yod_dbconn_stmt_query(self, sql, len, args, __ENV_TRACE3);
	va_end(args);

	if (_yod_dbconn_stmt_clean(self->stmt) != 0) {
		yod_stdlog_error(NULL, "%s, errno=%d in %s:%d %s",
			mysql_error(&self->link), mysql_errno(&self->link), __ENV_TRACE3);
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_DBCONN_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %s, %lu): %d in %s:%d %s",
		__FUNCTION__, self, sql, len, ret, __ENV_TRACE3);
#endif

	return ret;
}
/* }}} */


/** {{{ int yod_dbconn_insert(yod_dbconn_t *self, const char *table, const char *field, const char *value, ...)
*/
int yod_dbconn_insert(yod_dbconn_t *self, const char *table, const char *field, const char *value, ...)
{
	char sql[YOD_DBCONN_SQL_LEN];
	va_list args;
	int ret = -1;

	if (!self || !self->root) {
		return (-1);
	}

	if ((ret = snprintf(sql, YOD_DBCONN_SQL_LEN, "INSERT INTO %s (%s) VALUES (%s)", table, field, value)) == -1) {
		YOD_STDLOG_ERROR("snprintf failed");
		return (-1);
	}

	va_start(args, value);
	ret = _yod_dbconn_stmt_query(self, sql, (ulong) ret, args, __ENV_TRACE3);
	va_end(args);

	if (_yod_dbconn_stmt_clean(self->stmt) != 0) {
		yod_stdlog_error(NULL, "%s, errno=%d in %s:%d %s",
			mysql_error(&self->link), mysql_errno(&self->link), __ENV_TRACE3);
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_DBCONN_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %s, %s, %s, ...): %d in %s:%d %s",
		__FUNCTION__, self, table, field, value, ret, __ENV_TRACE3);
#endif

	return ret;
}
/* }}} */


/** {{{ yod_dbconn_s *yod_dbconn_select(yod_dbconn_t *self, const char *table, const char *field, const char *where, yod_dbconn_s **stmt, ...)
*/
yod_dbconn_s *yod_dbconn_select(yod_dbconn_t *self, const char *table, const char *field, const char *where, yod_dbconn_s **stmt, ...)
{
	yod_dbconn_s *ret = NULL;
	char sql[YOD_DBCONN_SQL_LEN];
	va_list args;
	int res = 0;

	if (!self || !self->root) {
		return NULL;
	}

	if (!where) {
		res = snprintf(sql, YOD_DBCONN_SQL_LEN, "SELECT %s FROM %s", field, table);
	}
	else {
		res = snprintf(sql, YOD_DBCONN_SQL_LEN, "SELECT %s FROM %s WHERE %s", field, table, where);
	}

	if (res == -1) {
		YOD_STDLOG_ERROR("snprintf failed");
		return NULL;
	}

	va_start(args, stmt);
	res = _yod_dbconn_stmt_query(self, sql, (ulong) res, args, __ENV_TRACE3);
	va_end(args);

	if (res == 0) {
		ret = self->stmt;
		if (stmt) {
			*stmt = self->stmt;
			self->stmt = NULL;
			ret->state = 1;
		}
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_DBCONN_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %s, %s, %s, ...): %p in %s:%d %s",
		__FUNCTION__, self, table, field, where, ret, __ENV_TRACE3);
#endif

	return ret;
}
/* }}} */


/** {{{ int yod_dbconn_update(yod_dbconn_t *self, const char *table, const char *setval, const char *where, ...)
*/
int yod_dbconn_update(yod_dbconn_t *self, const char *table, const char *setval, const char *where, ...)
{
	char sql[YOD_DBCONN_SQL_LEN];
	va_list args;
	int ret = -1;

	if (!self || !self->root) {
		return (-1);
	}

	if ((ret = snprintf(sql, YOD_DBCONN_SQL_LEN, "UPDATE %s SET %s WHERE %s", table, setval, where)) == -1) {
		YOD_STDLOG_ERROR("snprintf failed");
		return (-1);
	}

	va_start(args, where);
	ret = _yod_dbconn_stmt_query(self, sql, (ulong) ret, args, __ENV_TRACE3);
	va_end(args);

	if (_yod_dbconn_stmt_clean(self->stmt) != 0) {
		yod_stdlog_error(NULL, "%s, errno=%d in %s:%d %s",
			mysql_error(&self->link), mysql_errno(&self->link), __ENV_TRACE3);
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_DBCONN_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %s, %s, %s, ...): %d in %s:%d %s",
		__FUNCTION__, self, table, setval, where, ret, __ENV_TRACE3);
#endif

	return ret;
}
/* }}} */


/** {{{ int yod_dbconn_delete(yod_dbconn_t *self, const char *table, const char *where, ...)
*/
int yod_dbconn_delete(yod_dbconn_t *self, const char *table, const char *where, ...)
{
	char sql[YOD_DBCONN_SQL_LEN];
	va_list args;
	int ret = -1;

	if (!self || !self->root) {
		return (-1);
	}

	if ((ret = snprintf(sql, YOD_DBCONN_SQL_LEN, "DELETE FROM %s WHERE %s", table, where)) == -1) {
		YOD_STDLOG_ERROR("snprintf failed");
		return (-1);
	}

	va_start(args, where);
	ret = _yod_dbconn_stmt_query(self, sql, (ulong) ret, args, __ENV_TRACE3);
	va_end(args);

	if (_yod_dbconn_stmt_clean(self->stmt) != 0) {
		yod_stdlog_error(NULL, "%s, errno=%d in %s:%d %s",
			mysql_error(&self->link), mysql_errno(&self->link), __ENV_TRACE3);
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_DBCONN_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %s, %s, ...): %lu in %s:%d %s",
		__FUNCTION__, self, table, where, ret, __ENV_TRACE3);
#endif

	return ret;
}
/* }}} */


/** {{{ ulong yod_dbconn_count(yod_dbconn_t *self, const char *table, const char *where, ...)
*/
ulong yod_dbconn_count(yod_dbconn_t *self, const char *table, const char *where, ...)
{
	char sql[YOD_DBCONN_SQL_LEN];
	uint64_t count = 0;
	va_list args;
	int ret = 0;

	if (!self || !self->root) {
		return (0);
	}

	if (!where) {
		ret = snprintf(sql, YOD_DBCONN_SQL_LEN, "SELECT COUNT(*) FROM %s", table);
	}
	else {
		ret = snprintf(sql, YOD_DBCONN_SQL_LEN, "SELECT COUNT(*) FROM %s WHERE %s", table, where);
	}

	if (ret == -1) {
		YOD_STDLOG_ERROR("snprintf failed");
		return (0);
	}

	va_start(args, where);
	ret = _yod_dbconn_stmt_query(self, sql, (ulong) ret, args, __ENV_TRACE3);
	va_end(args);

	if (ret == 0) {
		_yod_dbconn_stmt_fetch(self->stmt, &count);
	}

	if (_yod_dbconn_stmt_clean(self->stmt) != 0) {
		yod_stdlog_error(NULL, "%s, errno=%d in %s:%d %s",
			mysql_error(&self->link), mysql_errno(&self->link), __ENV_TRACE3);
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_DBCONN_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %s, %s, ...): %lu in %s:%d %s",
		__FUNCTION__, self, table, where, (ulong) count, __ENV_TRACE3);
#endif

	return (ulong) count;
}
/* }}} */


/** {{{ int _yod_dbconn_fetch(yod_dbconn_s *stmt, ...)
*/
int _yod_dbconn_fetch(yod_dbconn_s *stmt, ...)
{
	yod_string_t *bind_value = NULL;
	yod_string_t *bind_data = NULL;
	MYSQL_BIND *bind_field = NULL;
	MYSQL_FIELD *bind_meta = NULL;
	MYSQL_STMT *bind_stmt = NULL;
	uint bind_count = 0;
	va_list args;
	uint i = 0;
	int ret = -1;

	if (!stmt || !stmt->stmt || !stmt->meta) {
		return (-1);
	}
	bind_stmt = stmt->stmt;
	bind_field = stmt->bind;
	bind_data = stmt->data;

	bind_count = mysql_num_fields(stmt->meta);
	if (bind_count == 0 || bind_count > stmt->count) {
		return (-1);
	}

	bind_meta = mysql_fetch_fields(stmt->meta);

	memset(bind_field, 0, bind_count * sizeof(MYSQL_BIND));

	va_start(args, stmt);
	for (i = 0; i < bind_count; ++i) {
		switch (bind_meta[i].type) {
			case FIELD_TYPE_TINY:
				bind_field[i].buffer_type = FIELD_TYPE_TINY;
				bind_field[i].buffer = va_arg(args, uint8_t *);
				bind_field[i].length = (ulong *) &(bind_data[i].len);
				bind_data[i].ptr = NULL;
				bind_data[i].len = 0;
				break;
				
			case FIELD_TYPE_SHORT:
				bind_field[i].buffer_type = FIELD_TYPE_SHORT;
				bind_field[i].buffer = va_arg(args, uint16_t *);
				bind_field[i].length = (ulong *) &(bind_data[i].len);
				bind_data[i].ptr = NULL;
				bind_data[i].len = 0;
				break;

			case FIELD_TYPE_INT24:
			case FIELD_TYPE_LONG:
				bind_field[i].buffer_type = bind_meta[i].type;
				bind_field[i].buffer = va_arg(args, uint32_t *);
				bind_field[i].length = (ulong *) &(bind_data[i].len);
				bind_data[i].ptr = NULL;
				bind_data[i].len = 0;
				break;

			case FIELD_TYPE_LONGLONG:
				bind_field[i].buffer_type = FIELD_TYPE_LONGLONG;
				bind_field[i].buffer = va_arg(args, uint64_t *);
				bind_field[i].length = (ulong *) &(bind_data[i].len);
				bind_data[i].ptr = NULL;
				bind_data[i].len = 0;
				break;

			case FIELD_TYPE_FLOAT:
				bind_field[i].buffer_type = FIELD_TYPE_FLOAT;
				bind_field[i].buffer = va_arg(args, float *);
				bind_field[i].length = (ulong *) &(bind_data[i].len);
				bind_data[i].ptr = NULL;
				bind_data[i].len = 0;
				break;

			case FIELD_TYPE_DOUBLE:
				bind_field[i].buffer_type = FIELD_TYPE_DOUBLE;
				bind_field[i].buffer = va_arg(args, double *);
				bind_field[i].length = (ulong *) &(bind_data[i].len);
				bind_data[i].ptr = NULL;
				bind_data[i].len = 0;
				break;

			case FIELD_TYPE_STRING:
				bind_field[i].buffer_type = FIELD_TYPE_STRING;
				bind_field[i].buffer = va_arg(args, char *);
				bind_field[i].buffer_length = (ulong ) va_arg(args, size_t);
				bind_field[i].length = (ulong *) &(bind_data[i].len);
				bind_data[i].ptr = NULL;
				bind_data[i].len = 0;
				break;

			case FIELD_TYPE_VAR_STRING:
			case FIELD_TYPE_TINY_BLOB:
			case FIELD_TYPE_BLOB:
			case FIELD_TYPE_MEDIUM_BLOB:
			case FIELD_TYPE_LONG_BLOB:
			case FIELD_TYPE_BIT:
				bind_field[i].buffer_type = bind_meta[i].type;
				bind_field[i].buffer = NULL;
				bind_field[i].length = (ulong *) &(bind_data[i].len);
				bind_data[i].ptr = (char *) va_arg(args, yod_string_t *);
				bind_data[i].len = 0;
				break;

			default :
				bind_field[i].buffer_type = FIELD_TYPE_STRING;
				bind_field[i].buffer = va_arg(args, char *);
				bind_field[i].buffer_length = bind_meta[i].length;
				bind_field[i].length = (ulong *) &(bind_data[i].len);
				bind_data[i].ptr = NULL;
				bind_data[i].len = 0;
				break;
		};
	}
	va_end(args);

	if ((ret = mysql_stmt_bind_result(bind_stmt, bind_field)) != 0) {
		yod_stdlog_error(NULL, "mysql_stmt_bind_result: %s, errno=%d in %s:%d %s",
			mysql_stmt_error(bind_stmt), mysql_stmt_errno(bind_stmt), __ENV_TRACE3);
		goto e_failed;
	}

	if ((ret = mysql_stmt_fetch(bind_stmt)) != 0) {
		if (ret != MYSQL_NO_DATA && ret != MYSQL_DATA_TRUNCATED) {
			yod_stdlog_error(NULL, "mysql_stmt_fetch: %s, errno=%d in %s:%d %s",
				mysql_stmt_error(bind_stmt), mysql_stmt_errno(bind_stmt), __ENV_TRACE3);
			goto e_failed;
		}
	}

	if (ret == MYSQL_DATA_TRUNCATED) {
		for (i = 0; i < bind_count; ++i) {
			if (bind_data[i].ptr != NULL) {
				bind_value = (yod_string_t *) bind_data[i].ptr;
				bind_field[i].buffer = bind_value->ptr;
				bind_field[i].length = NULL;
				if (bind_data[i].len > bind_value->len) {
					bind_field[i].buffer = (char *) realloc(bind_value->ptr, (bind_data[i].len + 1) * sizeof(char));
					if (!bind_field[i].buffer) {
						YOD_STDLOG_ERROR("realloc failed");
						goto e_failed;
					}
					bind_value->ptr = bind_field[i].buffer;
					bind_value->len = bind_data[i].len;
					bind_value->ptr[bind_value->len] = '\0';
				}
				bind_field[i].buffer_length = (ulong) bind_data[i].len;
				if ((ret = mysql_stmt_fetch_column(bind_stmt, &bind_field[i], i, 0)) != 0) {
					yod_stdlog_error(NULL, "mysql_stmt_fetch: %s, errno=%d in %s:%d %s",
						mysql_stmt_error(bind_stmt), mysql_stmt_errno(bind_stmt), __ENV_TRACE3);
					goto e_failed;
				}
			}
		}
	}

e_failed:

#if (_YOD_SYSTEM_DEBUG && _YOD_DBCONN_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): %d in %s:%d %s",
		__FUNCTION__, stmt, ret, __ENV_TRACE3);
#endif

	return ret;
}
/* }}} */


/** {{{ int _yod_dbconn_clean(yod_dbconn_s *stmt __ENV_CPARM)
*/
int _yod_dbconn_clean(yod_dbconn_s *stmt __ENV_CPARM)
{
	int ret = -1;

	if (!stmt) {
		return (-1);
	}

	if ((ret = _yod_dbconn_stmt_clean(stmt)) != 0) {
		yod_stdlog_error(NULL, "%s, errno=%d in %s:%d %s",
			mysql_error(stmt->link), mysql_errno(stmt->link), __ENV_TRACE);
	}

	if (stmt->state) {
		if (stmt->bind) {
			free(stmt->bind);
		}
		if (stmt->data) {
			free(stmt->data);
		}
		free(stmt);
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_DBCONN_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): %d in %s:%d %s",
		__FUNCTION__, stmt, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ static int _yod_dbconn_bind_dburl(char *dburl, char **host, uint *port, char **user, char **passwd, char **dbname)
*/
static int _yod_dbconn_bind_dburl(char *dburl, char **host, uint *port, char **user, char **passwd, char **dbname)
{
	char *ptr = NULL;
	size_t len = 0;

	if (!dburl || !host || !port || !user || !passwd || !dbname) {
		return (-1);
	}

	len = strlen(dburl);
	if (len < 13 || strncmp(dburl, "mysql://", 8)) {
		return (-1);
	}

	if ((ptr = strrchr(dburl, '/')) == NULL) {
		return (-1);
	}
	*dbname = ptr + 1;
	*ptr = '\0';

	if ((ptr = strrchr(dburl, '@')) == NULL) {
		return (-1);
	}
	*host = ptr + 1;
	*ptr = '\0';

	if ((ptr = strrchr(*host, ':')) != NULL) {
		*port = (uint) atoi(ptr + 1);
		*ptr = '\0';
	}

	*user = dburl + 8;
	if ((ptr = strchr(*user, ':')) == NULL) {
		return (-1);
	}
	*passwd = ptr + 1;
	*ptr = '\0';

	return (0);
}
/* }}} */


/** {{{ static int _yod_dbconn_stmt_query(yod_dbconn_t *self, const char *sql, ulong len, va_list args __ENV_CPARM)
*/
static int _yod_dbconn_stmt_query(yod_dbconn_t *self, const char *sql, ulong len, va_list args __ENV_CPARM)
{
	yod_dbconn_s *bind_entry = NULL;
	yod_string_t *bind_data = NULL;
	MYSQL_BIND *bind_param = NULL;
	MYSQL_BIND *bind_field = NULL;
	yod_string_t bind_text = {0};
	ulong bind_count = 0;
	ulong bind_length = 0;
	char *bind_value = 0;
	int bind_type = 0;
	int ret = -1;
	ulong i = 0;

#if (_YOD_SYSTEM_DEBUG && _YOD_DBCONN_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %s, %lu, %p) in %s:%d %s",
		__FUNCTION__, self, sql, len, args, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	if (!self || !self->root) {
		return (-1);
	}

/*
TINYINT					FIELD_TYPE_TINY			uint8_t
SMALLINT				FIELD_TYPE_SHORT		uint16_t
MEDIUMINT				FIELD_TYPE_INT24		uint32_t
INT						FIELD_TYPE_LONG			uint32_t
BIGINT					FIELD_TYPE_LONGLONG		uint64_t
FLOAT					FIELD_TYPE_FLOAT		float
DOUBLE					FIELD_TYPE_DOUBLE		double
DECIMAL					FIELD_TYPE_NEWDECIMAL	char[]
YEAR					FIELD_TYPE_SHORT		short int
TIME					FIELD_TYPE_TIME			MYSQL_TIME
DATE					FIELD_TYPE_DATE			MYSQL_TIME
DATETIME				FIELD_TYPE_DATETIME		MYSQL_TIME
TIMESTAMP				FIELD_TYPE_TIMESTAMP	MYSQL_TIME
CHAR, BINARY			FIELD_TYPE_STRING		char[]
VARCHAR, VARBINARY		FIELD_TYPE_VAR_STRING	char[]
TINYBLOB, TINYTEXT		FIELD_TYPE_TINY_BLOB	char[]
BLOB, TEXT				FIELD_TYPE_BLOB			char[]
MEDIUMBLOB, MEDIUMTEXT	FIELD_TYPE_MEDIUM_BLOB	char[]
LONGBLOB, LONGTEXT		FIELD_TYPE_LONG_BLOB	char[]
BIT						FIELD_TYPE_BIT			char[]
*/

	if ((bind_entry = self->stmt) == NULL) {
		if ((bind_entry = (yod_dbconn_s *) malloc(sizeof(yod_dbconn_s))) == NULL) {
			YOD_STDLOG_ERROR("malloc failed");
			return (-1);
		}
		memset(bind_entry, 0, sizeof(yod_dbconn_s));
		self->stmt = bind_entry;
	}

	/* MYSQL */
	bind_entry->link = &(self->link);

	/* MYSQL_STMT */
	if ((bind_entry->stmt = mysql_stmt_init(&self->link)) == NULL) {
		YOD_STDLOG_ERROR("mysql_stmt_init failed");
		return (-1);
	}

	if (mysql_stmt_prepare(bind_entry->stmt, sql, len) != 0) {
		yod_stdlog_error(NULL, "mysql_stmt_prepare: %s, errno=%d in %s:%d %s",
			mysql_stmt_error(bind_entry->stmt), mysql_stmt_errno(bind_entry->stmt), __ENV_TRACE);
		goto e_failed;
	}

	/* MYSQL_BIND */
	if ((bind_count = mysql_stmt_param_count(bind_entry->stmt)) > 0) {
		if (bind_count > self->count) {
			bind_param = (MYSQL_BIND *) realloc(self->bind, bind_count * sizeof(MYSQL_BIND));
			if (!bind_param) {
				YOD_STDLOG_ERROR("realloc failed");
				goto e_failed;
			}
			self->count = bind_count;
			self->bind = bind_param;
		}
		else {
			bind_param = self->bind;
		}

		memset(bind_param, 0, bind_count * sizeof(MYSQL_BIND));

		for (i = 0; i < bind_count; ++i) {
			bind_type = va_arg(args, int);
			switch (bind_type) {
				case FIELD_TYPE_NULL:
					bind_param[i].buffer_type = FIELD_TYPE_NULL;
					bind_param[i].is_null_value = 1;
					break;

				case FIELD_TYPE_TINY:
					if ((bind_value = (char *) malloc(sizeof(uint8_t))) == NULL) {
						YOD_STDLOG_ERROR("malloc failed");
						goto e_failed;
					}
					*((uint8_t *) bind_value) = (uint8_t) va_arg(args, int);
					bind_param[i].buffer_type = FIELD_TYPE_TINY;
					bind_param[i].buffer = bind_value;
					bind_param[i].is_unsigned = 1;
					break;

				case FIELD_TYPE_SHORT:
					if ((bind_value = (char *) malloc(sizeof(uint16_t))) == NULL) {
						YOD_STDLOG_ERROR("malloc failed");
						goto e_failed;
					}
					*((uint16_t *) bind_value) = (uint16_t) va_arg(args, int);
					bind_param[i].buffer_type = FIELD_TYPE_SHORT;
					bind_param[i].buffer = bind_value;
					bind_param[i].is_unsigned = 1;
					break;

				case FIELD_TYPE_INT24:
				case FIELD_TYPE_LONG:
					if ((bind_value = (char *) malloc(sizeof(uint32_t))) == NULL) {
						YOD_STDLOG_ERROR("malloc failed");
						goto e_failed;
					}
					*((uint32_t *) bind_value) = (uint32_t) va_arg(args, uint32_t);
					bind_param[i].buffer_type = FIELD_TYPE_LONG;
					bind_param[i].buffer = bind_value;
					bind_param[i].is_unsigned = 1;
					break;

				case FIELD_TYPE_LONGLONG:
					if ((bind_value = (char *) malloc(sizeof(uint64_t))) == NULL) {
						YOD_STDLOG_ERROR("malloc failed");
						goto e_failed;
					}
					*((uint64_t *) bind_value) = va_arg(args, uint64_t);
					bind_param[i].buffer_type = FIELD_TYPE_LONGLONG;
					bind_param[i].buffer = bind_value;
					bind_param[i].is_unsigned = 1;
					break;

				case FIELD_TYPE_FLOAT:
					if ((bind_value = (char *) malloc(sizeof(float))) == NULL) {
						YOD_STDLOG_ERROR("malloc failed");
						goto e_failed;
					}
					*((float *) bind_value) = (float) va_arg(args, double);
					bind_param[i].buffer_type = FIELD_TYPE_FLOAT;
					bind_param[i].buffer = bind_value;
					break;

				case FIELD_TYPE_DOUBLE:
					if ((bind_value = (char *) malloc(sizeof(double))) == NULL) {
						YOD_STDLOG_ERROR("malloc failed");
						goto e_failed;
					}
					*((double *) bind_value) = va_arg(args, double);
					bind_param[i].buffer_type = FIELD_TYPE_DOUBLE;
					bind_param[i].buffer = bind_value;
					break;

				case FIELD_TYPE_VAR_STRING:
				case FIELD_TYPE_TINY_BLOB:
				case FIELD_TYPE_BLOB:
				case FIELD_TYPE_MEDIUM_BLOB:
				case FIELD_TYPE_LONG_BLOB:
				case FIELD_TYPE_BIT:
					bind_param[i].buffer_type = bind_type;
					bind_text = va_arg(args, yod_string_t);
					if (bind_text.ptr != NULL) {
						bind_length = (ulong) bind_text.len;
						bind_param[i].buffer = (char *) malloc((bind_length + 1) * sizeof(char));
						if (!bind_param[i].buffer) {
							YOD_STDLOG_ERROR("malloc failed");
							goto e_failed;
						}
						memcpy(bind_param[i].buffer, bind_text.ptr, bind_length);
						((char *) bind_param[i].buffer)[bind_length] = '\0';
						bind_param[i].buffer_length = bind_length;
					}
					else {
						bind_param[i].is_null_value = 1;
					}
					break;

				default :
					bind_param[i].buffer_type = FIELD_TYPE_STRING;
					if ((bind_value = va_arg(args, char *)) != NULL) {
						bind_length = (ulong) strlen(bind_value);
						bind_param[i].buffer = (char *) malloc((bind_length + 1) * sizeof(char));
						if (!bind_param[i].buffer) {
							YOD_STDLOG_ERROR("malloc failed");
							goto e_failed;
						}
						memcpy(bind_param[i].buffer, bind_value, bind_length);
						((char *) bind_param[i].buffer)[bind_length] = '\0';
						bind_param[i].buffer_length = bind_length;
					} else {
						bind_param[i].is_null_value = 1;
					}
					break;
			};
		}

		if ((ret = mysql_stmt_bind_param(bind_entry->stmt, bind_param)) != 0) {
			yod_stdlog_error(NULL, "mysql_stmt_bind_param: %s, errno=%d in %s:%d %s",
				mysql_stmt_error(bind_entry->stmt), mysql_stmt_errno(bind_entry->stmt), __ENV_TRACE);
			goto e_failed;
		}

	}

	if ((ret = mysql_stmt_execute(bind_entry->stmt)) != 0) {
		yod_stdlog_error(NULL, "mysql_stmt_execute: %s, errno=%d in %s:%d %s",
			mysql_stmt_error(bind_entry->stmt), mysql_stmt_errno(bind_entry->stmt), __ENV_TRACE);
		goto e_failed;
	}

	self->lastid = (ulong) mysql_stmt_insert_id(bind_entry->stmt);
	self->rowsnum = (ulong) mysql_stmt_affected_rows(bind_entry->stmt);

	/* MYSQL_RES */
	if ((bind_entry->meta = mysql_stmt_result_metadata(bind_entry->stmt)) != NULL) {
		if ((bind_length = mysql_num_fields(bind_entry->meta)) > 0) {
			if (bind_length > bind_entry->count) {
				bind_field = (MYSQL_BIND *) realloc(bind_entry->bind, bind_length * sizeof(MYSQL_BIND));
				if (!bind_field) {
					YOD_STDLOG_ERROR("realloc failed");
					goto e_failed;
				}
				bind_entry->bind = bind_field;

				bind_data = (yod_string_t *) realloc(bind_entry->data, bind_length * sizeof(yod_string_t));
				if (!bind_data) {
					YOD_STDLOG_ERROR("realloc failed");
					goto e_failed;
				}
				bind_entry->data = bind_data;

				bind_entry->count = bind_length;

				memset(bind_field, 0, bind_length * sizeof(MYSQL_BIND));
			}

			if ((ret = mysql_stmt_store_result(bind_entry->stmt)) != 0) {
				yod_stdlog_error(NULL, "mysql_stmt_store_result: %s, errno=%d in %s:%d %s",
					mysql_stmt_error(bind_entry->stmt), mysql_stmt_errno(bind_entry->stmt), __ENV_TRACE3);
			}
		}
	}

e_failed:

	if (bind_param) {
		for (i = 0; i < bind_count; ++i) {
			if (bind_param[i].buffer) {
				free(bind_param[i].buffer);
			}
		}
	}

	return ret;
}
/* }}} */


/** {{{ static int _yod_dbconn_stmt_clean(yod_dbconn_s *stmt __ENV_CPARM)
*/
static int _yod_dbconn_stmt_clean(yod_dbconn_s *stmt)
{
	int ret = -1;

	if (!stmt) {
		return (-1);
	}

	if (stmt->meta) {
		mysql_free_result(stmt->meta);
		stmt->meta = NULL;
	}

	if (stmt->stmt) {
		ret = mysql_stmt_close(stmt->stmt);
		stmt->stmt = NULL;
	}

	return ret;
}
/* }}} */
