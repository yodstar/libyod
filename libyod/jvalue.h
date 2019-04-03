#ifndef __YOD_JVALUE_H__
#define __YOD_JVALUE_H__

#include "system.h"


/* yod_jvalue_t */
typedef struct _yod_jvalue_t 									yod_jvalue_t;


enum
{
	YOD_JVALUE_TYPE_NONE,
	YOD_JVALUE_TYPE_OBJECT,
	YOD_JVALUE_TYPE_ARRAY,
	YOD_JVALUE_TYPE_STRING,
	YOD_JVALUE_TYPE_INTEGER,
	YOD_JVALUE_TYPE_DOUBLE,
	YOD_JVALUE_TYPE_BOOLEAN,
	YOD_JVALUE_TYPE_NULL
};


#define __JVT_NONE 												YOD_JVALUE_TYPE_NONE
#define __JVT_OBJECT 											YOD_JVALUE_TYPE_OBJECT
#define __JVT_ARRAY 											YOD_JVALUE_TYPE_ARRAY
#define __JVT_STRING 											YOD_JVALUE_TYPE_STRING
#define __JVT_INTEGER 											YOD_JVALUE_TYPE_INTEGER
#define __JVT_DOUBLE 											YOD_JVALUE_TYPE_DOUBLE
#define __JVT_BOOLEAN 											YOD_JVALUE_TYPE_BOOLEAN
#define __JVT_NULL												YOD_JVALUE_TYPE_NULL


#define yod_jvalue_new() 										_yod_jvalue_new(__ENV_ARGS)
#define yod_jvalue_free(x) 										_yod_jvalue_free(x __ENV_CARGS)

#define yod_jvalue_type(x) 										_yod_jvalue_type(x __ENV_CARGS)
#define yod_jvalue_clone(x) 									_yod_jvalue_clone(x __ENV_CARGS)
#define yod_jvalue_load(f, e) 									_yod_jvalue_load(f, e __ENV_CARGS)
#define yod_jvalue_dump(x) 										_yod_jvalue_dump(x __ENV_CARGS)

#define yod_jvalue_encode(d, x) 								_yod_jvalue_encode(d, x __ENV_CARGS)
#define yod_jvalue_decode(d, l, e) 								_yod_jvalue_decode(d, l, e __ENV_CARGS)

#define yod_jobject_new(z) 										_yod_jvalue_object_new(z __ENV_CARGS)
#define yod_jobject_set(x, k, v) 								_yod_jvalue_object_set(x, k, v __ENV_CARGS)
#define yod_jobject_get(x, k) 									_yod_jvalue_object_get(x, k __ENV_CARGS)
#define yod_jobject_fetch(x, i, k, l) 							_yod_jvalue_object_fetch(x, i, k, l __ENV_CARGS)
#define yod_jobject_count(x) 									_yod_jvalue_object_count(x __ENV_CARGS)
#define yod_jobject_init 										_yod_jvalue_object_init

#define yod_jarray_new(z) 										_yod_jvalue_array_new(z __ENV_CARGS)
#define yod_jarray_add(x, v) 									_yod_jvalue_array_add(x, v __ENV_CARGS)
#define yod_jarray_set(x, i, v) 								_yod_jvalue_array_set(x, i, v __ENV_CARGS)
#define yod_jarray_get(x, i) 									_yod_jvalue_array_get(x, i __ENV_CARGS)
#define yod_jarray_count(x) 									_yod_jvalue_array_count(x __ENV_CARGS)
#define yod_jarray_init 										_yod_jvalue_array_init

#define yod_jstring_new(s, l) 									_yod_jvalue_string_new(s, l __ENV_CARGS)
#define yod_jstring_set(x, s, l) 								_yod_jvalue_string_set(x, s, l __ENV_CARGS)
#define yod_jstring_get(x, l) 									_yod_jvalue_string_get(x, l __ENV_CARGS)

#define yod_jinteger_new(n) 									_yod_jvalue_integer_new(n __ENV_CARGS)
#define yod_jinteger_set(x, n) 									_yod_jvalue_integer_set(x, n __ENV_CARGS)
#define yod_jinteger_get(x) 									_yod_jvalue_integer_get(x __ENV_CARGS)

#define yod_jdouble_new(d) 										_yod_jvalue_double_new(d __ENV_CARGS)
#define yod_jdouble_set(x, d) 									_yod_jvalue_double_set(x, d __ENV_CARGS)
#define yod_jdouble_get(x) 										_yod_jvalue_double_get(x __ENV_CARGS)

#define yod_jboolean_new(b) 									_yod_jvalue_boolean_new(b __ENV_CARGS)
#define yod_jboolean_set(x, b) 									_yod_jvalue_boolean_set(x, b __ENV_CARGS)
#define yod_jboolean_get(x) 									_yod_jvalue_boolean_get(x __ENV_CARGS)

