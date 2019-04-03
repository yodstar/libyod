#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#ifdef WIN32
	#include <time.h>
	#include <winsock2.h>
	#include <direct.h>
	#include <io.h>
#else
	#include <sys/time.h>
	#include <unistd.h>
#endif
#include <errno.h>

#include "system.h"
#include "common.h"


/** {{{ uint64_t yod_common_nowtime()
*/
uint64_t yod_common_nowtime()
{
	uint64_t ret = 0;

#ifdef _WIN32
	static uint64_t msec = 0;
	SYSTEMTIME wtm;
	LARGE_INTEGER liCounter, liCurrent;

	if (!QueryPerformanceFrequency(&liCounter)) {
		ret = GetTickCount();
	} else {
		QueryPerformanceCounter(&liCurrent);
		ret = (uint64_t) (liCurrent.QuadPart * 1000 / liCounter.QuadPart);
	}
	if (msec == 0) {
		GetLocalTime(&wtm);
		msec = time(0) * 1000L + wtm.wMilliseconds - ret;
	}
	ret += msec;
#else
	struct timeval tval;
	gettimeofday(&tval, NULL);
	ret = tval.tv_sec * 1000L + tval.tv_usec / 1000L;
#endif

	return ret;
}
/* }}} */


/** {{{ int yod_common_strtime(char buf[28])
*/
int yod_common_strtime(char buf[28])
{
	struct timeval tval;
	struct tm dtime;
	int ret = -1;

	if (!buf) {
		return (-1);
	}

#ifdef _WIN32
	{
		SYSTEMTIME wtm;
		GetLocalTime(&wtm);
		dtime.tm_year 	= wtm.wYear - 1900;
		dtime.tm_mon 	= wtm.wMonth - 1;
		dtime.tm_mday 	= wtm.wDay;
		dtime.tm_hour 	= wtm.wHour;
		dtime.tm_min 	= wtm.wMinute;
		dtime.tm_sec 	= wtm.wSecond;
		dtime.tm_isdst 	= -1;
		tval.tv_sec 	= (long) mktime(&dtime);
		tval.tv_usec 	= wtm.wMilliseconds * 1000;
	}
#else
	gettimeofday(&tval, NULL);
	localtime_r(&tval.tv_sec, &dtime);
#endif

	ret = snprintf(buf, 27, "%04d-%02d-%02d %02d:%02d:%02d %06ld",
		1900 + dtime.tm_year, 1 + dtime.tm_mon, dtime.tm_mday,
		dtime.tm_hour, dtime.tm_min, dtime.tm_sec, tval.tv_usec);

	if (ret == -1) {
		memset(buf, '0', 27);
	} else {
		buf[ret] = '\0';
	}

	return ret;
}
/* }}} */


/** {{{ int yod_common_debug(FILE *fp, const char *fmt, ...)
*/
int yod_common_debug(FILE *fp, const char *fmt, ...)
{
	char buffer[1024] = {0};
	char stime[28] = {0};
	va_list args;
	int ret = -1;

	va_start(args, fmt);
#ifdef _WIN32
	ret = vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt, args);
#else
	ret = vsnprintf(buffer, sizeof(buffer), fmt, args);
#endif
	va_end(args);

	if (ret != -1) {
		fp = fp ? fp : stdout;
		yod_common_strtime(stime);
		ret = fprintf(fp, "[%s] %09lX - DEBUG %s\n", stime, (ulong) pthread_self(), buffer);
		fflush(fp);
	}

	return ret;
}
/* }}} */


/** {{{ int yod_common_debug(FILE *fp, const char *fmt, ...)
*/
int yod_common_error(FILE *fp, const char *fmt, ...)
{
	char buffer[1024] = {0};
	char stime[28] = {0};
	va_list args;
	int ret = -1;

	va_start(args, fmt);
#ifdef _WIN32
	ret = vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt, args);
#else
	ret = vsnprintf(buffer, sizeof(buffer), fmt, args);
#endif
	va_end(args);

	if (ret != -1) {
		fp = fp ? fp : stderr;
		yod_common_strtime(stime);
		ret = fprintf(fp, "[%s] %09lX - ERROR %s\n", stime, (ulong) pthread_self(), buffer);
		fflush(fp);
	}

	return ret;
}
/* }}} */


