#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <math.h>
#include <errno.h>

#include "stdlog.h"
#include "jvalue.h"


#ifndef _YOD_JVALUE_DEBUG
#define _YOD_JVALUE_DEBUG 										0
#endif

#define YOD_JVALUE_MAX_SIZE 									0xFFFFFFF7


/* yod_jvalue_v */
typedef struct
{
	char *name;
	size_t name_len;

	yod_jvalue_t *value;
} yod_jvalue_v;


/* yod_jvalue_t */
struct _yod_jvalue_t
{
	yod_jvalue_t *parent;
	int type;

	union
	{
		int bval;
		int64_t ival;
		double dval;

		struct
		{
			size_t len;
			char *ptr;
		} string;

		struct
		{
			size_t count;
			yod_jvalue_t **data;
			size_t size;
		} array;

		struct
		{
			size_t count;
			union {
				yod_jvalue_v *ptr;
				size_t num;
			} data;
			size_t size;
		} object;
	} u;

	struct
	{
		union {
			void *ptr;
			char *str;
		} u;
		size_t index;
	} _reserved;

};


enum
{
	YOD_JVALUE_FLAG_NEXT = 0x0001,
	YOD_JVALUE_FLAG_REPROC = 0x0002,
	YOD_JVALUE_FLAG_STRING = 0x0004,
	YOD_JVALUE_FLAG_ESCAPED = 0x0008,
	YOD_JVALUE_FLAG_DONE = 0x0010,
	YOD_JVALUE_FLAG_NEED_COMMA = 0x0020,
	YOD_JVALUE_FLAG_NEED_COLON = 0x0040,
	YOD_JVALUE_FLAG_NEED_QUOTE = 0x0080,
	YOD_JVALUE_FLAG_SEEK_VALUE = 0x0100,
	YOD_JVALUE_FLAG_NUM_NEGATIVE = 0x0200,
	YOD_JVALUE_FLAG_NUM_ZERO = 0x0400,
	YOD_JVALUE_FLAG_NUM_E = 0x0800,
	YOD_JVALUE_FLAG_NUM_E_GOT_SIGN = 0x1000,
	YOD_JVALUE_FLAG_NUM_E_NEGATIVE = 0x2000,
	YOD_JVALUE_FLAG_LINE_COMMENT = 0x4000,
	YOD_JVALUE_FLAG_BLOCK_COMMENT = 0x8000,
};


static int _yod_jvalue_decode_new(int first_pass, yod_jvalue_t **top, yod_jvalue_t** root, yod_jvalue_t **alloc, int type);
static size_t _yod_jvalue_encode_string(char *str, size_t len, char *data);
static size_t _yod_jvalue_encode_strlen(char *str, size_t len);
static int _yod_jvalue_object_find(yod_jvalue_t *self, char *name, size_t *index, int force);


#define __JVF_NEXT 												YOD_JVALUE_FLAG_NEXT
#define __JVF_REPROC 											YOD_JVALUE_FLAG_REPROC
#define __JVF_STRING 											YOD_JVALUE_FLAG_STRING
#define __JVF_ESCAPED 											YOD_JVALUE_FLAG_ESCAPED
#define __JVF_DONE												YOD_JVALUE_FLAG_DONE
#define __JVF_NEED_COMMA 										YOD_JVALUE_FLAG_NEED_COMMA
#define __JVF_NEED_COLON 										YOD_JVALUE_FLAG_NEED_COLON
#define __JVF_NEED_QUOTE 										YOD_JVALUE_FLAG_NEED_QUOTE
#define __JVF_SEEK_VALUE 										YOD_JVALUE_FLAG_SEEK_VALUE
#define __JVF_NUM_NEGATIVE										YOD_JVALUE_FLAG_NUM_NEGATIVE
#define __JVF_NUM_ZERO											YOD_JVALUE_FLAG_NUM_ZERO
#define __JVF_NUM_E												YOD_JVALUE_FLAG_NUM_E
#define __JVF_NUM_E_GOT_SIGN									YOD_JVALUE_FLAG_NUM_E_GOT_SIGN
#define __JVF_NUM_E_NEGATIVE									YOD_JVALUE_FLAG_NUM_E_NEGATIVE
#define __JVF_LINE_COMMENT										YOD_JVALUE_FLAG_LINE_COMMENT
#define __JVF_BLOCK_COMMENT										YOD_JVALUE_FLAG_BLOCK_COMMENT


/** {{{ yod_jvalue_t *_yod_jvalue_new(__ENV_PARM)
*/
yod_jvalue_t *_yod_jvalue_new(__ENV_PARM)
{
	yod_jvalue_t *self = NULL;

	self = (yod_jvalue_t *) malloc(sizeof(yod_jvalue_t));
	if (!self) {
		YOD_STDLOG_ERROR("malloc failed");
		return NULL;
	}

	self->type = __JVT_NULL;
	self->parent = NULL;

	self->_reserved.u.ptr = NULL;
	self->_reserved.index = 0;

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(): %p in %s:%d %s",
		__FUNCTION__, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return self;
}
/* }}} */


/** {{{ void _yod_jvalue_free(yod_jvalue_t *self __ENV_CPARM)
*/
void _yod_jvalue_free(yod_jvalue_t *self __ENV_CPARM)
{
	size_t i = 0;

	if (!self) {
		return;
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p) in %s:%d %s",
		__FUNCTION__, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	switch (self->type) {
		case __JVT_OBJECT:
			if (self->u.object.data.ptr) {
				for (i = 0; i < self->u.object.count; ++i) {
					yod_jvalue_free(self->u.object.data.ptr[i].value);
				}
				free(self->u.object.data.ptr);
			}
			break;
		case __JVT_ARRAY:
			if (self->u.array.data) {
				for (i = 0; i < self->u.array.count; ++i) {
					yod_jvalue_free(self->u.array.data[i]);
				}
				free(self->u.array.data);
			}
			break;
		case __JVT_STRING:
			if (self->u.string.ptr) {
				free(self->u.string.ptr);
			}
			break;
	}

	free(self);
}
/* }}} */


/** {{{ int _yod_jvalue_type(yod_jvalue_t *self __ENV_CPARM)
*/
int _yod_jvalue_type(yod_jvalue_t *self __ENV_CPARM)
{
	int ret = 0;

	if (!self) {
		return (-1);
	}

	ret = self->type;

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): %d in %s:%d %s",
		__FUNCTION__, self, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ yod_jvalue_t *_yod_jvalue_load(const char *file, char *err __ENV_CPARM)
*/
yod_jvalue_t *_yod_jvalue_load(const char *file, char *err __ENV_CPARM)
{
	yod_jvalue_t *self = NULL;
	char cwd[256], path[256];
	FILE *fp = NULL;
	char *data = NULL;
	long len = 0;

	if (!file) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return NULL;
	}

	if (*file == '/' || *(file + 1) == ':') {
		strcpy(path, file);
	} else {
		if (getcwd(cwd, sizeof(cwd)) == NULL) {
			YOD_STDLOG_ERROR("getcwd failed");
			return NULL;
		}

		if (snprintf(path, sizeof(path), "%s/%s", cwd, file) == -1) {
			YOD_STDLOG_ERROR("snprintf failed");
			return NULL;
		}
	}

#ifdef _WIN32
	if (fopen_s(&fp, path, "rb") != 0)
#else
	if ((fp = fopen(path, "rb")) == NULL)
#endif
	{
		YOD_STDLOG_ERROR("fopen failed");
		return NULL;
	}

	fseek(fp, 0L, SEEK_END);
	len = ftell(fp);

	data = (char *) malloc((len + 1) * sizeof(char));
	if (!data) {
		YOD_STDLOG_ERROR("malloc failed");
		goto e_failed;
	}

	fseek(fp, 0L, SEEK_SET);
	if (len != (long) fread(data, sizeof(char), len, fp)) {
		YOD_STDLOG_ERROR("fread failed");
		goto e_failed;
	}

	self = yod_jvalue_decode(data, len, err);

e_failed:

	if (data) {
		free(data);
	}
	fclose(fp);

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%s): %p in %s:%d %s",
		__FUNCTION__, file, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return self;
}
/* }}} */


