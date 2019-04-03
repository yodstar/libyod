#ifndef __YOD_SYSTEM_H__
#define __YOD_SYSTEM_H__

#include <stddef.h>
#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	/* inttypes */
	typedef __int8 												int8_t;
	typedef __int16 											int16_t;
	typedef __int32 											int32_t;
	typedef	__int64 											int64_t;
	typedef unsigned __int8 									uint8_t;
	typedef unsigned __int16 									uint16_t;
	typedef unsigned __int32 									uint32_t;
	typedef unsigned __int64 									uint64_t;
	typedef unsigned long 										ulong;
	typedef unsigned int 										uint;
	typedef unsigned short 										ushort;
	typedef unsigned char 										uchar;
	typedef unsigned char 										byte;

	typedef int 												mode_t;

	/* pthread */
	typedef unsigned int										pthread_t;
	typedef int 												pthread_mutexattr_t;
	typedef HANDLE 												pthread_mutex_t;
	#define pthread_mutex_init(x, a) 							((*x = CreateMutex(NULL, FALSE, NULL)) == 0)
	#define pthread_mutex_lock(x) 								WaitForSingleObject(*x, INFINITE)
	#define pthread_mutex_unlock(x) 							ReleaseMutex(*x)  
	#define pthread_mutex_destroy(x) 							CloseHandle(*x)
	#define pthread_self() 										GetCurrentThreadId()
	/* poll */
	#define poll												WSAPoll
	/* string */
	#define strdup 												_strdup
	#define strcasecmp 											_stricmp
	#define strncasecmp 										_strnicmp
	#define strndup 											yod_system_strndup_s
	#define strtok_r 											strtok_s
	#define sscanf 												sscanf_s
	#define snprintf 											sprintf_s
	#define strcpy(d, s) 										strcpy_s(d, sizeof(d), s)
	/* io */
	#define getcwd 												_getcwd
	#define chdir 												_chdir
	#define close 												_close
	#define access 												_access
	#define mkdir(p, m) 										_mkdir(p)
	#define realpath(p, r) 										_fullpath(r, p, _MAX_PATH)
	#define stat 												_stat
	#define isatty 												_isatty
	/* const */
	#define STDIN_FILENO 										_fileno(stdin)
#else
	#include <inttypes.h>
	#include <pthread.h>
	typedef int 												SOCKET;
	typedef unsigned long int 									ulong;
	typedef unsigned int 										uint;
	typedef unsigned short 										ushort;
	typedef unsigned char 										uchar;
	typedef unsigned char 										byte;
	#define _MAX_PATH 											260
#endif


/* yod_string_t */
typedef struct {char *ptr; size_t len;} 						yod_string_t;


#ifndef INVALID_SOCKET
#define INVALID_SOCKET 											(SOCKET)(~0)
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR 											(-1)
#endif


#include "common.h"


/* system */
#ifndef _YOD_SYSTEM_IGNORE
#define _YOD_SYSTEM_IGNORE 										0xFF
#endif


#if _YOD_SYSTEM_DEBUG
#define YOD_SYSTEM_ENV_PARM 									const char *__file, int __line, const char *__func
#define YOD_SYSTEM_ENV_ARGS 									__FILE__, __LINE__, __FUNCTION__
#define YOD_SYSTEM_ENV_TRACE 									__file, __line, __func
#define YOD_SYSTEM_ENV_VOID 									(void) __file, (void) __line, (void) __func;
#define YOD_SYSTEM_ENV_CPARM 									, YOD_SYSTEM_ENV_PARM
#define YOD_SYSTEM_ENV_CARGS 									, YOD_SYSTEM_ENV_ARGS
#define YOD_SYSTEM_ENV_ASSERT(x) 								_yod_system_assert(x __ENV_CARGS)
#define YOD_SYSTEM_ENV_VFREE 									void (*) (void *, const char *, int, const char *)
#else
#define YOD_SYSTEM_ENV_PARM
#define YOD_SYSTEM_ENV_ARGS
#define YOD_SYSTEM_ENV_TRACE 									NULL, 0, NULL
#define YOD_SYSTEM_ENV_VOID
#define YOD_SYSTEM_ENV_CPARM
#define YOD_SYSTEM_ENV_CARGS
#define YOD_SYSTEM_ENV_ASSERT(x)
#define YOD_SYSTEM_ENV_VFREE									void (*) (void *)
#endif

/* errno */
#ifdef _WIN32
#define YOD_SYSTEM_ENV_ERRNO 									GetLastError()
#else
#define YOD_SYSTEM_ENV_ERRNO 									errno
#endif

#define __ENV_PARM 												YOD_SYSTEM_ENV_PARM
#define __ENV_ARGS 												YOD_SYSTEM_ENV_ARGS
#define __ENV_TRACE 											YOD_SYSTEM_ENV_TRACE
#define __ENV_VOID 												YOD_SYSTEM_ENV_VOID
#define __ENV_CPARM 											YOD_SYSTEM_ENV_CPARM
#define __ENV_CARGS 											YOD_SYSTEM_ENV_CARGS
#define __ENV_ASSERT(x) 										YOD_SYSTEM_ENV_ASSERT(x)
#define __ENV_VFREE 											YOD_SYSTEM_ENV_VFREE


#define __ENV_TRACE2 											__FILE__, __LINE__
#define __ENV_TRACE3 											__FILE__, __LINE__, __FUNCTION__

#define __ENV_ERRNO 											YOD_SYSTEM_ENV_ERRNO


#if (_YOD_SYSTEM_DEBUG)
#if (_YOD_SYSTEM_IGNORE & 0xFF)
#define malloc(z) 												_yod_system_malloc(z __ENV_CARGS)
#define calloc(n, z) 											_yod_system_calloc(n, z __ENV_CARGS)
#define realloc(x, z) 											_yod_system_realloc(x, z __ENV_CARGS)
#define free(x) 												_yod_system_free(x __ENV_CARGS)

#undef strdup
#define strdup(s) 												_yod_system_strndup(s, strlen(s) __ENV_CARGS)
#undef strndup
#define strndup(s, l) 											_yod_system_strndup(s, l __ENV_CARGS)

void *_yod_system_malloc(size_t size __ENV_CPARM);
void *_yod_system_calloc(size_t num, size_t size __ENV_CPARM);
void *_yod_system_realloc(void *ptr, size_t size __ENV_CPARM);

char *_yod_system_strndup(const char *str, size_t len __ENV_CPARM);
#endif

int _yod_system_assert(void *ptr __ENV_CPARM);
#endif


ulong yod_system_memory(short what);
#ifdef _WIN32
char *yod_system_strndup_s(const char *str, size_t len);
#endif
void _yod_system_free(void *ptr __ENV_CPARM);

#endif