/** {{{ uint32_t yod_common_ip2long(char *ipv4)
*/
uint32_t yod_common_ip2long(char *ipv4)
{
	uint32_t a, b, c, d;
	uint32_t ret = 0;

	a = b = c = d = 0;
	sscanf(ipv4, "%hhu.%hhu.%hhu.%hhu", (uint8_t *) &a, (uint8_t *) &b, (uint8_t *) &c, (uint8_t *) &d);
	ret = (a << 24) | (b << 16) | (c << 8) | d;

	return ret;
}
/* }}} */


/** {{{ char *yod_common_long2ip(char buf[16], uint32_t ipv4)
*/
char *yod_common_long2ip(char buf[16], uint32_t ipv4)
{
	uint8_t tmp[4] = {0};
	int ret = 0;

	for (ret = 0; ret < 4; ++ ret) { 
		tmp[ret] = (uint8_t) (ipv4 & 0xFF);
		ipv4 = ipv4 >> 8;
	}
	ret = snprintf(buf, 16, "%hhu.%hhu.%hhu.%hhu", tmp[3], tmp[2], tmp[1], tmp[0]);

	return (ret != -1) ? buf : NULL;
}
/* }}} */


/** {{{ char *yod_common_urlencode(char *buf, size_t max, char *url)
*/
char *yod_common_urlencode(char *buf, size_t max, char *url)
{
	size_t i, j, l;
	byte tmp[4];

	if (!buf || !url) {
		return NULL;
	}

	j = 0;
	l = strlen(url);
	for (i = 0; i < l; ++i) {
		if (j + 3 < max) {
			break;
		}
		if (isalnum((byte) url[i])) {
			buf[j++] = url[i];
		}
		else {
			tmp[0] = '%';
			tmp[1] = yod_common_byte2hex(url[i] >> 4, 1);
			tmp[2] = yod_common_byte2hex(url[i] % 16, 1);
			memcpy(buf + j, tmp, 3);
			j += 3;
		}
	}
	buf[j++] = '\0';

	return buf;
}
/* }}} */


/** {{{ char *yod_common_urldecode(char *buf, size_t max, char *url)
*/
char *yod_common_urldecode(char *buf, size_t max, char *url)
{
	size_t i, j, l;
	byte chr = 0;

	if (!buf || !url) {
		return NULL;
	}

	j = 0;
	l = strlen(url);
	for (i = 0; i < l; ++i) {
		if (j >= max) {
			break;
		}
		if (url[i]=='%') {
			chr = (yod_common_hex2byte(url[i + 1]) << 4);
			chr |= yod_common_hex2byte(url[i + 2]);
			i += 2;
		}
		else if (url[i] == '+') {
			chr = ' ';
		}
		else {
			chr = url[i];
		}
		buf[j++] = chr;
	}
	buf[j++] = '\0';

	return buf;
}
/* }}} */


/** {{{ char *yod_common_convpath(char *path, char *conv)
*/
char *yod_common_convpath(char *path, char *conv)
{
	short flag = 0;
	char *p, *v;

	if (!path) {
		return NULL;
	}

	if (!conv) {
		v = (char *) malloc((strlen(path) + 1) * sizeof(char));
	} else {
		v = conv;
	}

	p = path;
	while (*p != '\0') {
		if (flag == 0) {
			if (*p == '/' || *p == '\\') {
				flag = 1;
			}
			*v++ = *p;
		}
		else if (flag == 1) {
			if (*p != '.' && *p != '/' && *p != '\\') {
				*v++ = *p;
				flag = 0;
			}
		}
		++ p;
	}
	if (flag == 1) {
		-- v;
	}
	*v = '\0';

	return conv;
}
/* }}} */


/** {{{ int yod_common_pidfile(const char *file)
*/
int yod_common_pidfile(const char *file)
{
	FILE *fp = NULL;
	char buf[12];

	if (!file) {
		return (-1);
	}

#ifdef _WIN32
	if (fopen_s(&fp, file, "w") != 0)
#else
	if ((fp = fopen(file, "w")) == NULL)
#endif
	{
		return (-1);
	}

#ifdef _WIN32
	snprintf(buf, sizeof(buf), "%ld", (uint64_t) GetCurrentProcess());
#else
	snprintf(buf, sizeof(buf), "%d", (uint32_t) getpid());
#endif
	fwrite(buf, strlen(buf), 1, fp);
	fclose(fp);

	return (0);
}
/* }}} */