/** {{{ yod_jvalue_t *_yod_jvalue_clone(yod_jvalue_t *self __ENV_CPARM)
*/
yod_jvalue_t *_yod_jvalue_clone(yod_jvalue_t *self __ENV_CPARM)
{
	yod_jvalue_t *value = self;
	yod_jvalue_v vitem = {0};
	yod_jvalue_t *item = NULL;
	yod_jvalue_t *root = NULL;
	yod_jvalue_t *parent = NULL;
	yod_jvalue_t *clone = NULL;
	size_t data_len = 0;
	size_t name_len = 0;
	int alloc = 0;

	if (!self) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return (0);
	}

	while (value) {
		alloc = 0;
		if (value->type == __JVT_OBJECT || value->type == __JVT_ARRAY) {
			alloc = (int) value->_reserved.index;
		}
		if (alloc == 0) {
			if ((clone = (yod_jvalue_t *) malloc(sizeof(yod_jvalue_t))) != NULL) {
				clone->type = __JVT_NULL;
				clone->parent = parent;

				clone->_reserved.u.ptr = NULL;
				clone->_reserved.index = 0;
			}
			if (!root) {
				root = clone;
			}
		}

		if (clone != NULL) {
			switch (value->type) {
				case __JVT_OBJECT:
					if (value->_reserved.index == 0) {
						clone->type = __JVT_OBJECT;
						clone->u.object.count = 0;
						clone->u.object.size = value->u.object.size;
						data_len = (value->u.object.size + 1) * sizeof(yod_jvalue_v);
						name_len = value->u.object.data.ptr[value->u.object.size].name_len;
						clone->u.object.data.ptr = malloc(data_len + name_len);
						if (clone->u.object.data.ptr) {
							clone->u.object.data.ptr[clone->u.object.size].name = (char *) clone->u.object.data.ptr + data_len;
							clone->u.object.data.ptr[clone->u.object.size].name_len = name_len;
							clone->u.object.data.ptr[clone->u.object.size].value = NULL;
						}
						if (value->u.object.count == 0) {
							break;
						}
					}

					if (value->_reserved.index == value->u.object.count) {
						value->_reserved.index = 0;
						break;
					}

					vitem = value->u.object.data.ptr[value->_reserved.index ++];

					if (clone->u.object.data.ptr) {
						clone->u.object.data.ptr[clone->u.object.count].name = clone->u.object.data.ptr[clone->u.object.size].name;
						memcpy(clone->u.object.data.ptr[clone->u.object.count].name, vitem.name, vitem.name_len + 1);
						clone->u.object.data.ptr[clone->u.object.count].name_len = vitem.name_len;
						clone->u.object.data.ptr[clone->u.object.count].value = NULL;

						clone->u.object.data.ptr[clone->u.object.size].name += vitem.name_len + 1;
					}

					if (vitem.value) {
						value = vitem.value;
						parent = clone;
					}
					continue;

				case __JVT_ARRAY:
					if (value->_reserved.index == 0) {
						clone->type = __JVT_ARRAY;
						clone->u.array.count = 0;
						clone->u.array.size = value->u.array.size;
						clone->u.array.data = calloc(value->u.array.size, sizeof(yod_jvalue_t *));
						if (value->u.array.count == 0) {
							break;
						}
					}

					if (value->_reserved.index == value->u.array.count) {
						value->_reserved.index = 0;
						break;
					}

					item = value->u.array.data[value->_reserved.index ++];

					if (clone->u.array.data) {
						clone->u.array.data[self->u.array.count] = NULL;
					}

					if (item) {
						value = item;
						parent = clone;
					}
					continue;

				case __JVT_STRING:
					clone->type = __JVT_STRING;
					clone->u.string.ptr = (char *) malloc((value->u.string.len + 1)* sizeof(char));
					if (clone->u.string.ptr) {
						clone->u.string.len = value->u.string.len;
						memcpy(clone->u.string.ptr, value->u.string.ptr, clone->u.string.len);
						clone->u.string.ptr[clone->u.string.len] = '\0';
					}
					break;

				case __JVT_INTEGER:
					clone->type = __JVT_INTEGER;
					clone->u.ival = value->u.ival;
					break;

				case __JVT_DOUBLE:
					clone->type = __JVT_DOUBLE;
					clone->u.dval = value->u.dval;
					break;

				case __JVT_BOOLEAN:
					clone->type = __JVT_BOOLEAN;
					clone->u.bval = value->u.bval;
					break;

				case __JVT_NULL:
					break;

				default:
					break;
			};
		}

		if (parent != NULL) {
			if (parent->type == __JVT_OBJECT) {
				if (parent->u.object.data.ptr) {
					parent->u.object.data.ptr[parent->u.object.count ++].value = clone;
				}
			}
			else if (parent->type == __JVT_ARRAY) {
				if (parent->u.array.data) {
					parent->u.array.data[parent->u.array.count ++] = clone;
				}
			}
		}

		value = value->parent;
		if ((clone = parent) != NULL) {
			parent = clone->parent;
		}
		else {
			break;
		}
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): %p in %s:%d %s",
		__FUNCTION__, self, root, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return root;
}
/* }}} */


