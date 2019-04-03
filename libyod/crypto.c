#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef WIN32
#include <time.h>
#include <direct.h>
#include <io.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif
#include <errno.h>

#include "system.h"
#include "crypto.h"


/* md5enc_t */
typedef struct
{  
	uint32_t s[4];
	uint32_t c[2];
	byte b[64];
} md5enc_t;


#define MD5ENC_FF(a, b, c, d, x, n, k) \
	{ \
		(a) += (((b) & (c)) | ((~b) & (d))) + (x) + (uint32_t) (k); \
		(a) = (((a) << (n)) | ((a) >> (32 - (n)))); \
		(a) += (b); \
	}
#define MD5ENC_GG(a, b, c, d, x, n, k) \
	{ \
		(a) += (((b) & (d)) | ((c) & (~d))) + (x) + (uint32_t) (k); \
		(a) = (((a) << (n)) | ((a) >> (32 - (n)))); \
		(a) += (b); \
	}
#define MD5ENC_HH(a, b, c, d, x, n, k) \
	{ \
		(a) += ((b) ^ (c) ^ (d)) + (x) + (uint32_t) (k); \
		(a) = (((a) << (n)) | ((a) >> (32 - (n)))); \
		(a) += (b); \
	}
#define MD5ENC_II(a, b, c, d, x, n, k) \
	{ \
		(a) += ((c) ^ ((b) | (~d))) + (x) + (uint32_t) (k); \
		(a) = (((a) << (n)) | ((a) >> (32 - (n)))); \
		(a) += (b); \
	}



