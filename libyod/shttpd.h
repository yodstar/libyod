#ifndef __YOD_SHTTPD_H__
#define __YOD_SHTTPD_H__


#define YOD_SHTTPD_VERSION 										"1.0.0"
#define YOD_SHTTPD_VERSION_NUMBER 								0x01000000

#define YOD_SHTTPD_HTTP_1_0 									0x0100
#define YOD_SHTTPD_HTTP_1_1 									0x0101

/* MIME */
#define YOD_SHTTPD_MIME_BMP 									"image/bmp"
#define YOD_SHTTPD_MIME_GIF 									"image/gif"
#define YOD_SHTTPD_MIME_JPEG 									"image/jpeg"
#define YOD_SHTTPD_MIME_PNG 									"image/png"
#define YOD_SHTTPD_MIME_ICO 									"image/x-icon"
#define YOD_SHTTPD_MIME_CSS 									"text/css"
#define YOD_SHTTPD_MIME_HTML 									"text/html"
#define YOD_SHTTPD_MIME_JS 										"application/x-javascript"
#define YOD_SHTTPD_MIME_JSON 									"application/json"
#define YOD_SHTTPD_MIME_TXT 									"text/plain"
#define YOD_SHTTPD_MIME_XML 									"text/xml"
#define YOD_SHTTPD_MIME_ZIP 									"application/zip"
#define YOD_SHTTPD_MIME_XXX 									"application/octet-stream"


enum
{
	YOD_SHTTPD_METHOD_UNKNOWN,
	YOD_SHTTPD_METHOD_OPTIONS,
	YOD_SHTTPD_METHOD_GET,
	YOD_SHTTPD_METHOD_HEAD,
	YOD_SHTTPD_METHOD_POST,
	YOD_SHTTPD_METHOD_PUT,
	YOD_SHTTPD_METHOD_DELETE,
	YOD_SHTTPD_METHOD_TRACE,
	YOD_SHTTPD_METHOD_CONNECT
};


/* yod_shttpd_t */
typedef struct _yod_shttpd_t 									yod_shttpd_t;


/* yod_shttpd_r */
typedef struct _yod_shttpd_r
{
	short method;
	short version;
	yod_string_t host;
	yod_string_t path;
	yod_string_t qstr;
	short keep_alive;
	yod_string_t user_agent;
	yod_string_t referer;
	yod_string_t cookie;

	yod_string_t headers;
	yod_string_t outfile;
	yod_string_t content;
	char *mime_type;
	short status;

} yod_shttpd_r;


/* yod_shttpd_fn */
typedef void (*yod_shttpd_fn) (yod_shttpd_r *hreq __ENV_CPARM);


#define yod_shttpd_new(s, l, h) 								_yod_shttpd_new(s, l, h __ENV_CARGS)
#define yod_shttpd_free(x) 										_yod_shttpd_free(x __ENV_CARGS)

#define yod_shttpd_route(x, r, f) 								_yod_shttpd_route(x, r, f __ENV_CARGS)


yod_shttpd_t *_yod_shttpd_new(yod_server_t *server, const char *listen, const char *htdocs __ENV_CPARM);
void _yod_shttpd_free(yod_shttpd_t *self __ENV_CPARM);

int _yod_shttpd_route(yod_shttpd_t *self, const char *route, yod_shttpd_fn func __ENV_CPARM);

#endif