/** {{{ char *_yod_jvalue_dump(yod_jvalue_t *self __ENV_CPARM)
*/
char *_yod_jvalue_dump(yod_jvalue_t *self __ENV_CPARM)
{
	size_t len = 0;
	char *ret = NULL;

	if (!self) {
		return NULL;
	}

	len = yod_jvalue_encode(NULL, self);
	if ((ret = (char *) malloc(len * sizeof(char))) != NULL) {
		yod_jvalue_encode(ret, self);
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): %p in %s:%d %s",
		__FUNCTION__, self, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ size_t _yod_jvalue_encode(char *data, yod_jvalue_t *self __ENV_CPARM)
*/
size_t _yod_jvalue_encode(char *data, yod_jvalue_t *self __ENV_CPARM)
{
	yod_jvalue_t *value = self;
	yod_jvalue_v vitem = {0};
	yod_jvalue_t *item = NULL;
	int64_t num1, num2;
	char tmp[20], *ptr, *dot;
	size_t ret = 0;

	if (!self) {
		errno = EINVAL;
		YOD_STDLOG_WARN("invalid argument");
		return (0);
	}

	while (value) {
		switch (value->type) {
			case __JVT_OBJECT:
				if (value->_reserved.index == 0) {
					if (data) {
						data[ret] = '{';
					}
					++ ret;
					if (value->u.object.count == 0) {
						if (data) {
							data[ret] = '}';
						}
						++ ret;
						break;
					}
				}

				if (value->_reserved.index == value->u.object.count) {
					if (data) {
						data[ret] = '}';
					}
					++ ret;
					value->_reserved.index = 0;
					break;
				}

				if (value->_reserved.index > 0) {
					if (data) {
						data[ret] = ',';
					}
					++ ret;
				}

				vitem = value->u.object.data.ptr[value->_reserved.index ++];

				if (data) {
					data[ret ++] = '\"';
					ret += _yod_jvalue_encode_string(vitem.name, vitem.name_len, data + ret);
					data[ret ++] = '\"';
					data[ret ++] = ':';
					if (!vitem.value) {
						memcpy(data + ret, "null", 4);
						ret += 4;
					}
				}
				else {
					ret += _yod_jvalue_encode_strlen(vitem.name, vitem.name_len) + 3;
					if (!vitem.value) {
						ret += 4;
					}
				}

				if (vitem.value) {
					value = vitem.value;
				}
				continue;

			case __JVT_ARRAY:
				if (value->_reserved.index == 0) {
					if (data) {
						data[ret] = '[';
					}
					++ ret;
					if (value->u.array.count == 0) {
						if (data) {
							data[ret] = ']';
						}
						++ ret;
						break;
					}
				}

				if (value->_reserved.index == value->u.array.count) {
					if (data) {
						data[ret] = ']';
					}
					++ ret;
					value->_reserved.index = 0;
					break;
				}

				if (value->_reserved.index > 0) {
					if (data) {
						data[ret] = ',';
					}
					++ ret;
				}

				item = value->u.array.data[value->_reserved.index ++];

				if (!item) {
					if (data) {
						memcpy(data + ret, "null", 4);
					}
					ret += 4;
				}
				else {
					value = item;
				}
				continue;

			case __JVT_STRING:
				if (data) {
					data[ret ++] = '\"';
					ret += _yod_jvalue_encode_string(value->u.string.ptr, value->u.string.len, data + ret);
					data[ret ++] = '\"';
				}
				else {
					ret += _yod_jvalue_encode_strlen(value->u.string.ptr, value->u.string.len) + 2;
				}
				break;

			case __JVT_INTEGER:
				num1 = value->u.ival;

				if (num1 < 0) {
					if (data) {
						data[ret] = '-';
					}
					++ ret;
					num1 = - num1;
				}
				num2 = num1;

				++ ret;
				while (num1 >= 10) {
					++ ret;
					num1 /= 10;
				}

				if (data) {
					num1 = num2;
					ptr = data + ret;
					do {
						*-- ptr = "0123456789"[num1 % 10];
					} while ((num1 /= 10) > 0);
				}

				break;

			case __JVT_DOUBLE:
				if (data) {
					ptr = data + ret;
#ifdef _WIN32
					ret += sprintf_s(ptr, 20, "%lg", value->u.dval);
#else
					ret += sprintf(ptr, "%lg", value->u.dval);
#endif
					if ((dot = strchr(ptr, ',')) != NULL) {
						*dot = '.';
					}
					else if (!strchr (ptr, '.')) {
						data[ret ++] = '.';
						data[ret ++] = '0';
					}
				}
				else {
#ifdef _WIN32
					ret += sprintf_s(tmp, 20, "%lg", value->u.dval);
#else
					ret += sprintf(tmp, "%lg", value->u.dval);
#endif
					if (!strchr(tmp, ',') && !strchr (tmp, '.')) {
						ret += 2;
					}
				}
				break;

			case __JVT_BOOLEAN:
				if (data) {
					ptr = data + ret;
					if (value->u.bval) {
						memcpy(ptr, "true", 4);
						ret += 4;
					}
					else {
						memcpy(ptr, "false", 5);
						ret += 5;
					}
				}
				else {
					ret += value->u.bval ? 4 : 5;
				}
				break;

			case __JVT_NULL:
				if (data) {
					memcpy(data + ret, "null", 4);
				}
				ret += 4;
				break;

			default:
				break;
		};

		value = value->parent;
	}

	if (data) {
		data[ret] = '\0';
	}
	++ ret;

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %p): %d in %s:%d %s",
		__FUNCTION__, data, self, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ yod_jvalue_t *_yod_jvalue_decode(char *data, size_t len, char *err __ENV_CPARM)
*/
yod_jvalue_t *_yod_jvalue_decode(char *data, size_t len, char *err __ENV_CPARM)
{
	yod_jvalue_t *top, *root, *alloc = NULL;
	yod_jvalue_t *parent = NULL;
	long num_digits = 0, num_e = 0;
	int64_t num_fraction = 0;
	uint32_t curr_line, curr_col;
	const char *jptr, *jend, *jnum;
	int first_pass;
	long flags;

	if (!data || !len) {
		return NULL;
	}

	/* Skip UTF-8 BOM */
	if (len >= 3 && ((byte) data[0]) == 0xEF && ((byte) data[1]) == 0xBB && ((byte) data[2]) == 0xBF) {
		data += 3;
		len -= 3;
	}

	jend = (data + len);
	jnum = NULL;

	for (first_pass = 1; first_pass >= 0; -- first_pass) {
		byte uc_b1, uc_b2, uc_b3, uc_b4;
		uint32_t uc1, uc2;
		char *str = NULL;
		size_t str_len = 0;

		top = root = NULL;
		flags = __JVF_SEEK_VALUE;

		curr_line = 1;
		curr_col = 0;

		for (jptr = data; ; ++ jptr) {
			char b = (jptr == jend ? 0 : *jptr);
			++ curr_col;

			/* string */
			if (flags & __JVF_STRING) {
				if (!b) {
					if (err) {
						snprintf(err, 255, "Unexpected EOF in string (at %d:%d) in %s:%d %s",
							curr_line, curr_col, __ENV_TRACE3);
					}
					goto e_failed;
				}

				if (str_len > YOD_JVALUE_MAX_SIZE) {
					if (err) {
						snprintf(err, 255, "Too long (caught overflow) (at %d:%d) in %s:%d %s",
							curr_line, curr_col, __ENV_TRACE3);
					}
					goto e_failed;
				}

				if (flags & __JVF_ESCAPED) {
					flags &= ~ __JVF_ESCAPED;

					switch (b) {
						case 'b':
							if (!first_pass) {
								str[str_len] = '\b';
							}
							++ str_len;
							break;

						case 'f':
							if (!first_pass) {
								str[str_len] = '\f';
							}
							++ str_len;
							break;

						case 'n':
							if (!first_pass) {
								str[str_len] = '\n';
							}
							++ str_len;
							break;

						case 'r':
							if (!first_pass) {
								str[str_len] = '\r';
							}
							++ str_len;
							break;

						case 't':
							if (!first_pass) {
								str[str_len] = '\t';
							}
							++ str_len;
							break;

						case 'u':

							if (jend - jptr < 4 || 
								(uc_b1 = yod_common_hex2byte(*++ jptr)) == 0xFF ||
								(uc_b2 = yod_common_hex2byte(*++ jptr)) == 0xFF ||
								(uc_b3 = yod_common_hex2byte(*++ jptr)) == 0xFF ||
								(uc_b4 = yod_common_hex2byte(*++ jptr)) == 0xFF)
							{
								if (err) {
									snprintf(err, 255, "Invalid character value `%c` (at %d:%d) in %s:%d %s",
										b, curr_line, curr_col, __ENV_TRACE3);
								}
								goto e_failed;
							}

							uc_b1 = (uc_b1 << 4) | uc_b2;
							uc_b2 = (uc_b3 << 4) | uc_b4;
							uc1 = (uc_b1 << 8) | uc_b2;

							if ((uc1 & 0xF800) == 0xD800) {
								if (jend - jptr < 6 || (*++ jptr) != '\\' || (*++ jptr) != 'u' ||
									(uc_b1 = yod_common_hex2byte(*++ jptr)) == 0xFF ||
									(uc_b2 = yod_common_hex2byte(*++ jptr)) == 0xFF ||
									(uc_b3 = yod_common_hex2byte(*++ jptr)) == 0xFF ||
									(uc_b4 = yod_common_hex2byte(*++ jptr)) == 0xFF)
								{
									if (err) {
										snprintf(err, 255, "Invalid character value `%c` (at %d:%d) in %s:%d %s",
											b, curr_line, curr_col, __ENV_TRACE3);
									}
									goto e_failed;
								}

								uc_b1 = (uc_b1 << 4) | uc_b2;
								uc_b2 = (uc_b3 << 4) | uc_b4;
								uc2 = (uc_b1 << 8) | uc_b2;

								uc1 = 0x010000 | ((uc1 & 0x3FF) << 10) | (uc2 & 0x3FF);
							}

							if (sizeof(char) >= sizeof(uint32_t) || (uc1 <= 0x7F)) {
								if (!first_pass) {
									str[str_len] = (char) uc1;
								}
								++ str_len;
								break;
							}

							if (uc1 <= 0x7FF) {
								if (!first_pass) {
									str[str_len++] = (char) (0xC0 | (uc1 >> 6));
									str[str_len++] = (char) (0x80 | (uc1 & 0x3F));
								}
								else {
									str_len += 2;
								}
								break;
							}

							if (uc1 <= 0xFFFF) {
								if (!first_pass) {
									str[str_len++] = (char) (0xE0 | (uc1 >> 12));
									str[str_len++] = (char) (0x80 | ((uc1 >> 6) & 0x3F));
									str[str_len++] = (char) (0x80 | (uc1 & 0x3F));
								}
								else {
									str_len += 3;
								}
								break;
							}

							if (!first_pass) {
								str[str_len++] = (char) (0xF0 | (uc1 >> 18));
								str[str_len++] = (char) (0x80 | ((uc1 >> 12) & 0x3F));
								str[str_len++] = (char) (0x80 | ((uc1 >> 6) & 0x3F));
								str[str_len++] = (char) (0x80 | (uc1 & 0x3F));
							}
							else {
								str_len += 4;
							}

							break;

						default:
							if (!first_pass) {
								str[str_len] = b;
							}
							++ str_len;
					};

					continue;
				}

				if (b == '\\') {
					flags |= __JVF_ESCAPED;
					continue;
				}

				if (flags & __JVF_NEED_QUOTE) {
					if (b == '"') {
						if (err) {
							snprintf(err, 255, "Unexpected `%c` in comment opening sequence (at %d:%d) in %s:%d %s",
								(b ? b : '\1'), curr_line, curr_col, __ENV_TRACE3);
						}
						goto e_failed;
					}

					if (b == ' ' || b == '\t' || b == '\r' || b == '\n' || b == ',' || b == ']' || b == '}') {
						flags |= __JVF_REPROC;
					}
					else if (top->type == __JVT_OBJECT && b == ':') {
						flags |= __JVF_REPROC;
					}
				}

				if ((b == '"') || (flags & __JVF_REPROC)) {
					if (!first_pass) {
						str[str_len] = '\0';
					}

					flags &= ~ __JVF_STRING;
					str = NULL;

					if (flags & __JVF_REPROC) {
						flags &= ~ (__JVF_NEED_QUOTE | __JVF_REPROC);

						-- curr_col;
						-- jptr;
					}

					switch (top->type) {
						case __JVT_OBJECT:
							if (!first_pass) {
								_yod_jvalue_object_find(top, top->_reserved.u.str, &top->_reserved.index, 1);
								top->u.object.data.ptr[top->_reserved.index].name = top->_reserved.u.str;
								top->u.object.data.ptr[top->_reserved.index].name_len = str_len;
								top->_reserved.u.str += str_len + 1;
							}
							else {
								top->u.object.data.num += str_len + 1;
							}

							flags |= __JVF_SEEK_VALUE | __JVF_NEED_COLON;
							continue;

						case __JVT_STRING:
							top->u.string.len = str_len;
							flags |= __JVF_NEXT;
							break;

						default:
							break;
					};
				}
				else {
					if (!first_pass) {
						str[str_len] = b;
					}
					++ str_len;
					continue;
				}
			}

			/* comment */
			if (flags & (__JVF_LINE_COMMENT | __JVF_BLOCK_COMMENT)) {
				if (flags & __JVF_LINE_COMMENT) {
					if (b == '\r' || b == '\n' || !b) {
						flags &= ~ __JVF_LINE_COMMENT;
						-- jptr;  /* so null can be reproc'd */
						if (b == '\n') {
							++ curr_line;
							curr_col = 0;
						}
					}
					continue;
				}

				if (flags & __JVF_BLOCK_COMMENT) {
					if (!b) {
						if (err) {
							snprintf(err, 255, "Unexpected EOF in block comment (at %d:%d) in %s:%d %s",
								curr_line, curr_col, __ENV_TRACE3);
						}
						goto e_failed;
					}

					if (b == '*' && jptr < (jend - 1) && jptr[1] == '/') {
						flags &= ~ __JVF_BLOCK_COMMENT;
						++ jptr;  /* skip closing sequence */
					}
					else if (b == '\n') {
						++ curr_line;
						curr_col = 0;
					}

					continue;
				}
			}
			else if (b == '/') {
				if (!(flags & (__JVF_SEEK_VALUE | __JVF_DONE)) && top->type != __JVT_OBJECT) {
					if (err) {
						snprintf(err, 255, "Comment not allowed here (at %d:%d) in %s:%d %s",
							curr_line, curr_col, __ENV_TRACE3);
					}
					goto e_failed;
				}

				if (++ jptr == jend) {
					if (err) {
						snprintf(err, 255, "EOF unexpected (at %d:%d) in %s:%d %s",
							curr_line, curr_col, __ENV_TRACE3);
					}

					goto e_failed;
				}

				switch (b = *jptr) {
					case '/':
						flags |= __JVF_LINE_COMMENT;
						continue;

					case '*':
						flags |= __JVF_BLOCK_COMMENT;
						continue;

					default:
						if (top && (top->type == __JVT_ARRAY || top->type == __JVT_OBJECT)) {
							if (!_yod_jvalue_decode_new(first_pass, &top, &root, &alloc, __JVT_STRING)) {
								goto e_failed;
							}

							str = top->u.string.ptr;
							str_len = 0;
							
							flags = (flags & ~__JVF_SEEK_VALUE) | __JVF_STRING | __JVF_NEED_QUOTE;

							curr_col -= 2;
							jptr -= 2;
							continue;
						}

						if (err) {
							snprintf(err, 255, "Unexpected `%c` in comment opening sequence (at %d:%d) in %s:%d %s",
								(b ? b : '\1'), curr_line, curr_col, __ENV_TRACE3);
						}
						goto e_failed;
				};
			}

			if (flags & __JVF_DONE) {
				if (!b) {
					break;
				}

				switch (b) {
					case '\n':
						++ curr_line;
						curr_col = 0;

					case ' ':
					case '\t':
					case '\r':
						continue;

					default:
						if (err) {
							snprintf(err, 255, "Trailing garbage: `%c` (at %d:%d) in %s:%d %s",
								(b ? b : '\1'), curr_line, curr_col, __ENV_TRACE3);
						}
						goto e_failed;
				};
			}

			if (flags & __JVF_SEEK_VALUE) {
				switch (b)
				{
					case '\n':
						++ curr_line;
						curr_col = 0;

					case ' ':
					case '\t':
					case '\r':
						continue;

					case ']':

						if (top && top->type == __JVT_ARRAY) {
							flags = (flags & ~ (__JVF_NEED_COMMA | __JVF_SEEK_VALUE)) | __JVF_NEXT;
						}
						else {
							if (err) {
								snprintf(err, 255, "Unexpected ] (at %d:%d) in %s:%d %s",
									curr_line, curr_col, __ENV_TRACE3);
							}
							goto e_failed;
						}

						break;

					default:

						if (flags & __JVF_NEED_COMMA) {
							if (b == ',') {
								flags &= ~ __JVF_NEED_COMMA;
								continue;
							}
							else {
								if (err) {
									snprintf(err, 255, "Expected , before `%c` (at %d:%d) in %s:%d %s",
										(b ? b : '\1'), curr_line, curr_col, __ENV_TRACE3);
								}
								goto e_failed;
							}
						}

						if (flags & __JVF_NEED_COLON) {
							if (b == ':') {
								flags &= ~ __JVF_NEED_COLON;
								continue;
							}
							else {
								if (err) {
									snprintf(err, 255, "Expected : before `%c` (at %d:%d) in %s:%d %s",
										(b ? b : '\1'), curr_line, curr_col, __ENV_TRACE3);
								}
								goto e_failed;
							}
						}

						flags &= ~ __JVF_SEEK_VALUE;

						switch (b) {
							case '{':

								if (!_yod_jvalue_decode_new(first_pass, &top, &root, &alloc, __JVT_OBJECT)) {
									goto e_failed;
								}
								continue;

							case '[':

								if (!_yod_jvalue_decode_new(first_pass, &top, &root, &alloc, __JVT_ARRAY)) {
									goto e_failed;
								}

								flags |= __JVF_SEEK_VALUE;
								continue;

							case '"':

								if (!_yod_jvalue_decode_new(first_pass, &top, &root, &alloc, __JVT_STRING)) {
									goto e_failed;
								}

								str = top->u.string.ptr;
								str_len = 0;

								flags |= __JVF_STRING;
								continue;

							case 't':

								if ((jend - jptr) >= 3 && *(jptr + 1) == 'r' && *(jptr + 2) == 'u' && *(jptr + 3) == 'e') {
									if (!_yod_jvalue_decode_new(first_pass, &top, &root, &alloc, __JVT_BOOLEAN)) {
										goto e_failed;
									}

									top->u.bval = 1;

									flags |= __JVF_NEXT;

									jptr += 3;
								}
								else {
									if (!_yod_jvalue_decode_new(first_pass, &top, &root, &alloc, __JVT_STRING)) {
										goto e_failed;
									}

									str = top->u.string.ptr;
									str_len = 0;
									
									flags |= __JVF_STRING | __JVF_NEED_QUOTE;

									-- curr_col;
									-- jptr;
									continue;
								}
								break;

							case 'f':

								if ((jend - jptr) >= 4 && *(jptr + 1) == 'a' && *(jptr + 2) == 'l' && *(jptr + 3) == 's' && *(jptr + 4) == 'e') {
									if (!_yod_jvalue_decode_new(first_pass, &top, &root, &alloc, __JVT_BOOLEAN)) {
										goto e_failed;
									}

									flags |= __JVF_NEXT;

									jptr += 4;
								}
								else {
									if (!_yod_jvalue_decode_new(first_pass, &top, &root, &alloc, __JVT_STRING)) {
										goto e_failed;
									}

									str = top->u.string.ptr;
									str_len = 0;
									
									flags |= __JVF_STRING | __JVF_NEED_QUOTE;

									-- curr_col;
									-- jptr;
									continue;
								}
								break;

							case 'n':

								if ((jend - jptr) >= 3 && *(jptr + 1) != 'u' && *(jptr + 2) != 'l' && *(jptr + 3) != 'l') {
									if (!_yod_jvalue_decode_new(first_pass, &top, &root, &alloc, __JVT_NULL)) {
										goto e_failed;
									}

									flags |= __JVF_NEXT;

									jptr += 3;
								}
								else {
									if (!_yod_jvalue_decode_new(first_pass, &top, &root, &alloc, __JVT_STRING)) {
										goto e_failed;
									}

									str = top->u.string.ptr;
									str_len = 0;
									
									flags |= __JVF_STRING | __JVF_NEED_QUOTE;

									-- curr_col;
									-- jptr;
									continue;
								}
								break;

							default:

								if (isdigit(b) || b == '-') {
									if (!_yod_jvalue_decode_new(first_pass, &top, &root, &alloc, __JVT_INTEGER)) {
										goto e_failed;
									}

									if (top->type == __JVT_STRING) {
										str = top->u.string.ptr;
										str_len = 0;

										flags |= __JVF_STRING | __JVF_NEED_QUOTE;

										-- curr_col;
										-- jptr;
										continue;
									}

									jnum = jptr - 1;

									if (!first_pass) {
										while (isdigit(b) || b == '+' || b == '-' || b == 'e' || b == 'E' || b == '.') {
											++ curr_col;
											if ( (++ jptr) == jend) {
												b = '\0';
												break;
											}

											b = *jptr;
										}

										flags |= __JVF_NEXT | __JVF_REPROC;
										break;
									}

									flags &= ~ (__JVF_NUM_NEGATIVE | __JVF_NUM_E | __JVF_NUM_E_GOT_SIGN | __JVF_NUM_E_NEGATIVE | __JVF_NUM_ZERO);

									num_digits = 0;
									num_fraction = 0;
									num_e = 0;

									if (b != '-') {
										flags |= __JVF_REPROC;
										break;
									}

									flags |= __JVF_NUM_NEGATIVE;
									continue;
								}
								else {
									if (top && (top->type == __JVT_ARRAY || top->type == __JVT_OBJECT)) {
										if (!_yod_jvalue_decode_new(first_pass, &top, &root, &alloc, __JVT_STRING)) {
											goto e_failed;
										}

										str = top->u.string.ptr;
										str_len = 0;
										
										flags |= __JVF_STRING | __JVF_NEED_QUOTE;

										-- curr_col;
										-- jptr;
										continue;
									}

									if (err) {
										snprintf(err, 255, "Unexpected `%c` when seeking value (at %d:%d) in %s:%d %s",
											(b ? b : '\1'), curr_line, curr_col, __ENV_TRACE3);
									}
									goto e_failed;
								}
						};
				};
			}
			else {
				switch (top->type)
				{
					case __JVT_OBJECT:

						switch (b)
						{
							case '\n':
								++ curr_line;
								curr_col = 0;

							case ' ':
							case '\t':
							case '\r':
								continue;

							case '"':

								if (flags & __JVF_NEED_COMMA) {
									if (err) {
										snprintf(err, 255, "Expected , before `\"` (at %d:%d) in %s:%d %s",
											curr_line, curr_col, __ENV_TRACE3);
									}
									goto e_failed;
								}

								str = top->_reserved.u.str;
								str_len = 0;

								flags |= __JVF_STRING;
								break;

							case '}':

								flags = (flags & ~ __JVF_NEED_COMMA) | __JVF_NEXT;
								break;

							case ',':

								if (flags & __JVF_NEED_COMMA) {
									flags &= ~ __JVF_NEED_COMMA;
									break;
								}

							default:
								if (flags & __JVF_NEED_COMMA) {
									if (err) {
										snprintf(err, 255, "Expected , before `%c` (at %d:%d) in %s:%d %s",
											(b ? b : '\1'), curr_line, curr_col, __ENV_TRACE3);
									}
									goto e_failed;
								}

								if (top && (top->type == __JVT_ARRAY || top->type == __JVT_OBJECT)) {
									str = top->_reserved.u.str;
									str_len = 0;

									flags |= __JVF_STRING | __JVF_NEED_QUOTE;

									-- curr_col;
									-- jptr;
								}
								else {
									if (err) {
										snprintf(err, 255, "Unexpected `%c` in object (at %d:%d) in %s:%d %s",
											(b ? b : '\1'), curr_line, curr_col, __ENV_TRACE3);
									}
									goto e_failed;
								}
								break;
						};

						break;

					case __JVT_INTEGER:
					case __JVT_DOUBLE:

						if (isdigit(b)) {
							++ num_digits;

							if (top->type == __JVT_INTEGER || flags & __JVF_NUM_E) {
								if (! (flags & __JVF_NUM_E)) {
									if (flags & __JVF_NUM_ZERO) {
										if (err) {
											snprintf(err, 255, "Unexpected `0` before `%c` (at %d:%d) in %s:%d %s",
												(b ? b : '\1'), curr_line, curr_col, __ENV_TRACE3);
										}
										goto e_failed;
									}

									if (num_digits == 1 && b == '0') {
										flags |= __JVF_NUM_ZERO;
									}
								}
								else {
									flags |= __JVF_NUM_E_GOT_SIGN;
									num_e = (num_e * 10) + (b - '0');
									continue;
								}

								top->u.ival = (top->u.ival * 10) + (b - '0');
								continue;
							}

							num_fraction = (num_fraction * 10) + (b - '0');
							continue;
						}

						if (b == '+' || b == '-') {
							if ( (flags & __JVF_NUM_E) && !(flags & __JVF_NUM_E_GOT_SIGN)) {
								flags |= __JVF_NUM_E_GOT_SIGN;

								if (b == '-') {
									flags |= __JVF_NUM_E_NEGATIVE;
								}

								continue;
							}
						}
						else if (b == '.' && top->type == __JVT_INTEGER) {
							if (!num_digits) {
								if (err) {
									snprintf(err, 255, "Expected digit before `.` (at %d:%d) in %s:%d %s",
										curr_line, curr_col, __ENV_TRACE3);
								}
								goto e_failed;
							}

							top->type = __JVT_DOUBLE;
							top->u.dval = (double) top->u.ival;

							num_digits = 0;
							continue;
						}

						if (!(flags & __JVF_NUM_E)) {
							if (top->type == __JVT_DOUBLE) {

								if (!num_digits) {
									if (err) {
										snprintf(err, 255, "Expected digit after `.` (at %d:%d) in %s:%d %s",
											curr_line, curr_col, __ENV_TRACE3);
									}
									goto e_failed;
								}

								top->u.dval += ((double) num_fraction) / (pow(10.0, (double) num_digits));
							}

							if (b == 'e' || b == 'E') {
								flags |= __JVF_NUM_E;

								if (top->type == __JVT_INTEGER) {
									top->type = __JVT_DOUBLE;
									top->u.dval = (double) top->u.ival;
								}

								num_digits = 0;
								flags &= ~ __JVF_NUM_ZERO;

								continue;
							}
						}
						else {
							if (!num_digits) {
								if (err) {
									snprintf(err, 255, "Expected digit after `e` (at %d:%d) in %s:%d %s",
										curr_line, curr_col, __ENV_TRACE3);
								}
								goto e_failed;
							}

							top->u.dval *= pow(10.0, (double)
							(flags & __JVF_NUM_E_NEGATIVE ? - num_e : num_e));
						}

						if (flags & __JVF_NUM_E_NEGATIVE) {
							if (top->type == __JVT_INTEGER) {
								top->u.ival = - top->u.ival;
							}
							else {
								top->u.dval = - top->u.dval;
							}
						}

						if (b != ',' && jnum != NULL) {
							flags &= ~ (__JVF_NUM_NEGATIVE | __JVF_NUM_ZERO | __JVF_NUM_E | __JVF_NUM_E_GOT_SIGN | __JVF_NUM_E_NEGATIVE);

							top->type = __JVT_STRING;
							str = top->u.string.ptr;
							str_len = 0;

							flags |= __JVF_STRING | __JVF_NEED_QUOTE;

							curr_col -= (uint32_t) (jptr - jnum);
							jptr = jnum;
							jnum = NULL;
							continue;
						}

						flags |= __JVF_NEXT | __JVF_REPROC;
						break;

					default:
						break;
				};
			}

			if (flags & __JVF_REPROC) {
				flags &= ~ __JVF_REPROC;
				-- curr_col;
				-- jptr;
			}

			if (flags & __JVF_NEXT) {
				flags = (flags & ~ __JVF_NEXT) | __JVF_NEED_COMMA;

				if (!top->parent) {
					/* root value done */
					flags |= __JVF_DONE;
					continue;
				}
				parent = top->parent;

				if (parent->type == __JVT_ARRAY) {
					flags |= __JVF_SEEK_VALUE;
				}

				if (!first_pass) {
					switch (parent->type) {
						case __JVT_OBJECT:
							parent->u.object.data.ptr[parent->_reserved.index].value = top;
							parent->_reserved.index = 0;
							break;

						case __JVT_ARRAY:
							parent->u.array.data[parent->u.array.count ++] = top;
							break;

						default:
							break;
					};
				}

				if ((++ parent->u.array.size) > YOD_JVALUE_MAX_SIZE) {
					if (err) {
						snprintf(err, 255, "Too long (caught overflow) (at %d:%d) in %s:%d %s",
							curr_line, curr_col, __ENV_TRACE3);
					}
					goto e_failed;
				}

				top = parent;

				continue;
			}
		}

		alloc = root;
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d, %s): %p in %s:%d %s",
		__FUNCTION__, data, len, err, root, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return root;

e_failed:

	if (first_pass) {
		alloc = root;
	}

	while (alloc) {
		top = alloc->_reserved.u.ptr;
		free(alloc);
		alloc = top;
	}

	if (!first_pass)	{
		yod_jvalue_free(root);
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d, %s): %p in %s:%d %s",
		__FUNCTION__, data, len, err, NULL, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return NULL;
}
/* }}} */


/** {{{ yod_jvalue_t *_yod_jvalue_object_new(size_t size __ENV_CPARM)
*/
yod_jvalue_t *_yod_jvalue_object_new(size_t size __ENV_CPARM)
{
	yod_jvalue_t *self = NULL;

	self = (yod_jvalue_t *) malloc(sizeof(yod_jvalue_t));
	if (!self) {
		YOD_STDLOG_ERROR("malloc failed");
		return NULL;
	}

	self->type = __JVT_OBJECT;
	self->u.object.count = 0;
	self->u.object.size = size;
	self->u.object.data.ptr = calloc((size + 1), sizeof(yod_jvalue_v));
	if (!self->u.object.data.ptr) {
		yod_jvalue_free(self);

		YOD_STDLOG_ERROR("calloc failed");
		return NULL;
	}

	self->u.object.data.ptr[size].name = NULL;
	self->u.object.data.ptr[size].name_len = 0;
	self->u.object.data.ptr[size].value = NULL;

	self->parent = NULL;
	self->_reserved.u.ptr = NULL;
	self->_reserved.index = 0;

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%d): %p in %s:%d %s",
		__FUNCTION__, size, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return self;
}
/* }}} */