/** {{{ int yod_common_mkdirp(const char *path, mode_t mode)
*/
int yod_common_mkdirp(const char *path, mode_t mode)
{  
	char dir[_MAX_PATH];
	size_t len = 0;
	size_t i = 0;

	if (!path) {
		return (-1);
	}

#ifdef _WIN32
	strcpy_s(dir, _MAX_PATH, path);
#else
	strcpy(dir, path);
#endif
	len = strlen(dir);
	for (i = 1; i<len; ++i) {
		if (dir[i] == '/') {
			dir[i] = 0;
			if (access(dir, 0) != 0) {
				if (mkdir(dir, mode) == -1) {
					return (-1);
				}
			}
			dir[i] = '/';
		}
	}

	return (0);

	(void) mode;
}
/* }}} */


/** {{{ uint16_t yod_common_get_uint16(byte data[2])
*/
uint16_t yod_common_get_uint16(byte data[2])
{
	uint16_t ret = 0;
	uint16_t n = 1;
	uint8_t i = 0;

	for (; i < 2; ++i) {
		ret += (data[i] & 0xFF) * n;
		n *= 256;
	}

	return ret;
}
/* }}} */


/** {{{ uint32_t yod_common_get_uint32(byte data[4])
*/
uint32_t yod_common_get_uint32(byte data[4])
{
	uint32_t ret = 0;
	uint32_t n = 1;
	uint8_t i = 0;

	for (; i < 4; ++i) {
		ret += (data[i] & 0xFF) * n;
		n *= 256;
	}

	return ret;
}
/* }}} */


/** {{{ uint64_t yod_common_get_uint64(byte data[8])
*/
uint64_t yod_common_get_uint64(byte data[8])
{
	uint64_t ret = 0;
	uint64_t n = 1;
	uint8_t i = 0;

	for (; i < 8; ++i) {
		ret += (data[i] & 0xFF) * n;
		n *= 256;
	}

	return ret;
}
/* }}} */


/** {{{ float yod_common_get_float(byte data[4])
*/
float yod_common_get_float(byte data[4])
{
	float ret = 0;
	uint32_t num = 0;

	num = yod_common_get_uint32(data);
	memcpy(&ret, &num, 4);

	return ret;
}
/* }}} */


/** {{{ double yod_common_get_double(byte data[8])
*/
double yod_common_get_double(byte data[8])
{
	double ret = 0;
	uint64_t num = 0;

	num = yod_common_get_uint64(data);
	memcpy(&ret, &num, 8);

	return ret;
}
/* }}} */


/** {{{ char *yod_common_get_text(byte *data, size_t *len)
*/
char *yod_common_get_text(byte *data, size_t *len)
{
	uint64_t num = 0;
	uint8_t pos = 0;

	num = yod_common_get_vint32(data, &pos);
	if (len) {
		*len = (size_t) num;
	}

	return (char *) (data + pos);
}
/* }}} */


/** {{{ byte *yod_common_get_blob(byte *data, size_t *len)
*/
byte *yod_common_get_blob(byte *data, size_t *len)
{
	uint64_t num = 0;
	uint8_t pos = 0;

	num = yod_common_get_vint32(data, &pos);
	if (len) {
		*len = (size_t) num;
	}

	return (byte *) (data + pos);
}
/* }}} */


/** {{{ uint32_t yod_common_get_vint32(byte data[5], uint8_t *len)
*/
uint32_t yod_common_get_vint32(byte data[5], uint8_t *len)
{
	uint32_t ret = 0;
	uint32_t n = 1;
	uint8_t i = 0;

	while (i < 5) {
		ret += (data[i] & 0x7F) * n;
		if ((data[i++] & 0x80) == 0) {
			break;
		}
		n *= 128;
	}
	if (i > 4 && (data[4] & 0x80) != 0) {
		ret += 0x80000000;
	}

	if (len) {
		*len = i;
	}

	return ret;
}
/* }}} */


/** {{{ uint64_t yod_common_get_vint64(byte data[9], uint8_t *len)
*/
uint64_t yod_common_get_vint64(byte data[9], uint8_t *len)
{
	uint64_t ret = 0;
	uint64_t n = 1;
	uint8_t i = 0;

	while (i < 9) {
		ret += (data[i] & 0x7F) * n;
		if ((data[i++] & 0x80) == 0) {
			break;
		}
		n *= 128;
	}
	if (i > 8 && (data[8] & 0x80) != 0) {
		ret += 0x8000000000000000;
	}

	if (len) {
		*len = i;
	}

	return ret;
}
/* }}} */


