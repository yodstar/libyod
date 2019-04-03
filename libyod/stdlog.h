#ifndef __YOD_STDLOG_H__
#define __YOD_STDLOG_H__

#include "system.h"


/* stdlog level */
enum
{
	YOD_STDLOG_LEVEL_FATAL 	= 0x0080,
	YOD_STDLOG_LEVEL_ALERT 	= 0x0040,
	YOD_STDLOG_LEVEL_ERROR 	= 0x0020,
	YOD_STDLOG_LEVEL_WARN 	= 0x0010,
	YOD_STDLOG_LEVEL_NOTICE = 0x0008,
	YOD_STDLOG_LEVEL_INFO 	= 0x0004,
	YOD_STDLOG_LEVEL_DEBUG 	= 0x0002,
	YOD_STDLOG_LEVEL_TRACE 	= 0x0001
};


/* stdlog state */
enum
{
	YOD_STDLOG_STATE_INIT,
	YOD_STDLOG_STATE_WRITE,
	YOD_STDLOG_STATE_FREE
};


/* yod_stdlog_t */
typedef struct _yod_stdlog_t 									yod_stdlog_t;

/* yod_stdlog_fn */
typedef int (*yod_stdlog_fn) (short what, short level, char *buf, void *arg __ENV_CPARM);


#define YOD_STDLOG_LINE_1 										"----------------------------------------------------------------"
#define YOD_STDLOG_LINE_2 										"================================================================"


#define __LOG_LINE_1 											YOD_STDLOG_LINE_1
#define __LOG_LINE_2 											YOD_STDLOG_LINE_2

#define __LOG_FATAL 											YOD_STDLOG_LEVEL_FATAL
#define __LOG_ALERT 											YOD_STDLOG_LEVEL_ALERT
#define __LOG_ERROR 											YOD_STDLOG_LEVEL_ERROR
#define __LOG_WARN 												YOD_STDLOG_LEVEL_WARN
#define __LOG_NOTICE 											YOD_STDLOG_LEVEL_NOTICE
#define __LOG_INFO 												YOD_STDLOG_LEVEL_INFO
#define __LOG_DEBUG 											YOD_STDLOG_LEVEL_DEBUG
#define __LOG_TRACE 											YOD_STDLOG_LEVEL_TRACE


#define __LOG_INIT 												YOD_STDLOG_STATE_INIT
#define __LOG_WRITE 											YOD_STDLOG_STATE_WRITE
#define __LOG_FREE 												YOD_STDLOG_STATE_FREE


#define YOD_STDLOG_HEX(s, b, l) 								yod_stdlog_hex(NULL, __LOG_TRACE, s, (byte *) b, l)
#define YOD_STDLOG_FATAL(s) 									yod_stdlog_fatal(NULL, "%s, errno=%d in %s:%d %s", s, __ENV_ERRNO, __ENV_TRACE3)
#define YOD_STDLOG_ALERT(s) 									yod_stdlog_alert(NULL, "%s, errno=%d in %s:%d %s", s, __ENV_ERRNO, __ENV_TRACE3)
#define YOD_STDLOG_ERROR(s) 									yod_stdlog_error(NULL, "%s, errno=%d in %s:%d %s", s, __ENV_ERRNO, __ENV_TRACE3)
#define YOD_STDLOG_WARN(s) 										yod_stdlog_warn(NULL, "%s, errno=%d in %s:%d %s", s, __ENV_ERRNO, __ENV_TRACE3)
#define YOD_STDLOG_NOTICE(s) 									yod_stdlog_notice(NULL, "%s, errno=%d in %s:%d %s", s, __ENV_ERRNO, __ENV_TRACE3)
#define YOD_STDLOG_INFO(s) 										yod_stdlog_info(NULL, "%s in %s:%d %s", s, __ENV_TRACE3)
#define YOD_STDLOG_DEBUG(s) 									yod_stdlog_debug(NULL, "%s in %s:%d %s", s, __ENV_TRACE3)
#define YOD_STDLOG_TRACE(s) 									yod_stdlog_trace(NULL, "%s in %s:%d %s", s, __ENV_TRACE3)
#define YOD_STDLOG_DUMP(s) 										yod_stdlog_dump(NULL, "%s\n%s\n%s\n", __LOG_LINE_1, s, __LOG_LINE_1)

#define yod_stdlog_new(s, l, f, a) 								_yod_stdlog_new(s, l, f, a __ENV_CARGS)
#define yod_stdlog_free(x) 										_yod_stdlog_free(x __ENV_CARGS)

#define yod_stdlog_level(x, l) 									_yod_stdlog_level(x, l __ENV_CARGS)
#define yod_stdlog_format(x, l, f)								_yod_stdlog_format(x, l, f __ENV_CARGS)

#define yod_stdlog_init() 										_yod_stdlog_init(__ENV_ARGS)
#define yod_stdlog_destroy() 									_yod_stdlog_destroy(__ENV_ARGS)


yod_stdlog_t *_yod_stdlog_new(const char *name, short level, yod_stdlog_fn func, void *arg __ENV_CPARM);
void _yod_stdlog_free(yod_stdlog_t *self __ENV_CPARM);

int _yod_stdlog_level(yod_stdlog_t *self, short level __ENV_CPARM);
int _yod_stdlog_format(yod_stdlog_t *self, short level, char *format __ENV_CPARM);

int _yod_stdlog_init(__ENV_PARM);
void _yod_stdlog_destroy(__ENV_PARM);

int yod_stdlog_write(yod_stdlog_t *self, short level, const char *fmt, ...);
int yod_stdlog_hex(yod_stdlog_t *self, short level, const char *str, byte *blob, size_t len);

int yod_stdlog_fatal(yod_stdlog_t *self, const char *fmt, ...);
int yod_stdlog_alert(yod_stdlog_t *self, const char *fmt, ...);
int yod_stdlog_error(yod_stdlog_t *self, const char *fmt, ...);
int yod_stdlog_warn(yod_stdlog_t *self, const char *fmt, ...);
int yod_stdlog_notice(yod_stdlog_t *self, const char *fmt, ...);
int yod_stdlog_info(yod_stdlog_t *self, const char *fmt, ...);
int yod_stdlog_debug(yod_stdlog_t *self, const char *fmt, ...);
int yod_stdlog_trace(yod_stdlog_t *self, const char *fmt, ...);

int yod_stdlog_dump(yod_stdlog_t *self, const char *fmt, ...);

#endif