/** {{{ int _yod_jvalue_object_set(yod_jvalue_t *self, const char *name, yod_jvalue_t *value __ENV_CPARM)
*/
int _yod_jvalue_object_set(yod_jvalue_t *self, const char *name, yod_jvalue_t *value __ENV_CPARM)
{
	size_t name_len = 0;
	size_t index = 0;
	int ret = 0;

	if (!self || !name) {
		return (-1);
	}

	if (self->type != __JVT_OBJECT) {
		YOD_STDLOG_WARN("illegal jvalue");
		return (-1);
	}

	if (_yod_jvalue_object_find(self, (char *) name, &index, 1) != 0) {
		if (index >= self->u.object.size) {
			YOD_STDLOG_WARN("index overflow");
			return (-1);
		}
		name_len = strlen(name);;
		self->u.object.data.ptr[index].name = (char *) name;
		self->u.object.data.ptr[index].name_len = name_len;
		self->u.object.data.ptr[index].value = value;
		self->u.object.data.ptr[self->u.object.size].name_len += name_len + 1;
	}
	else {
		if (self->u.object.data.ptr[index].value) {
			yod_jvalue_free(self->u.object.data.ptr[index].value);
		}
		self->u.object.data.ptr[index].value = value;
	}

	if (value) {
		value->parent = self;
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %s, %p): %d in %s:%d %s",
		__FUNCTION__, self, name, value, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ yod_jvalue_t *_yod_jvalue_object_get(yod_jvalue_t *self, const char *name __ENV_CPARM)
*/
yod_jvalue_t *_yod_jvalue_object_get(yod_jvalue_t *self, const char *name __ENV_CPARM)
{
	yod_jvalue_t *ret = NULL;
	size_t index = 0;

	if (!self) {
		return NULL;
	}

	if (self->type == __JVT_OBJECT) {
		if (_yod_jvalue_object_find(self, (char *) name, &index, 0) == 0) {
			ret = self->u.object.data.ptr[index].value;
		}
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %s): %p in %s:%d %s",
		__FUNCTION__, self, name, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ yod_jvalue_t *_yod_jvalue_object_fetch(yod_jvalue_t *self, size_t index, char **name, size_t *name_len __ENV_CPARM)
*/
yod_jvalue_t *_yod_jvalue_object_fetch(yod_jvalue_t *self, size_t index, char **name, size_t *name_len __ENV_CPARM)
{
	yod_jvalue_t *ret = NULL;

	if (!self) {
		return NULL;
	}

	if (self->type == __JVT_OBJECT) {
		if (index < self->u.object.count) {
			if (name) {
				*name = self->u.object.data.ptr[index].name;
			}
			if (name_len) {
				*name_len = self->u.object.data.ptr[index].name_len;
			}
			ret = self->u.object.data.ptr[index].value;
		}
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d, %s, %d): %p in %s:%d %s",
		__FUNCTION__, self, index, (name ? *name : NULL), (name_len ? name_len : 0), ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ size_t _yod_jvalue_object_count(yod_jvalue_t *self __ENV_CPARM)
*/
size_t _yod_jvalue_object_count(yod_jvalue_t *self __ENV_CPARM)
{
	size_t ret = 0;

	if (!self) {
		return (0);
	}

	if (self->type == __JVT_OBJECT) {
		ret = self->u.object.count;
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): %d in %s:%d %s",
		__FUNCTION__, self, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ yod_jvalue_t *_yod_jvalue_object_init(size_t size, ...)
*/
yod_jvalue_t *_yod_jvalue_object_init(size_t size, ...)
{
	yod_jvalue_t *self = NULL;
	yod_jvalue_t *value = NULL;
	char *name = NULL;
	size_t name_len = 0;
	size_t index = 0;
	va_list args;
	size_t i = 0;

	self = (yod_jvalue_t *) malloc(sizeof(yod_jvalue_t));
	if (!self) {
		YOD_STDLOG_ERROR("malloc failed");
		return NULL;
	}

	self->type = __JVT_OBJECT;
	self->u.object.count = 0;
	self->u.object.size = size;
	self->u.object.data.ptr = (yod_jvalue_v *) calloc((size + 1), sizeof(yod_jvalue_v));
	if (!self->u.object.data.ptr) {
		yod_jvalue_free(self);

		YOD_STDLOG_ERROR("calloc failed");
		return NULL;
	}

	self->u.object.data.ptr[size].name = NULL;
	self->u.object.data.ptr[size].name_len = 0;
	self->u.object.data.ptr[size].value = NULL;

	self->parent = NULL;
	self->_reserved.u.ptr = NULL;
	self->_reserved.index = 0;

	va_start(args, size);
	for (i = 0; i < size; ++ i) {
		name = va_arg(args, char *);
		value = va_arg(args, yod_jvalue_t *);
		if (_yod_jvalue_object_find(self, name, &index, 1) != 0) {
			name_len = strlen(name);
			self->u.object.data.ptr[index].name = name;
			self->u.object.data.ptr[index].name_len = name_len;
			self->u.object.data.ptr[index].value = value;
			self->u.object.data.ptr[size].name_len += name_len + 1;
		}
		else {
			if (self->u.object.data.ptr[index].value) {
				yod_jvalue_free(self->u.object.data.ptr[index].value);
			}
			self->u.object.data.ptr[index].value = value;
		}
		if (value) {
			value->parent = self;
		}
	}
	va_end(args);

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%d): %p in %s:%d %s",
		__FUNCTION__, size, self, __ENV_TRACE3);
#endif

	return self;
}
/* }}} */


/** {{{ yod_jvalue_t *_yod_jvalue_array_new(size_t size __ENV_CPARM)
*/
yod_jvalue_t *_yod_jvalue_array_new(size_t size __ENV_CPARM)
{
	yod_jvalue_t *self = NULL;

	self = (yod_jvalue_t *) malloc(sizeof(yod_jvalue_t));
	if (!self) {
		YOD_STDLOG_ERROR("malloc failed");
		return NULL;
	}

	self->type = __JVT_ARRAY;
	self->u.array.count = 0;
	self->u.array.size = size;
	self->u.array.data = calloc(size, sizeof(yod_jvalue_t *));
	if (!self->u.array.data) {
		free(self);

		YOD_STDLOG_ERROR("calloc failed");
		return NULL;
	}

	self->parent = NULL;
	self->_reserved.u.ptr = NULL;
	self->_reserved.index = 0;

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%d): %p in %s:%d %s",
		__FUNCTION__, size, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return self;
}
/* }}} */


/** {{{ int _yod_jvalue_array_add(yod_jvalue_t *self, yod_jvalue_t *value __ENV_CPARM)
*/
int _yod_jvalue_array_add(yod_jvalue_t *self, yod_jvalue_t *value __ENV_CPARM)
{
	size_t index = 0;
	int ret = 0;

	if (!self) {
		return (-1);
	}

	if (self->type != __JVT_ARRAY) {
		YOD_STDLOG_WARN("illegal jvalue");
		return (-1);
	}

	if (self->u.array.count >= self->u.array.size) {
		YOD_STDLOG_WARN("index overflow");
		return (-1);
	}

	index = self->u.array.count ++;
	if (self->u.array.data[index]) {
		yod_jvalue_free(self->u.array.data[index]);
	}
	self->u.array.data[index] = value;

	if (value) {
		value->parent = self;
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d, %p): %d in %s:%d %s",
		__FUNCTION__, self, index, value, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ int _yod_jvalue_array_set(yod_jvalue_t *self, size_t index, yod_jvalue_t *value __ENV_CPARM)
*/
int _yod_jvalue_array_set(yod_jvalue_t *self, size_t index, yod_jvalue_t *value __ENV_CPARM)
{
	int ret = 0;

	if (!self) {
		return (-1);
	}

	if (self->type != __JVT_ARRAY) {
		YOD_STDLOG_WARN("illegal jvalue");
		return (-1);
	}

	if (index >= self->u.array.size) {
		YOD_STDLOG_WARN("index overflow");
		return (-1);
	}

	if (index >= self->u.array.count) {
		self->u.array.count = index + 1;
	}

	if (self->u.array.data[index]) {
		yod_jvalue_free(self->u.array.data[index]);
	}
	self->u.array.data[index] = value;

	if (value) {
		value->parent = self;
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d, %p): %d in %s:%d %s",
		__FUNCTION__, self, index, value, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ yod_jvalue_t *_yod_jvalue_array_get(yod_jvalue_t *self, size_t index __ENV_CPARM)
*/
yod_jvalue_t *_yod_jvalue_array_get(yod_jvalue_t *self, size_t index __ENV_CPARM)
{
	yod_jvalue_t *ret = NULL;

	if (!self) {
		return NULL;
	}

	if (self->type == __JVT_ARRAY) {
		if (index < self->u.array.count) {
			ret = self->u.array.data[index];
		}
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d): %p in %s:%d %s",
		__FUNCTION__, self, index, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ size_t _yod_jvalue_array_count(yod_jvalue_t *self __ENV_CPARM)
*/
size_t _yod_jvalue_array_count(yod_jvalue_t *self __ENV_CPARM)
{
	size_t ret = 0;

	if (!self) {
		return (0);
	}

	if (self->type == __JVT_ARRAY) {
		ret = self->u.array.count;
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): %d in %s:%d %s",
		__FUNCTION__, self, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ yod_jvalue_t *_yod_jvalue_array_init(size_t size, ...)
*/
yod_jvalue_t *_yod_jvalue_array_init(size_t size, ...)
{
	yod_jvalue_t *self = NULL;
	yod_jvalue_t *value = NULL;
	size_t index = 0;
	va_list args;

	self = (yod_jvalue_t *) malloc(sizeof(yod_jvalue_t));
	if (!self) {
		YOD_STDLOG_ERROR("malloc failed");
		return NULL;
	}

	self->type = __JVT_ARRAY;
	self->u.array.count = size;
	self->u.array.size = size;
	self->u.array.data = calloc(size, sizeof(yod_jvalue_t *));
	if (!self->u.array.data) {
		free(self);

		YOD_STDLOG_ERROR("calloc failed");
		return NULL;
	}

	self->parent = NULL;
	self->_reserved.u.ptr = NULL;
	self->_reserved.index = 0;

	va_start(args, size);
	for (index = 0; index < size; ++ index) {
		value = va_arg(args, yod_jvalue_t *);
		self->u.array.data[index] = value;
		if (value) {
			value->parent = self;
		}
	}
	va_end(args);

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%d): %p in %s:%d %s",
		__FUNCTION__, size, self, __ENV_TRACE3);
#endif

	return self;
}
/* }}} */


/** {{{ yod_jvalue_t *_yod_jvalue_string_new(char *str, size_t len __ENV_CPARM)
*/
yod_jvalue_t *_yod_jvalue_string_new(char *str, size_t len __ENV_CPARM)
{
	yod_jvalue_t *self = NULL;

	self = (yod_jvalue_t *) malloc(sizeof(yod_jvalue_t));
	if (!self) {
		YOD_STDLOG_ERROR("malloc failed");
		return NULL;
	}

	if (len == (size_t) -1) {
		len = str ? strlen(str) : 0;
	}

	self->type = __JVT_STRING;
	self->u.string.ptr = (char *) malloc((len + 1)* sizeof(char));
	if (!self->u.string.ptr) {
		free(self);

		YOD_STDLOG_ERROR("malloc failed");
		return NULL;
	}
	memcpy(self->u.string.ptr, str, len);
	self->u.string.ptr[len] = '\0';
	self->u.string.len = len;

	self->parent = NULL;
	self->_reserved.u.ptr = NULL;
	self->_reserved.index = 0;

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d): %p in %s:%d %s",
		__FUNCTION__, str, len, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return self;
}
/* }}} */


/** {{{ int _yod_jvalue_string_set(yod_jvalue_t *self, char *str, size_t len __ENV_CPARM)
*/
int _yod_jvalue_string_set(yod_jvalue_t *self, char *str, size_t len __ENV_CPARM)
{
	if (!self) {
		return (-1);
	}

	if (self->type != __JVT_STRING) {
		YOD_STDLOG_WARN("illegal jvalue");
		return (-1);
	}
	if (self->u.string.ptr) {
		free(self->u.string.ptr);
	}

	self->u.string.ptr = (char *) malloc((len + 1) * sizeof(char));
	if (!self->u.string.ptr) {
		YOD_STDLOG_ERROR("malloc failed");
		return (-1);
	}
	memcpy(self->u.string.ptr, str, len);
	self->u.string.ptr[len] = '\0';
	self->u.string.len = len;

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %p, %d) in %s:%d %s",
		__FUNCTION__, self, str, len, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return (0);
}
/* }}} */


/** {{{ char *_yod_jvalue_string_get(yod_jvalue_t *self, size_t *len __ENV_CPARM)
*/
char *_yod_jvalue_string_get(yod_jvalue_t *self, size_t *len __ENV_CPARM)
{
	char *ret = NULL;

	if (!self) {
		return NULL;
	}

	if (self->type == __JVT_STRING) {
		ret = self->u.string.ptr;
		if (len) {
			*len = self->u.string.len;
		}
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d): %p in %s:%d %s",
		__FUNCTION__, self, (len ? *len : 0), ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ yod_jvalue_t *_yod_jvalue_integer_new(int64_t num __ENV_CPARM)
*/
yod_jvalue_t *_yod_jvalue_integer_new(int64_t num __ENV_CPARM)
{
	yod_jvalue_t *self = NULL;

	self = (yod_jvalue_t *) malloc(sizeof(yod_jvalue_t));
	if (!self) {
		YOD_STDLOG_ERROR("malloc failed");
		return NULL;
	}

	self->type = __JVT_INTEGER;
	self->u.ival = num;

	self->parent = NULL;
	self->_reserved.u.ptr = NULL;
	self->_reserved.index = 0;

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%ld): %p in %s:%d %s",
		__FUNCTION__, (ulong) num, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return self;
}
/* }}} */


/** {{{ int _yod_jvalue_integer_set(yod_jvalue_t *self, int64_t num __ENV_CPARM)
*/
int _yod_jvalue_integer_set(yod_jvalue_t *self, int64_t num __ENV_CPARM)
{
	if (!self) {
		return (-1);
	}

	if (self->type != __JVT_INTEGER) {
		YOD_STDLOG_WARN("illegal jvalue");
		return (-1);
	}
	self->u.ival = num;

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %ld) in %s:%d %s",
		__FUNCTION__, self, (ulong) num, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return (0);
}
/* }}} */