/** {{{ float yod_common_get_vfloat(byte data[4], uint8_t *len)
*/
float yod_common_get_vfloat(byte data[5], uint8_t *len)
{
	float ret = 0;
	uint32_t num = 0;

	num = yod_common_get_vint32(data, len);
	memcpy(&ret, &num, 4);

	return ret;
}
/* }}} */


/** {{{ double yod_common_get_vdouble(byte data[9], uint8_t *len)
*/
double yod_common_get_vdouble(byte data[9], uint8_t *len)
{
	double ret = 0;
	uint64_t num = 0;

	num = yod_common_get_vint64(data, len);
	memcpy(&ret, &num, 8);

	return ret;
}
/* }}} */


/** {{{ uint8_t yod_common_set_uint16(byte data[2], uint16_t num)
*/
uint8_t yod_common_set_uint16(byte data[2], uint16_t num)
{
	uint8_t i = 0;

	for (; i < 2; ++i) {
		data[i] = num & 0xFF;
		num /= 256;
	}

	return (2);
}
/* }}} */


/** {{{ uint8_t yod_common_set_uint32(byte data[4], uint32_t num)
*/
uint8_t yod_common_set_uint32(byte data[4], uint32_t num)
{
	uint8_t i = 0;

	for (; i < 4; ++i) {
		data[i] = num & 0xFF;
		num /= 256;
	}

	return (4);
}
/* }}} */


/** {{{ uint8_t yod_common_set_uint64(byte data[8], int64_t num)
*/
uint8_t yod_common_set_uint64(byte data[8], int64_t num)
{
	uint8_t i = 0;

	for (; i < 8; ++i) {
		data[i] = num & 0xFF;
		num /= 256;
	}

	return (8);
}
/* }}} */


/** {{{ uint8_t yod_common_set_float(byte data[4], float num)
*/
uint8_t yod_common_set_float(byte data[4], float num)
{
	uint32_t u32 = 0;

	memcpy(&u32, &num, 4);
	yod_common_set_uint32(data, u32);

	return (4);
}
/* }}} */


/** {{{ uint8_t yod_common_set_double(byte data[8], double dval)
*/
uint8_t yod_common_set_double(byte data[8], double num)
{
	uint64_t u64 = 0;

	memcpy(&u64, &num, 8);
	yod_common_set_uint64(data, u64);

	return (8);
}
/* }}} */


/** {{{ size_t yod_common_set_text(byte *data, char *text, size_t len)
*/
size_t yod_common_set_text(byte *data, char *text, size_t len)
{
	uint8_t num = 0;

	num = yod_common_set_vint32(data, (uint32_t) len);
	if (text) {
		memcpy(data + num, text, len);
	} else {
		memset(data + num, 0, len);
	}
	data[num + len] = '\0';

	return (size_t) (len + num + 1);
}
/* }}} */


/** {{{ size_t yod_common_set_blob(byte *data, byte *blob, size_t len)
*/
size_t yod_common_set_blob(byte *data, byte *blob, size_t len)
{
	uint8_t num = 0;

	num = yod_common_set_vint32(data, (uint32_t) len);
	if (blob) {
		memcpy(data + num, blob, len);
	} else {
		memset(data + num, 0, len);
	}

	return (size_t) (len + num);
}
/* }}} */


/** {{{ uint8_t yod_common_set_vint32(byte data[5], uint32_t num)
*/
uint8_t yod_common_set_vint32(byte data[5], uint32_t num)
{
	uint8_t ret = 0;

	while (num > 128) {
		data[ret] = (byte) ((num % 128) | 0x80);
		num /= 128;
		if (ret++ > 3) {
			break;
		}
	}
	data[ret++] = (byte) num;

	return ret;
}
/* }}} */


/** {{{ uint8_t yod_common_set_vint64(byte data[9], uint64_t num)
*/
uint8_t yod_common_set_vint64(byte data[9], uint64_t num)
{
	uint8_t ret = 0;

	while (num > 128) {
		data[ret] = (byte) ((num % 128) | 0x80);
		num /= 128;
		if (ret++ > 7) {
			break;
		}
	}
	data[ret++] = (byte) num;

	return ret;
}
/* }}} */