/** {{{ static void _yod_crypto_md5enc(md5enc_t *ctx, byte *data, size_t len)
*/
static void _yod_crypto_md5enc(md5enc_t *ctx, byte *data, size_t len)
{
	uint32_t a, b, c, d, x[16];
	size_t i, j, k, n, l;
	byte *temp = NULL;
	int flag = 1;

	n = (size_t)((ctx->c[0] >> 3) & 0x3F);
	if ((ctx->c[0] += ((uint32_t) len << 3)) < ((uint32_t) len << 3)) {
		++ ctx->c[1];
	}
	ctx->c[1] += ((uint32_t) len >> 29);
	l = 64 - n;

	if (len >= l) {
		memcpy((byte *) &ctx->b[n], (byte *) data, l);

		k = l;
		do {
			if (flag > 0) {
				temp = ctx->b;
				flag = 0;
			} else {
				temp = &data[k];
				k += 64;
			}

			a = ctx->s[0];
			b = ctx->s[1];
			c = ctx->s[2];
			d = ctx->s[3];

			for (i = 0, j = 0; j < 64; i++, j += 4) {
				x[i] = ((uint32_t) temp[j]) | (((uint32_t) temp[j + 1]) << 8) |
					(((uint32_t) temp[j + 2]) << 16) | (((uint32_t) temp[j + 3]) << 24);
			}

			MD5ENC_FF(a, b, c, d, x[ 0], 0x07, 0xD76AA478); /* 1 */
			MD5ENC_FF(d, a, b, c, x[ 1], 0x0C, 0xE8C7B756); /* 2 */
			MD5ENC_FF(c, d, a, b, x[ 2], 0x11, 0x242070DB); /* 3 */
			MD5ENC_FF(b, c, d, a, x[ 3], 0x16, 0xC1BDCEEE); /* 4 */
			MD5ENC_FF(a, b, c, d, x[ 4], 0x07, 0xF57C0FAF); /* 5 */
			MD5ENC_FF(d, a, b, c, x[ 5], 0x0C, 0x4787C62A); /* 6 */
			MD5ENC_FF(c, d, a, b, x[ 6], 0x11, 0xA8304613); /* 7 */
			MD5ENC_FF(b, c, d, a, x[ 7], 0x16, 0xFD469501); /* 8 */
			MD5ENC_FF(a, b, c, d, x[ 8], 0x07, 0x698098D8); /* 9 */
			MD5ENC_FF(d, a, b, c, x[ 9], 0x0C, 0x8B44F7AF); /* 10 */
			MD5ENC_FF(c, d, a, b, x[10], 0x11, 0xFFFF5BB1); /* 11 */
			MD5ENC_FF(b, c, d, a, x[11], 0x16, 0x895CD7BE); /* 12 */
			MD5ENC_FF(a, b, c, d, x[12], 0x07, 0x6B901122); /* 13 */
			MD5ENC_FF(d, a, b, c, x[13], 0x0C, 0xFD987193); /* 14 */
			MD5ENC_FF(c, d, a, b, x[14], 0x11, 0xA679438E); /* 15 */
			MD5ENC_FF(b, c, d, a, x[15], 0x16, 0x49B40821); /* 16 */

			MD5ENC_GG(a, b, c, d, x[ 1], 0x05, 0xF61E2562); /* 17 */
			MD5ENC_GG(d, a, b, c, x[ 6], 0x09, 0xC040B340); /* 18 */
			MD5ENC_GG(c, d, a, b, x[11], 0x0E, 0x265E5A51); /* 19 */
			MD5ENC_GG(b, c, d, a, x[ 0], 0x14, 0xE9B6C7AA); /* 20 */
			MD5ENC_GG(a, b, c, d, x[ 5], 0x05, 0xD62F105D); /* 21 */
			MD5ENC_GG(d, a, b, c, x[10], 0x09, 0x02441453); /* 22 */
			MD5ENC_GG(c, d, a, b, x[15], 0x0E, 0xD8A1E681); /* 23 */
			MD5ENC_GG(b, c, d, a, x[ 4], 0x14, 0xE7D3FBC8); /* 24 */
			MD5ENC_GG(a, b, c, d, x[ 9], 0x05, 0x21E1CDE6); /* 25 */
			MD5ENC_GG(d, a, b, c, x[14], 0x09, 0xC33707D6); /* 26 */
			MD5ENC_GG(c, d, a, b, x[ 3], 0x0E, 0xF4D50D87); /* 27 */
			MD5ENC_GG(b, c, d, a, x[ 8], 0x14, 0x455A14ED); /* 28 */
			MD5ENC_GG(a, b, c, d, x[13], 0x05, 0xA9E3E905); /* 29 */
			MD5ENC_GG(d, a, b, c, x[ 2], 0x09, 0xFCEFA3F8); /* 30 */
			MD5ENC_GG(c, d, a, b, x[ 7], 0x0E, 0x676F02D9); /* 31 */
			MD5ENC_GG(b, c, d, a, x[12], 0x14, 0x8D2A4C8A); /* 32 */

			MD5ENC_HH(a, b, c, d, x[ 5], 0x04, 0xFFFA3942); /* 33 */
			MD5ENC_HH(d, a, b, c, x[ 8], 0x0B, 0x8771F681); /* 34 */
			MD5ENC_HH(c, d, a, b, x[11], 0x10, 0x6D9D6122); /* 35 */
			MD5ENC_HH(b, c, d, a, x[14], 0x17, 0xFDE5380C); /* 36 */
			MD5ENC_HH(a, b, c, d, x[ 1], 0x04, 0xA4BEEA44); /* 37 */
			MD5ENC_HH(d, a, b, c, x[ 4], 0x0B, 0x4BDECFA9); /* 38 */
			MD5ENC_HH(c, d, a, b, x[ 7], 0x10, 0xF6BB4B60); /* 39 */
			MD5ENC_HH(b, c, d, a, x[10], 0x17, 0xBEBFBC70); /* 40 */
			MD5ENC_HH(a, b, c, d, x[13], 0x04, 0x289B7EC6); /* 41 */
			MD5ENC_HH(d, a, b, c, x[ 0], 0x0B, 0xEAA127FA); /* 42 */
			MD5ENC_HH(c, d, a, b, x[ 3], 0x10, 0xD4EF3085); /* 43 */
			MD5ENC_HH(b, c, d, a, x[ 6], 0x17, 0x04881D05); /* 44 */
			MD5ENC_HH(a, b, c, d, x[ 9], 0x04, 0xD9D4D039); /* 45 */
			MD5ENC_HH(d, a, b, c, x[12], 0x0B, 0xE6DB99E5); /* 46 */
			MD5ENC_HH(c, d, a, b, x[15], 0x10, 0x1FA27CF8); /* 47 */
			MD5ENC_HH(b, c, d, a, x[ 2], 0x17, 0xC4AC5665); /* 48 */

			MD5ENC_II(a, b, c, d, x[ 0], 0x06, 0xF4292244); /* 49 */
			MD5ENC_II(d, a, b, c, x[ 7], 0x0A, 0x432AFF97); /* 50 */
			MD5ENC_II(c, d, a, b, x[14], 0x0F, 0xAB9423A7); /* 51 */
			MD5ENC_II(b, c, d, a, x[ 5], 0x15, 0xFC93A039); /* 52 */
			MD5ENC_II(a, b, c, d, x[12], 0x06, 0x655B59C3); /* 53 */
			MD5ENC_II(d, a, b, c, x[ 3], 0x0A, 0x8F0CCC92); /* 54 */
			MD5ENC_II(c, d, a, b, x[10], 0x0F, 0xFFEFF47D); /* 55 */
			MD5ENC_II(b, c, d, a, x[ 1], 0x15, 0x85845DD1); /* 56 */
			MD5ENC_II(a, b, c, d, x[ 8], 0x06, 0x6FA87E4F); /* 57 */
			MD5ENC_II(d, a, b, c, x[15], 0x0A, 0xFE2CE6E0); /* 58 */
			MD5ENC_II(c, d, a, b, x[ 6], 0x0F, 0xA3014314); /* 59 */
			MD5ENC_II(b, c, d, a, x[13], 0x15, 0x4E0811A1); /* 60 */
			MD5ENC_II(a, b, c, d, x[ 4], 0x06, 0xF7537E82); /* 61 */
			MD5ENC_II(d, a, b, c, x[11], 0x0A, 0xBD3AF235); /* 62 */
			MD5ENC_II(c, d, a, b, x[ 2], 0x0F, 0x2AD7D2BB); /* 63 */
			MD5ENC_II(b, c, d, a, x[ 9], 0x15, 0xEB86D391); /* 64 */

			ctx->s[0] += a;
			ctx->s[1] += b;
			ctx->s[2] += c;
			ctx->s[3] += d;

			memset ((byte *) x, 0, sizeof(x));
		
		} while (k + 63 < len);

		n = 0;
	}
	else {
		k = 0;
	}

	memcpy((byte *) &ctx->b[n], (byte *) &data[k], len - k);
}
/* }}} */