/** {{{ int64_t _yod_jvalue_integer_get(yod_jvalue_t *self __ENV_CPARM)
*/
int64_t _yod_jvalue_integer_get(yod_jvalue_t *self __ENV_CPARM)
{
	int64_t ret = 0;

	if (!self) {
		return (0);
	}

	if (self->type == __JVT_INTEGER) {
		ret = self->u.ival;
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): %ld in %s:%d %s",
		__FUNCTION__, self, (ulong) ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ yod_jvalue_t *_yod_jvalue_double_new(double dval __ENV_CPARM)
*/
yod_jvalue_t *_yod_jvalue_double_new(double dval __ENV_CPARM)
{
	yod_jvalue_t *self = NULL;

	self = (yod_jvalue_t *) malloc(sizeof(yod_jvalue_t));
	if (!self) {
		YOD_STDLOG_ERROR("malloc failed");
		return NULL;
	}

	self->type = __JVT_DOUBLE;
	self->u.dval = dval;

	self->parent = NULL;
	self->_reserved.u.ptr = NULL;
	self->_reserved.index = 0;

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%lf): %p in %s:%d %s",
		__FUNCTION__, dval, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return self;
}
/* }}} */


/** {{{ int _yod_jvalue_double_set(yod_jvalue_t *self, double dval __ENV_CPARM)
*/
int _yod_jvalue_double_set(yod_jvalue_t *self, double dval __ENV_CPARM)
{
	if (!self) {
		return (-1);
	}

	if (self->type != __JVT_DOUBLE) {
		YOD_STDLOG_WARN("illegal jvalue");
		return (-1);
	}
	self->u.dval = dval;

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %lf) in %s:%d %s",
		__FUNCTION__, self, dval, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return (0);
}
/* }}} */