/** {{{ uint8_t yod_common_set_vfloat(byte data[5], float num)
*/
uint8_t yod_common_set_vfloat(byte data[5], float num)
{
	uint8_t ret = 0;
	uint32_t u32 = 0;

	memcpy(&u32, &num, 4);
	ret = yod_common_set_vint32(data, u32);

	return ret;
}
/* }}} */


/** {{{ uint8_t yod_common_set_vdouble(byte data[9], double dval)
*/
uint8_t yod_common_set_vdouble(byte data[9], double num)
{
	uint8_t ret = 0;
	uint64_t u64 = 0;

	memcpy(&u64, &num, 8);
	ret = yod_common_set_vint64(data, u64);

	return ret;
}
/* }}} */


/** {{{ char *yod_common_blob2hex(char *buf, size_t max, byte *data, size_t len)
*/
char *yod_common_blob2hex(char *buf, size_t max, byte *data, size_t len)
{
	size_t i = 0;

	if (!buf || !data) {
		return NULL;
	}

	while (i < len) {
		if (i * 2 + 2 > max) {
			break;
		}
		buf[i * 2] = yod_common_byte2hex(data[i] >> 4, 1);
		buf[i * 2 + 1] = yod_common_byte2hex(data[i] & 0x0F, 1);
		buf[i * 2 + 2] = '\0';
		++ i;
	}

	return buf;
}
/* }}} */


/** {{{ size_t yod_common_hex2blob(byte *buf, size_t max, char *data, size_t len)
*/
size_t yod_common_hex2blob(byte *buf, size_t max, char *data, size_t len)
{
	size_t ret = 0;

	if (!buf || !data) {
		return (0);
	}

	while (ret < max) {
		if (ret * 2 + 1 > len) {
			break;
		}
		buf[ret] = (yod_common_hex2byte(data[ret * 2]) << 4);
		buf[ret] |= yod_common_hex2byte(data[ret * 2 + 1]);
		++ ret;
	}

	return ret;
}
/* }}} */


/** {{{ char yod_common_byte2hex(byte num, int cap)
*/
char yod_common_byte2hex(byte num, int cap)
{
	if (num > 9) {
		return cap ? num + 55 : num + 87;
	}
	return (num + 48);
}
/* }}} */


/** {{{ byte yod_common_hex2byte(char hex)
*/
byte yod_common_hex2byte(char hex)
{
	if (isdigit(hex)) {
		return hex - '0';
	}

	switch (hex) {
		case 'a': case 'A': return 0x0A;
		case 'b': case 'B': return 0x0B;
		case 'c': case 'C': return 0x0C;
		case 'd': case 'D': return 0x0D;
		case 'e': case 'E': return 0x0E;
		case 'f': case 'F': return 0x0F;
		default: return 0xFF;
	}
}
/* }}} */


/** {{{ char *yod_common_strncpy(yod_string_t *dest, char *str, size_t len)
*/
char *yod_common_strncpy(yod_string_t *dest, char *str, size_t len)
{
	char *ptr = NULL;

	if (!dest || !str) {
		return NULL;
	}

	if (dest->len < len) {
		if ((ptr = realloc(dest->ptr, len + 1)) == NULL) {
			return NULL;
		}
		dest->ptr = ptr;
	}
	dest->len = len;
	memcpy(dest->ptr, str, len);
	dest->ptr[len] = '\0';

	return dest->ptr;
}
/* }}} */


/** {{{ char *yod_common_strcpy(yod_string_t *dest, char *str)
*/
char *yod_common_strcpy(yod_string_t *dest, char *str)
{
	char *ptr = NULL;
	size_t len = 0;

	if (!dest || !str) {
		return NULL;
	}

	len = strlen(str);
	if (!dest->ptr || dest->len < len) {
		if ((ptr = realloc(dest->ptr, len + 1)) == NULL) {
			return NULL;
		}
		dest->ptr = ptr;
	}
	dest->len = len;
	memcpy(dest->ptr, str, len);
	dest->ptr[len] = '\0';

	return dest->ptr;
}
/* }}} */


/** {{{ byte yod_common_chksum(byte *data, size_t len)
*/
byte yod_common_chksum(byte *data, size_t len)
{
	short ret = 0;
	size_t i = 0;

	for (i = 0; i < len; i++) {
		ret += data[i];
		ret += (ret >> 8) & 0xFF;
		ret &= 0x0FF;   
	}
	ret ^= 0xFF;
	ret &= 0x0FF;

	return (byte) ret;
}
/* }}} */