/** {{{ byte *yod_crypto_md5enc(byte md5[16], char *data, size_t len)
*/
byte *yod_crypto_md5enc(byte md5[16], char *data, size_t len)
{
	static byte pad[64] = {0x80};
	size_t i, j, n, l;
	md5enc_t ctx;

	if (!md5 || !data || !len) {
		return NULL;
	}

	ctx.c[0] = ctx.c[1] = 0;
	ctx.s[0] = 0x67452301;
	ctx.s[1] = 0xEFCDAB89;
	ctx.s[2] = 0x98BADCFE;
	ctx.s[3] = 0x10325476;

	_yod_crypto_md5enc(&ctx, (byte *) data, len);

	for (i = 0, j = 0; j < 8; i++, j += 4) {  
		md5[j] = (byte) (ctx.c[i] & 0x0FF);  
		md5[j + 1] = (byte) ((ctx.c[i] >> 8) & 0x0FF);  
		md5[j + 2] = (byte) ((ctx.c[i] >> 16) & 0x0FF);  
		md5[j + 3] = (byte) ((ctx.c[i] >> 24) & 0x0FF);  
	}

	n = (size_t) ((ctx.c[0] >> 3) & 0x03F);
	l = (n < 56) ? (56 - n) : (120 - n);
	_yod_crypto_md5enc(&ctx, pad, l);
	_yod_crypto_md5enc(&ctx, md5, 8);

	for (i = 0, j = 0; j < 16; i++, j += 4) {  
		md5[j] = (byte) (ctx.s[i] & 0x0FF);  
		md5[j + 1] = (byte) ((ctx.s[i] >> 8) & 0x0FF);  
		md5[j + 2] = (byte) ((ctx.s[i] >> 16) & 0x0FF);  
		md5[j + 3] = (byte) ((ctx.s[i] >> 24) & 0x0FF);  
	}

	memset((byte *) &ctx, 0, sizeof(ctx));

	return md5;
}
/* }}} */


/** {{{ char *yod_crypto_md5str(char buf[33], char *data, size_t len)
*/
char *yod_crypto_md5str(char buf[33], char *data, size_t len)
{
	byte md5[16];
	int i = 0;

	if (!buf || !data) {
		return NULL;
	}

	if (!yod_crypto_md5enc(md5, data, len)) {
		return NULL;
	}

	while (i < 16) {
		buf[i * 2] = yod_common_byte2hex(md5[i] >> 4, 0);
		buf[i * 2 + 1] = yod_common_byte2hex(md5[i] & 0x0F, 0);
		buf[i * 2 + 2] = 0;
		++ i;
	}

	return buf;
}
/* }}} */