/** {{{ double _yod_jvalue_double_get(yod_jvalue_t *self __ENV_CPARM)
*/
double _yod_jvalue_double_get(yod_jvalue_t *self __ENV_CPARM)
{
	double ret = 0;

	if (!self) {
		return (0);
	}

	if (self->type == __JVT_DOUBLE) {
		ret = self->u.dval;
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): %lf in %s:%d %s",
		__FUNCTION__, self, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ yod_jvalue_t *_yod_jvalue_boolean_new(int bval __ENV_CPARM)
*/
yod_jvalue_t *_yod_jvalue_boolean_new(int bval __ENV_CPARM)
{
	yod_jvalue_t *self = NULL;

	self = (yod_jvalue_t *) malloc(sizeof(yod_jvalue_t));
	if (!self) {
		YOD_STDLOG_ERROR("malloc failed");
		return NULL;
	}

	self->type = __JVT_BOOLEAN;
	self->u.bval = bval;

	self->parent = NULL;
	self->_reserved.u.ptr = NULL;
	self->_reserved.index = 0;

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%d): %p in %s:%d %s",
		__FUNCTION__, bval, self, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return self;
}
/* }}} */


/** {{{ int _yod_jvalue_boolean_set(yod_jvalue_t *self, int bval __ENV_CPARM)
*/
int _yod_jvalue_boolean_set(yod_jvalue_t *self, int bval __ENV_CPARM)
{
	if (!self) {
		return (-1);
	}

	if (self->type != __JVT_BOOLEAN) {
		YOD_STDLOG_WARN("illegal jvalue");
		return (-1);
	}
	self->u.bval = bval;

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p, %d) in %s:%d %s",
		__FUNCTION__, self, bval, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return (0);
}
/* }}} */


