#ifndef __YOD_CRYPTO_H__
#define __YOD_CRYPTO_H__

#include "system.h"


/* MD5 */
byte *yod_crypto_md5enc(byte md5[16], char *data, size_t len);
byte *yod_crypto_md5file(byte md5[16], char *file);
char *yod_crypto_md5str(char buf[33], char *data, size_t len);

/* CRC32 */
uint32_t yod_crypto_crc32(byte *data, size_t len);

#endif