/** {{{ byte *yod_crypto_md5file(byte md5[16], char *file)
*/
byte *yod_crypto_md5file(byte md5[16], char *file)
{
	static byte pad[64] = {0x80};
	size_t i, j, n, l;
	FILE *fp = NULL;
	byte data[1024];
	size_t len = 0;
	md5enc_t ctx;

	if (!md5 || !file) {
		return NULL;
	}

#ifdef _WIN32
	if (fopen_s(&fp, file, "rb") != 0)
#else
	if ((fp = fopen(file, "rb")) == NULL)
#endif
	{
		return NULL;
	}

	ctx.c[0] = ctx.c[1] = 0;
	ctx.s[0] = 0x67452301;
	ctx.s[1] = 0xEFCDAB89;
	ctx.s[2] = 0x98BADCFE;
	ctx.s[3] = 0x10325476;

	while ((len = fread(data, 1, 1024, fp)) > 0) {
		_yod_crypto_md5enc(&ctx, data, (size_t) len);
	}
	fclose(fp);

	for (i = 0, j = 0; j < 8; i++, j += 4) {  
		md5[j] = (byte) (ctx.c[i] & 0x0FF);  
		md5[j + 1] = (byte) ((ctx.c[i] >> 8) & 0x0FF);  
		md5[j + 2] = (byte) ((ctx.c[i] >> 16) & 0x0FF);  
		md5[j + 3] = (byte) ((ctx.c[i] >> 24) & 0x0FF);  
	}

	n = (size_t) ((ctx.c[0] >> 3) & 0x03F);
	l = (n < 56) ? (56 - n) : (120 - n);
	_yod_crypto_md5enc(&ctx, pad, l);
	_yod_crypto_md5enc(&ctx, md5, 8);

	for (i = 0, j = 0; j < 16; i++, j += 4) {  
		md5[j] = (byte) (ctx.s[i] & 0x0FF);  
		md5[j + 1] = (byte) ((ctx.s[i] >> 8) & 0x0FF);  
		md5[j + 2] = (byte) ((ctx.s[i] >> 16) & 0x0FF);  
		md5[j + 3] = (byte) ((ctx.s[i] >> 24) & 0x0FF);  
	}

	memset((byte *) &ctx, 0, sizeof(ctx));

	return md5;
}
/* }}} */