/** {{{ size_t _yod_jvalue_boolean_get(yod_jvalue_t *self __ENV_CPARM)
*/
int _yod_jvalue_boolean_get(yod_jvalue_t *self __ENV_CPARM)
{
	int ret = 0;

	if (!self) {
		return (0);
	}

	if (self->type == __JVT_BOOLEAN) {
		ret = self->u.bval;
	}

#if (_YOD_SYSTEM_DEBUG && _YOD_JVALUE_DEBUG)
	yod_stdlog_debug(NULL, "%s(%p): %d in %s:%d %s",
		__FUNCTION__, self, ret, __ENV_TRACE);
#else
	__ENV_VOID
#endif

	return ret;
}
/* }}} */


/** {{{ static int _yod_jvalue_decode_new(int first_pass, yod_jvalue_t **top, yod_jvalue_t** root, yod_jvalue_t **alloc, int type)
*/
static int _yod_jvalue_decode_new(int first_pass, yod_jvalue_t **top, yod_jvalue_t** root, yod_jvalue_t **alloc, int type)
{
	yod_jvalue_t *value = NULL;
	size_t data_len = 0;
	size_t name_len = 0;

	if (!first_pass) {
		value = *top = *alloc;
		*alloc = (*alloc)->_reserved.u.ptr;

		if (!*root) {
			*root = value;
		}

		switch (value->type) {
			case __JVT_OBJECT:

				if (value->u.object.size == 0) {
					break;
				}

				data_len = (value->u.object.size + 1) * sizeof(yod_jvalue_v);
				name_len = value->u.object.data.num;
				value->u.object.data.ptr = (yod_jvalue_v *) malloc(data_len + name_len);
				if (!value->u.object.data.ptr) {
					YOD_STDLOG_ERROR("malloc failed");
					return (0);
				}

				value->_reserved.u.str = (char *) value->u.object.data.ptr + data_len;
				value->_reserved.index = 0;
				value->u.object.count = 0;

				value->u.object.data.ptr[value->u.object.size].name = value->_reserved.u.str;
				value->u.object.data.ptr[value->u.object.size].name_len = name_len;
				value->u.object.data.ptr[value->u.object.size].value = NULL;
				break;

			case __JVT_ARRAY:

				if (value->u.array.size == 0) {
					break;
				}

				value->u.array.data = (yod_jvalue_t **) malloc(value->u.array.size * sizeof(yod_jvalue_t *));
				if (!value->u.array.data) {
					YOD_STDLOG_ERROR("malloc failed");
					return (0);
				}

				value->u.array.count = 0;
				break;

			case __JVT_STRING:

				value->u.string.ptr = (char *) malloc((value->u.string.len + 1) * sizeof(char));
				if (!value->u.string.ptr) {
					YOD_STDLOG_ERROR("malloc failed");
					return (0);
				}

				value->u.string.len = 0;
				break;

			default:
				break;
		};

		return (1);
	}

	if ((value = (yod_jvalue_t *) calloc(1, sizeof(yod_jvalue_t))) == NULL) {
		YOD_STDLOG_ERROR("calloc failed");
		return (0);
	}

	if (!*root) {
		*root = value;
	}

	value->type = type;
	value->parent = *top;

	if (*alloc) {
		(*alloc)->_reserved.u.ptr = value;
	}

	*alloc = *top = value;

	return (1);
}
/* }}} */