#define yod_jobject_str(x, k, l) 								yod_jstring_get(yod_jobject_get(x, k), l)
#define yod_jobject_int(x, k) 									yod_jinteger_get(yod_jobject_get(x, k))
#define yod_jobject_dbl(x, k) 									yod_jdouble_get(yod_jobject_get(x, k))
#define yod_jobject_bool(x, k) 									yod_jboolean_get(yod_jobject_get(x, k))

#define yod_jobject_fetch_str(x, i, k, l, n) 					yod_jstring_get(yod_jobject_fetch(x, i, k, l), n)
#define yod_jobject_fetch_int(x, i, k, l) 						yod_jinteger_get(yod_jobject_fetch(x, i, k, l))
#define yod_jobject_fetch_dbl(x, i, k, l) 						yod_jdouble_get(yod_jobject_fetch(x, i, k, l))
#define yod_jobject_fetch_bool(x, i, k, l) 						yod_jboolean_get(yod_jobject_fetch(x, i, k, l))

#define yod_jarray_str(x, i, l) 								yod_jstring_get(yod_jarray_get(x, i), l)
#define yod_jarray_int(x, i) 									yod_jinteger_get(yod_jarray_get(x, i))
#define yod_jarray_dbl(x, i) 									yod_jdouble_get(yod_jarray_get(x, i))
#define yod_jarray_bool(x, i) 									yod_jboolean_get(yod_jarray_get(x, i))


yod_jvalue_t *_yod_jvalue_new(__ENV_PARM);
void _yod_jvalue_free(yod_jvalue_t *self __ENV_CPARM);

int _yod_jvalue_type(yod_jvalue_t *self __ENV_CPARM);
yod_jvalue_t *_yod_jvalue_clone(yod_jvalue_t *self __ENV_CPARM);
yod_jvalue_t *_yod_jvalue_load(const char *file, char *err __ENV_CPARM);
char *_yod_jvalue_dump(yod_jvalue_t *self __ENV_CPARM);

size_t _yod_jvalue_encode(char *data, yod_jvalue_t *self __ENV_CPARM);
yod_jvalue_t *_yod_jvalue_decode(char *data, size_t len, char *err __ENV_CPARM);

/* jobject */
yod_jvalue_t *_yod_jvalue_object_new(size_t size __ENV_CPARM);
int _yod_jvalue_object_set(yod_jvalue_t *self, const char *name, yod_jvalue_t *value __ENV_CPARM);
yod_jvalue_t *_yod_jvalue_object_get(yod_jvalue_t *self, const char *name __ENV_CPARM);
yod_jvalue_t *_yod_jvalue_object_fetch(yod_jvalue_t *self, size_t index, char **name, size_t *name_len __ENV_CPARM);
size_t _yod_jvalue_object_count(yod_jvalue_t *self __ENV_CPARM);
yod_jvalue_t *_yod_jvalue_object_init(size_t size, ...);

/* jarray */
yod_jvalue_t *_yod_jvalue_array_new(size_t size __ENV_CPARM);
int _yod_jvalue_array_add(yod_jvalue_t *self, yod_jvalue_t *value __ENV_CPARM);
int _yod_jvalue_array_set(yod_jvalue_t *self, size_t index, yod_jvalue_t *value __ENV_CPARM);
yod_jvalue_t *_yod_jvalue_array_get(yod_jvalue_t *self, size_t index __ENV_CPARM);
size_t _yod_jvalue_array_count(yod_jvalue_t *self __ENV_CPARM);
yod_jvalue_t *_yod_jvalue_array_init(size_t size, ...);

/* jstring */
yod_jvalue_t *_yod_jvalue_string_new(char *str, size_t len __ENV_CPARM);
int _yod_jvalue_string_set(yod_jvalue_t *self, char *str, size_t len __ENV_CPARM);
char *_yod_jvalue_string_get(yod_jvalue_t *self, size_t *len __ENV_CPARM);

/* jinteger */
yod_jvalue_t *_yod_jvalue_integer_new(int64_t num __ENV_CPARM);
int _yod_jvalue_integer_set(yod_jvalue_t *self, int64_t num __ENV_CPARM);
int64_t _yod_jvalue_integer_get(yod_jvalue_t *self __ENV_CPARM);

/* jdouble */
yod_jvalue_t *_yod_jvalue_double_new(double dval __ENV_CPARM);
int _yod_jvalue_double_set(yod_jvalue_t *self, double dval __ENV_CPARM);
double _yod_jvalue_double_get(yod_jvalue_t *self __ENV_CPARM);

/* jboolean */
yod_jvalue_t *_yod_jvalue_boolean_new(int bval __ENV_CPARM);
int _yod_jvalue_boolean_set(yod_jvalue_t *self, int bval __ENV_CPARM);
int _yod_jvalue_boolean_get(yod_jvalue_t *self __ENV_CPARM);

#endif
