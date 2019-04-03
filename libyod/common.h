#ifndef __YOD_COMMON_H__
#define __YOD_COMMON_H__

#include <ctype.h>

/* debug */
#define _YOD_DBCONN_DEBUG 										0
#define _YOD_EVLOOP_DEBUG 										0
#define _YOD_HTABLE_DEBUG 										0
#define _YOD_JVALUE_DEBUG 										0
#define _YOD_RBTREE_DEBUG 										0
#define _YOD_SERVER_DEBUG 										0
#define _YOD_SHTTPD_DEBUG 										0
#define _YOD_SOCKET_DEBUG 										0
#define _YOD_STDLOG_DEBUG 										0
#define _YOD_SYSTEM_DEBUG 										1
#define _YOD_THREAD_DEBUG 										0


uint64_t yod_common_nowtime();
int yod_common_strtime(char buf[28]);

int yod_common_debug(FILE *fp, const char *fmt, ...);
int yod_common_error(FILE *fp, const char *fmt, ...);

uint32_t yod_common_ip2long(char *ipv4);
char *yod_common_long2ip(char buf[16], uint32_t ipv4);

char *yod_common_urlencode(char *buf, size_t max, char *url);
char *yod_common_urldecode(char *buf, size_t max, char *url);

char *yod_common_convpath(char *path, char *conv);

int yod_common_pidfile(const char *file);
int yod_common_mkdirp(const char *path, mode_t mode);

uint16_t yod_common_get_uint16(byte data[2]);
uint32_t yod_common_get_uint32(byte data[4]);
uint64_t yod_common_get_uint64(byte data[8]);
float yod_common_get_float(byte data[4]);
double yod_common_get_double(byte data[8]);
char *yod_common_get_text(byte *data, size_t *len);
byte *yod_common_get_blob(byte *data, size_t *len);
uint32_t yod_common_get_vint32(byte data[5], uint8_t *len);
uint64_t yod_common_get_vint64(byte data[9], uint8_t *len);
float yod_common_get_vfloat(byte data[5], uint8_t *len);
double yod_common_get_vdouble(byte data[9], uint8_t *len);

uint8_t yod_common_set_uint16(byte data[2], uint16_t num);
uint8_t yod_common_set_uint32(byte data[4], uint32_t num);
uint8_t yod_common_set_uint64(byte data[8], int64_t num);
uint8_t yod_common_set_float(byte data[4], float num);
uint8_t yod_common_set_double(byte data[8], double num);
size_t yod_common_set_text(byte *data, char *text, size_t len);
size_t yod_common_set_blob(byte *data, byte *blob, size_t len);
uint8_t yod_common_set_vint32(byte data[5], uint32_t num);
uint8_t yod_common_set_vint64(byte data[9], uint64_t num);
uint8_t yod_common_set_vfloat(byte data[5], float num);
uint8_t yod_common_set_vdouble(byte data[9], double num);

char *yod_common_blob2hex(char *buf, size_t max, byte *data, size_t len);
size_t yod_common_hex2blob(byte *buf, size_t max, char *data, size_t len);

char yod_common_byte2hex(byte num, int cap);
byte yod_common_hex2byte(char hex);

char *yod_common_strncpy(yod_string_t *dest, char *str, size_t len);
char *yod_common_strcpy(yod_string_t *dest, char *str);

byte yod_common_chksum(byte *data, size_t len);

#endif