/** {{{ uint32_t yod_crypto_crc32(byte *data, size_t len)
*/
uint32_t yod_crypto_crc32(byte *data, size_t len)
{
	static const unsigned long tbl[256] = {
		0x00000000L, 0x77073096L, 0xEE0E612CL, 0x990951BAL, 0x076DC419L, 0x706AF48FL, 0xE963A535L, 0x9E6495A3L,
		0x0EDB8832L, 0x79DCB8A4L, 0xE0D5E91EL, 0x97D2D988L, 0x09B64C2BL, 0x7EB17CBDL, 0xE7B82D07L, 0x90BF1D91L,
		0x1DB71064L, 0x6AB020F2L, 0xF3B97148L, 0x84BE41DEL, 0x1ADAD47DL, 0x6DDDE4EBL, 0xF4D4B551L, 0x83D385C7L,
		0x136C9856L, 0x646BA8C0L, 0xFD62F97AL, 0x8A65C9ECL, 0x14015C4FL, 0x63066CD9L, 0xFA0F3D63L, 0x8D080DF5L,
		0x3B6E20C8L, 0x4C69105EL, 0xD56041E4L, 0xA2677172L, 0x3C03E4D1L, 0x4B04D447L, 0xD20D85FDL, 0xA50AB56BL,
		0x35B5A8FAL, 0x42B2986CL, 0xDBBBC9D6L, 0xACBCF940L, 0x32D86CE3L, 0x45DF5C75L, 0xDCD60DCFL, 0xABD13D59L,
		0x26D930ACL, 0x51DE003AL, 0xC8D75180L, 0xBFD06116L, 0x21B4F4B5L, 0x56B3C423L, 0xCFBA9599L, 0xB8BDA50FL,
		0x2802B89EL, 0x5F058808L, 0xC60CD9B2L, 0xB10BE924L, 0x2F6F7C87L, 0x58684C11L, 0xC1611DABL, 0xB6662D3DL,
		0x76DC4190L, 0x01DB7106L, 0x98D220BCL, 0xEFD5102AL, 0x71B18589L, 0x06B6B51FL, 0x9FBFE4A5L, 0xE8B8D433L,
		0x7807C9A2L, 0x0F00F934L, 0x9609A88EL, 0xE10E9818L, 0x7F6A0DBBL, 0x086D3D2DL, 0x91646C97L, 0xE6635C01L,
		0x6B6B51F4L, 0x1C6C6162L, 0x856530D8L, 0xF262004EL, 0x6C0695EDL, 0x1B01A57BL, 0x8208F4C1L, 0xF50FC457L,
		0x65B0D9C6L, 0x12B7E950L, 0x8BBEB8EAL, 0xFCB9887CL, 0x62DD1DDFL, 0x15DA2D49L, 0x8CD37CF3L, 0xFBD44C65L,
		0x4DB26158L, 0x3AB551CEL, 0xA3BC0074L, 0xD4BB30E2L, 0x4ADFA541L, 0x3DD895D7L, 0xA4D1C46DL, 0xD3D6F4FBL,
		0x4369E96AL, 0x346ED9FCL, 0xAD678846L, 0xDA60B8D0L, 0x44042D73L, 0x33031DE5L, 0xAA0A4C5FL, 0xDD0D7CC9L,
		0x5005713CL, 0x270241AAL, 0xBE0B1010L, 0xC90C2086L, 0x5768B525L, 0x206F85B3L, 0xB966D409L, 0xCE61E49FL,
		0x5EDEF90EL, 0x29D9C998L, 0xB0D09822L, 0xC7D7A8B4L, 0x59B33D17L, 0x2EB40D81L, 0xB7BD5C3BL, 0xC0BA6CADL,
		0xEDB88320L, 0x9ABFB3B6L, 0x03B6E20CL, 0x74B1D29AL, 0xEAD54739L, 0x9DD277AFL, 0x04DB2615L, 0x73DC1683L,
		0xE3630B12L, 0x94643B84L, 0x0D6D6A3EL, 0x7A6A5AA8L, 0xE40ECF0BL, 0x9309FF9DL, 0x0A00AE27L, 0x7D079EB1L,
		0xF00F9344L, 0x8708A3D2L, 0x1E01F268L, 0x6906C2FEL, 0xF762575DL, 0x806567CBL, 0x196C3671L, 0x6E6B06E7L,
		0xFED41B76L, 0x89D32BE0L, 0x10DA7A5AL, 0x67DD4ACCL, 0xF9B9DF6FL, 0x8EBEEFF9L, 0x17B7BE43L, 0x60B08ED5L,
		0xD6D6A3E8L, 0xA1D1937EL, 0x38D8C2C4L, 0x4FDFF252L, 0xD1BB67F1L, 0xA6BC5767L, 0x3FB506DDL, 0x48B2364BL,
		0xD80D2BDAL, 0xAF0A1B4CL, 0x36034AF6L, 0x41047A60L, 0xDF60EFC3L, 0xA867DF55L, 0x316E8EEFL, 0x4669BE79L,
		0xCB61B38CL, 0xBC66831AL, 0x256FD2A0L, 0x5268E236L, 0xCC0C7795L, 0xBB0B4703L, 0x220216B9L, 0x5505262FL,
		0xC5BA3BBEL, 0xB2BD0B28L, 0x2BB45A92L, 0x5CB36A04L, 0xC2D7FFA7L, 0xB5D0CF31L, 0x2CD99E8BL, 0x5BDEAE1DL,
		0x9B64C2B0L, 0xEC63F226L, 0x756AA39CL, 0x026D930AL, 0x9C0906A9L, 0xEB0E363FL, 0x72076785L, 0x05005713L,
		0x95BF4A82L, 0xE2B87A14L, 0x7BB12BAEL, 0x0CB61B38L, 0x92D28E9BL, 0xE5D5BE0DL, 0x7CDCEFB7L, 0x0BDBDF21L,
		0x86D3D2D4L, 0xF1D4E242L, 0x68DDB3F8L, 0x1FDA836EL, 0x81BE16CDL, 0xF6B9265BL, 0x6FB077E1L, 0x18B74777L,
		0x88085AE6L, 0xFF0F6A70L, 0x66063BCAL, 0x11010B5CL, 0x8F659EFFL, 0xF862AE69L, 0x616BFFD3L, 0x166CCF45L,
		0xA00AE278L, 0xD70DD2EEL, 0x4E048354L, 0x3903B3C2L, 0xA7672661L, 0xD06016F7L, 0x4969474DL, 0x3E6E77DBL,
		0xAED16A4AL, 0xD9D65ADCL, 0x40DF0B66L, 0x37D83BF0L, 0xA9BCAE53L, 0xDEBB9EC5L, 0x47B2CF7FL, 0x30B5FFE9L,
		0xBDBDF21CL, 0xCABAC28AL, 0x53B39330L, 0x24B4A3A6L, 0xBAD03605L, 0xCDD70693L, 0x54DE5729L, 0x23D967BFL,
		0xB3667A2EL, 0xC4614AB8L, 0x5D681B02L, 0x2A6F2B94L, 0xB40BBE37L, 0xC30C8EA1L, 0x5A05DF1BL, 0x2D02EF8DL
	};

	uint32_t ret = 0;
	size_t i = 0;

	ret = ~ret;

	while (i < len) {
		ret = (ret >> 8) ^ tbl[(ret ^ data[i++]) & 0x0FF];
	}

    return (~ret);
}
/* }}} */
