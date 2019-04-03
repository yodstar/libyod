#ifndef __YOD_DBCONN_H__
#define __YOD_DBCONN_H__

#ifdef _WIN32
	#include <mysql.h>
#else
	#include <mysql/mysql.h>
#endif


/* yod_dbconn_t */
typedef struct _yod_dbconn_t 									yod_dbconn_t;

/* yod_dbconn_s */
typedef struct _yod_dbconn_s 									yod_dbconn_s;


#define yod_dbconn_new(s) 										_yod_dbconn_new(s __ENV_CARGS)
#define yod_dbconn_free(x) 										_yod_dbconn_free(x __ENV_CARGS)

#define yod_dbconn_open(x) 										_yod_dbconn_open(x __ENV_CARGS)
#define yod_dbconn_close(x) 									_yod_dbconn_close(x __ENV_CARGS)

#define yod_dbconn_lastid(x) 									_yod_dbconn_lastid(x __ENV_CARGS)
#define yod_dbconn_rowsnum(x) 									_yod_dbconn_rowsnum(x __ENV_CARGS)

#define yod_dbconn_begin(x) 									_yod_dbconn_begin(x __ENV_CARGS)
#define yod_dbconn_commit(x) 									_yod_dbconn_commit(x __ENV_CARGS)
#define yod_dbconn_rollback(x) 									_yod_dbconn_rollback(x __ENV_CARGS)

#define yod_dbconn_fetch 										_yod_dbconn_fetch
#define yod_dbconn_clean(x) 									_yod_dbconn_clean(x __ENV_CARGS)


yod_dbconn_t *_yod_dbconn_new(const char *dburl __ENV_CPARM);
void _yod_dbconn_free(yod_dbconn_t *self __ENV_CPARM);

yod_dbconn_t *_yod_dbconn_open(yod_dbconn_t *self __ENV_CPARM);
int _yod_dbconn_close(yod_dbconn_t *self __ENV_CPARM);

ulong _yod_dbconn_lastid(yod_dbconn_t *self __ENV_CPARM);
ulong _yod_dbconn_rowsnum(yod_dbconn_t *self __ENV_CPARM);
ulong yod_dbconn_escape(yod_dbconn_t *self, char *out, const char *str, size_t len);

int _yod_dbconn_begin(yod_dbconn_t *self __ENV_CPARM);
int _yod_dbconn_commit(yod_dbconn_t *self __ENV_CPARM);
int _yod_dbconn_rollback(yod_dbconn_t *self __ENV_CPARM);

int yod_dbconn_execute(yod_dbconn_t *self, const char *sql, ulong len, ...);
int yod_dbconn_insert(yod_dbconn_t *self, const char *table, const char *field, const char *value, ...);
yod_dbconn_s *yod_dbconn_select(yod_dbconn_t *self, const char *table, const char *field, const char *where, yod_dbconn_s **stmt, ...);
int yod_dbconn_update(yod_dbconn_t *self, const char *table, const char *setval, const char *where, ...);
int yod_dbconn_delete(yod_dbconn_t *self, const char *table, const char *where, ...);
ulong yod_dbconn_count(yod_dbconn_t *self, const char *table, const char *where, ...);

int _yod_dbconn_fetch(yod_dbconn_s *stmt, ...);
int _yod_dbconn_clean(yod_dbconn_s *stmt __ENV_CPARM);

#endif
