#ifndef MD5_H
#define MD5_H

#define MD5_DIGEST_LENGTH 16

/**
 * \brief          MD5 context structure
 */
typedef struct
{
    unsigned long long total[2];     /*!< number of bytes processed  */
    unsigned long long state[4];     /*!< intermediate digest state  */
    unsigned char buffer[64];   /*!< data block being processed */
}
md5_context;

typedef struct MD5state_st {
    unsigned int A,B,C,D;
    unsigned int Nl,Nh;
    unsigned int data[16];
    unsigned int num;
} MD5_CTX;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief          MD5 context setup
 *
 * \param ctx      context to be initialized
 */
void md5_starts( md5_context *ctx );

/**
 * \brief          MD5 process buffer
 *
 * \param ctx      MD5 context
 * \param input    buffer holding the  data
 * \param ilen     length of the input data
 */
void md5_update( md5_context *ctx, const unsigned char *input, int ilen );

/**
 * \brief          MD5 final digest
 *
 * \param ctx      MD5 context
 * \param output   MD5 checksum result
 */
void md5_finish( md5_context *ctx, unsigned char output[16] );

/**
 * \brief          Output = MD5( input buffer )
 *
 * \param input    buffer holding the  data
 * \param ilen     length of the input data
 * \param output   MD5 checksum result
 */
void md5_get( unsigned char *input, int ilen, unsigned char output[16] );
void calculate_md5(const char *filename, unsigned char *md5_result);

int MD5Init(MD5_CTX *c);
int MD5Update(MD5_CTX *c, const void *data, size_t len);
int MD5Final(unsigned char *md, MD5_CTX *c);
unsigned char *MD5(const unsigned char *d, size_t n, unsigned char *md);

#ifdef __cplusplus
}
#endif
#endif