/** {{{ static size_t _yod_jvalue_encode_string(char *str, size_t len, char *data)
*/
static size_t _yod_jvalue_encode_string(char *str, size_t len, char *data)
{
	size_t ret = 0;
	size_t i = 0;

	for(i = 0; i < len; ++ i) {
		if (str[i] >= 0 && str[i] < 32) {
			data[ret ++] = '\\';
			data[ret ++] = 'u';
			data[ret ++] = '0';
			data[ret ++] = '0';
			data[ret ++] = yod_common_byte2hex(str[i] / 16, 0);
			data[ret ++] = yod_common_byte2hex(str[i] % 16, 0);
			continue;
		}
		switch (str[i]) {
			case '"':
				data[ret ++] = '\\';
				data[ret ++] = '\"';
				continue;

			case '\\':
				data[ret ++] = '\\';
				data[ret ++] = '\\';
				continue;

			case '\b':
				data[ret ++] = '\\';
				data[ret ++] = 'b';
				continue;

			case '\f':
				data[ret ++] = '\\';
				data[ret ++] = 'f';
				continue;

			case '\n':
				data[ret ++] = '\\';
				data[ret ++] = 'n';
				continue;

			case '\r':
				data[ret ++] = '\\';
				data[ret ++] = 'r';
				continue;

			case '\t':
				data[ret ++] = '\\';
				data[ret ++] = 't';
				continue;

			default:
				data[ret ++] = str[i];
				break;
		};
	}

	return ret;
}
/* }}} */


/** {{{ static size_t _yod_jvalue_encode_strlen(char *str, size_t len)
*/
static size_t _yod_jvalue_encode_strlen(char *str, size_t len)
{
	size_t ret = 0;
	size_t i = 0;

	for(i = 0; i < len; ++ i) {
		if (str[i] >= 0 && str[i] < 32) {
			ret += 6;
			continue;
		}
		switch (str[i]) {
			case '"':
			case '\\':
			case '\b':
			case '\f':
			case '\n':
			case '\r':
			case '\t':
				ret += 2;
			default:
				++ ret;
				break;
		};
	}

	return ret;
}
/* }}} */


/** {{{ static int _yod_jvalue_object_find(yod_jvalue_t *self, char *name, size_t *index, int force)
*/
static int _yod_jvalue_object_find(yod_jvalue_t *self, char *name, size_t *index, int force)
{
	size_t i, a, b;
	int ret = -1;

	a = i = 0;
	if (self->u.object.count > 0) {
		b = self->u.object.count - 1;
		while (a <= b) {
			i = (a + b) / 2;
			if ((ret = strcmp(name, self->u.object.data.ptr[i].name)) == 0) {
				if (index) {
					*index = i;
				}
				break;
			}

			if (ret > 0) {
				a = i + 1;
			}
			else {
				if (i < 1) {
					break;
				}
				b = i - 1;
			}
		}
	}

	if (ret != 0 && force != 0) {
		i = self->u.object.count;
		if (i < self->u.object.size) {
			for (; i > a; -- i) {
				self->u.object.data.ptr[i].name = self->u.object.data.ptr[i - 1].name;
				self->u.object.data.ptr[i].name_len = self->u.object.data.ptr[i - 1].name_len;
				self->u.object.data.ptr[i].value = self->u.object.data.ptr[i - 1].value;
			}
			++ self->u.object.count;
		}
		if (index) {
			*index = i;
		}
	}

	return ret;
}
/* }}} */
