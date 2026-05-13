/*
 *  RFC 1321 compliant MD5 implementation
 *
 *  Based on XySSL: Copyright (C) 2006-2008  Christophe Devine
 *
 *  Copyright (C) 2009  Paul Bakker <polarssl_maintainer at polarssl dot org>
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the names of PolarSSL or XySSL nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 *  TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 *  The MD5 algorithm was designed by Ron Rivest in 1991.
 *
 *  http://www.ietf.org/rfc/rfc1321.txt
 */

#include <string.h>
#include <cstdio>
#include "md5.h"

#define MD5_CBLOCK	64
#define MD5_LBLOCK	(MD5_CBLOCK/4)

#define DATA_ORDER_IS_LITTLE_ENDIAN

#define	HASH_MAKE_STRING(c,s)	do {	\
	unsigned long long ll;		\
	ll=(c)->A; HOST_l2c(ll,(s));	\
	ll=(c)->B; HOST_l2c(ll,(s));	\
	ll=(c)->C; HOST_l2c(ll,(s));	\
	ll=(c)->D; HOST_l2c(ll,(s));	\
	} while (0)

#define ROTATE(a,n)     (((a)<<(n))|(((a)&0xffffffff)>>(32-(n))))

#if defined(DATA_ORDER_IS_BIG_ENDIAN)
#define HOST_c2l(c,l) \
	(l =(((unsigned long long)(*((c)++)))<<24),		\
	 l|=(((unsigned long long)(*((c)++)))<<16),		\
	 l|=(((unsigned long long)(*((c)++)))<< 8),		\
	 l|=(((unsigned long long)(*((c)++)))    ))

#define HOST_l2c(l,c) \
	(*((c)++)=(unsigned char)(((l)>>24)&0xff),	\
	 *((c)++)=(unsigned char)(((l)>>16)&0xff),	\
	 *((c)++)=(unsigned char)(((l)>> 8)&0xff),	\
	 *((c)++)=(unsigned char)(((l)    )&0xff))
#endif
#if defined(DATA_ORDER_IS_LITTLE_ENDIAN)
#define HOST_c2l(c,l) \
	(l =(((unsigned long long)(*((c)++)))    ),		\
	 l|=(((unsigned long long)(*((c)++)))<< 8),		\
	 l|=(((unsigned long long)(*((c)++)))<<16),		\
	 l|=(((unsigned long long)(*((c)++)))<<24))

#define HOST_l2c(l,c) \
	(*((c)++)=(unsigned char)(((l)    )&0xff),	\
	 *((c)++)=(unsigned char)(((l)>> 8)&0xff),	\
	 *((c)++)=(unsigned char)(((l)>>16)&0xff),	\
	 *((c)++)=(unsigned char)(((l)>>24)&0xff))
#endif

#define MD32_REG_T long

#define	F_(b,c,d)	((((c) ^ (d)) & (b)) ^ (d))
#define	G_(b,c,d)	((((b) ^ (c)) & (d)) ^ (c))
#define	H_(b,c,d)	((b) ^ (c) ^ (d))
#define	I_(b,c,d)	(((~(d)) | (b)) ^ (c))

#define R0(a,b,c,d,k,s,t) { \
	a+=((k)+(t)+F_((b),(c),(d))); \
	a=ROTATE(a,s); \
	a+=b; };
#define R1(a,b,c,d,k,s,t) { \
	a+=((k)+(t)+G_((b),(c),(d))); \
	a=ROTATE(a,s); \
	a+=b; };
#define R2(a,b,c,d,k,s,t) { \
	a+=((k)+(t)+H_((b),(c),(d))); \
	a=ROTATE(a,s); \
	a+=b; };
#define R3(a,b,c,d,k,s,t) { \
	a+=((k)+(t)+I_((b),(c),(d))); \
	a=ROTATE(a,s); \
	a+=b; };


/*
 * 32-bit integer manipulation macros (little endian)
 */
#ifndef GET_ULONG_LE
#define GET_ULONG_LE(n,b,i)                             \
{                                                       \
    (n) = ( (unsigned long) (b)[(i)    ]       )        \
        | ( (unsigned long) (b)[(i) + 1] <<  8 )        \
        | ( (unsigned long) (b)[(i) + 2] << 16 )        \
        | ( (unsigned long) (b)[(i) + 3] << 24 );       \
}
#endif

#ifndef PUT_ULONG_LE
#define PUT_ULONG_LE(n,b,i)                             \
{                                                       \
    (b)[(i)    ] = (unsigned char) ( (n)       );       \
    (b)[(i) + 1] = (unsigned char) ( (n) >>  8 );       \
    (b)[(i) + 2] = (unsigned char) ( (n) >> 16 );       \
    (b)[(i) + 3] = (unsigned char) ( (n) >> 24 );       \
}
#endif

/*
 * MD5 context setup
 */
void md5_starts( md5_context *ctx )
{
    ctx->total[0] = 0;
    ctx->total[1] = 0;

    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
}

static void md5_process( md5_context *ctx, const unsigned char data[64] )
{
    unsigned long X[16], A, B, C, D;

    GET_ULONG_LE( X[ 0], data,  0 );
    GET_ULONG_LE( X[ 1], data,  4 );
    GET_ULONG_LE( X[ 2], data,  8 );
    GET_ULONG_LE( X[ 3], data, 12 );
    GET_ULONG_LE( X[ 4], data, 16 );
    GET_ULONG_LE( X[ 5], data, 20 );
    GET_ULONG_LE( X[ 6], data, 24 );
    GET_ULONG_LE( X[ 7], data, 28 );
    GET_ULONG_LE( X[ 8], data, 32 );
    GET_ULONG_LE( X[ 9], data, 36 );
    GET_ULONG_LE( X[10], data, 40 );
    GET_ULONG_LE( X[11], data, 44 );
    GET_ULONG_LE( X[12], data, 48 );
    GET_ULONG_LE( X[13], data, 52 );
    GET_ULONG_LE( X[14], data, 56 );
    GET_ULONG_LE( X[15], data, 60 );

#define S(x,n) ((x << n) | ((x & 0xFFFFFFFF) >> (32 - n)))

#define P(a,b,c,d,k,s,t)                                \
{                                                       \
    a += F(b,c,d) + X[k] + t; a = S(a,s) + b;           \
}

    A = ctx->state[0];
    B = ctx->state[1];
    C = ctx->state[2];
    D = ctx->state[3];

#define F(x,y,z) (z ^ (x & (y ^ z)))

    P( A, B, C, D,  0,  7, 0xD76AA478 );
    P( D, A, B, C,  1, 12, 0xE8C7B756 );
    P( C, D, A, B,  2, 17, 0x242070DB );
    P( B, C, D, A,  3, 22, 0xC1BDCEEE );
    P( A, B, C, D,  4,  7, 0xF57C0FAF );
    P( D, A, B, C,  5, 12, 0x4787C62A );
    P( C, D, A, B,  6, 17, 0xA8304613 );
    P( B, C, D, A,  7, 22, 0xFD469501 );
    P( A, B, C, D,  8,  7, 0x698098D8 );
    P( D, A, B, C,  9, 12, 0x8B44F7AF );
    P( C, D, A, B, 10, 17, 0xFFFF5BB1 );
    P( B, C, D, A, 11, 22, 0x895CD7BE );
    P( A, B, C, D, 12,  7, 0x6B901122 );
    P( D, A, B, C, 13, 12, 0xFD987193 );
    P( C, D, A, B, 14, 17, 0xA679438E );
    P( B, C, D, A, 15, 22, 0x49B40821 );

#undef F

#define F(x,y,z) (y ^ (z & (x ^ y)))

    P( A, B, C, D,  1,  5, 0xF61E2562 );
    P( D, A, B, C,  6,  9, 0xC040B340 );
    P( C, D, A, B, 11, 14, 0x265E5A51 );
    P( B, C, D, A,  0, 20, 0xE9B6C7AA );
    P( A, B, C, D,  5,  5, 0xD62F105D );
    P( D, A, B, C, 10,  9, 0x02441453 );
    P( C, D, A, B, 15, 14, 0xD8A1E681 );
    P( B, C, D, A,  4, 20, 0xE7D3FBC8 );
    P( A, B, C, D,  9,  5, 0x21E1CDE6 );
    P( D, A, B, C, 14,  9, 0xC33707D6 );
    P( C, D, A, B,  3, 14, 0xF4D50D87 );
    P( B, C, D, A,  8, 20, 0x455A14ED );
    P( A, B, C, D, 13,  5, 0xA9E3E905 );
    P( D, A, B, C,  2,  9, 0xFCEFA3F8 );
    P( C, D, A, B,  7, 14, 0x676F02D9 );
    P( B, C, D, A, 12, 20, 0x8D2A4C8A );

#undef F

#define F(x,y,z) (x ^ y ^ z)

    P( A, B, C, D,  5,  4, 0xFFFA3942 );
    P( D, A, B, C,  8, 11, 0x8771F681 );
    P( C, D, A, B, 11, 16, 0x6D9D6122 );
    P( B, C, D, A, 14, 23, 0xFDE5380C );
    P( A, B, C, D,  1,  4, 0xA4BEEA44 );
    P( D, A, B, C,  4, 11, 0x4BDECFA9 );
    P( C, D, A, B,  7, 16, 0xF6BB4B60 );
    P( B, C, D, A, 10, 23, 0xBEBFBC70 );
    P( A, B, C, D, 13,  4, 0x289B7EC6 );
    P( D, A, B, C,  0, 11, 0xEAA127FA );
    P( C, D, A, B,  3, 16, 0xD4EF3085 );
    P( B, C, D, A,  6, 23, 0x04881D05 );
    P( A, B, C, D,  9,  4, 0xD9D4D039 );
    P( D, A, B, C, 12, 11, 0xE6DB99E5 );
    P( C, D, A, B, 15, 16, 0x1FA27CF8 );
    P( B, C, D, A,  2, 23, 0xC4AC5665 );

#undef F

#define F(x,y,z) (y ^ (x | ~z))

    P( A, B, C, D,  0,  6, 0xF4292244 );
    P( D, A, B, C,  7, 10, 0x432AFF97 );
    P( C, D, A, B, 14, 15, 0xAB9423A7 );
    P( B, C, D, A,  5, 21, 0xFC93A039 );
    P( A, B, C, D, 12,  6, 0x655B59C3 );
    P( D, A, B, C,  3, 10, 0x8F0CCC92 );
    P( C, D, A, B, 10, 15, 0xFFEFF47D );
    P( B, C, D, A,  1, 21, 0x85845DD1 );
    P( A, B, C, D,  8,  6, 0x6FA87E4F );
    P( D, A, B, C, 15, 10, 0xFE2CE6E0 );
    P( C, D, A, B,  6, 15, 0xA3014314 );
    P( B, C, D, A, 13, 21, 0x4E0811A1 );
    P( A, B, C, D,  4,  6, 0xF7537E82 );
    P( D, A, B, C, 11, 10, 0xBD3AF235 );
    P( C, D, A, B,  2, 15, 0x2AD7D2BB );
    P( B, C, D, A,  9, 21, 0xEB86D391 );

#undef F

    ctx->state[0] += A;
    ctx->state[1] += B;
    ctx->state[2] += C;
    ctx->state[3] += D;
}

/* Implemented from RFC1321 The MD5 Message-Digest Algorithm
 */

 #define INIT_DATA_A (unsigned long long)0x67452301L
 #define INIT_DATA_B (unsigned long long)0xefcdab89L
 #define INIT_DATA_C (unsigned long long)0x98badcfeL
 #define INIT_DATA_D (unsigned long long)0x10325476L
 
 int MD5Init (MD5_CTX *c)
 {
     c->A=INIT_DATA_A;
     c->B=INIT_DATA_B;
     c->C=INIT_DATA_C;
     c->D=INIT_DATA_D;
     c->Nl=0;
     c->Nh=0;
     c->num=0;
     return 1;
 }

/*
 * MD5 process buffer
 */
void md5_update( md5_context *ctx, const unsigned char *input, int ilen )
{
    int fill;
    unsigned long left;

    if( ilen <= 0 )
        return;

    left = ctx->total[0] & 0x3F;
    fill = 64 - left;

    ctx->total[0] += ilen;
    ctx->total[0] &= 0xFFFFFFFF;

    if( ctx->total[0] < (unsigned long) ilen )
        ctx->total[1]++;

    if( left && ilen >= fill )
    {
        memcpy( (void *) (ctx->buffer + left),
                input, fill );
        md5_process( ctx, ctx->buffer );
        input += fill;
        ilen  -= fill;
        left = 0;
    }

    while( ilen >= 64 )
    {
        md5_process( ctx, input );
        input += 64;
        ilen  -= 64;
    }

    if( ilen > 0 )
    {
        memcpy( (void *) (ctx->buffer + left),
                input, ilen );
    }
}

static const unsigned char md5_padding[64] =
{
 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*
 * MD5 final digest
 */
void md5_finish( md5_context *ctx, unsigned char output[16] )
{
    unsigned long last, padn;
    unsigned long high, low;
    unsigned char msglen[8];

    high = ( ctx->total[0] >> 29 )
         | ( ctx->total[1] <<  3 );
    low  = ( ctx->total[0] <<  3 );

    PUT_ULONG_LE( low,  msglen, 0 );
    PUT_ULONG_LE( high, msglen, 4 );

    last = ctx->total[0] & 0x3F;
    padn = ( last < 56 ) ? ( 56 - last ) : ( 120 - last );

    md5_update( ctx, md5_padding, padn );
    md5_update( ctx, msglen, 8 );

    PUT_ULONG_LE( ctx->state[0], output,  0 );
    PUT_ULONG_LE( ctx->state[1], output,  4 );
    PUT_ULONG_LE( ctx->state[2], output,  8 );
    PUT_ULONG_LE( ctx->state[3], output, 12 );
}

void md5_block_data_order (MD5_CTX *c, const void *data_, size_t num)
{
	const unsigned char *data=(const unsigned char *)data_;
	register unsigned MD32_REG_T A,B,C,D,l;
#ifndef MD32_XARRAY
	/* See comment in crypto/sha/sha_locl.h for details. */
	unsigned MD32_REG_T	XX0, XX1, XX2, XX3, XX4, XX5, XX6, XX7,
						XX8, XX9,XX10,XX11,XX12,XX13,XX14,XX15;
# define X(i)	XX##i
#else
	unsigned int XX[MD5_LBLOCK];
# define X(i)	XX[i]
#endif

	A=c->A;
	B=c->B;
	C=c->C;
	D=c->D;

	for (;num--;) {
		HOST_c2l(data,l); X( 0)=l;
		HOST_c2l(data,l); X( 1)=l;
		/* Round 0 */
		R0(A,B,C,D,X( 0), 7,0xd76aa478L);	HOST_c2l(data,l); X( 2)=l;
		R0(D,A,B,C,X( 1),12,0xe8c7b756L);	HOST_c2l(data,l); X( 3)=l;
		R0(C,D,A,B,X( 2),17,0x242070dbL);	HOST_c2l(data,l); X( 4)=l;
		R0(B,C,D,A,X( 3),22,0xc1bdceeeL);	HOST_c2l(data,l); X( 5)=l;
		R0(A,B,C,D,X( 4), 7,0xf57c0fafL);	HOST_c2l(data,l); X( 6)=l;
		R0(D,A,B,C,X( 5),12,0x4787c62aL);	HOST_c2l(data,l); X( 7)=l;
		R0(C,D,A,B,X( 6),17,0xa8304613L);	HOST_c2l(data,l); X( 8)=l;
		R0(B,C,D,A,X( 7),22,0xfd469501L);	HOST_c2l(data,l); X( 9)=l;
		R0(A,B,C,D,X( 8), 7,0x698098d8L);	HOST_c2l(data,l); X(10)=l;
		R0(D,A,B,C,X( 9),12,0x8b44f7afL);	HOST_c2l(data,l); X(11)=l;
		R0(C,D,A,B,X(10),17,0xffff5bb1L);	HOST_c2l(data,l); X(12)=l;
		R0(B,C,D,A,X(11),22,0x895cd7beL);	HOST_c2l(data,l); X(13)=l;
		R0(A,B,C,D,X(12), 7,0x6b901122L);	HOST_c2l(data,l); X(14)=l;
		R0(D,A,B,C,X(13),12,0xfd987193L);	HOST_c2l(data,l); X(15)=l;
		R0(C,D,A,B,X(14),17,0xa679438eL);
		R0(B,C,D,A,X(15),22,0x49b40821L);
		/* Round 1 */
		R1(A,B,C,D,X( 1), 5,0xf61e2562L);
		R1(D,A,B,C,X( 6), 9,0xc040b340L);
		R1(C,D,A,B,X(11),14,0x265e5a51L);
		R1(B,C,D,A,X( 0),20,0xe9b6c7aaL);
		R1(A,B,C,D,X( 5), 5,0xd62f105dL);
		R1(D,A,B,C,X(10), 9,0x02441453L);
		R1(C,D,A,B,X(15),14,0xd8a1e681L);
		R1(B,C,D,A,X( 4),20,0xe7d3fbc8L);
		R1(A,B,C,D,X( 9), 5,0x21e1cde6L);
		R1(D,A,B,C,X(14), 9,0xc33707d6L);
		R1(C,D,A,B,X( 3),14,0xf4d50d87L);
		R1(B,C,D,A,X( 8),20,0x455a14edL);
		R1(A,B,C,D,X(13), 5,0xa9e3e905L);
		R1(D,A,B,C,X( 2), 9,0xfcefa3f8L);
		R1(C,D,A,B,X( 7),14,0x676f02d9L);
		R1(B,C,D,A,X(12),20,0x8d2a4c8aL);
		/* Round 2 */
		R2(A,B,C,D,X( 5), 4,0xfffa3942L);
		R2(D,A,B,C,X( 8),11,0x8771f681L);
		R2(C,D,A,B,X(11),16,0x6d9d6122L);
		R2(B,C,D,A,X(14),23,0xfde5380cL);
		R2(A,B,C,D,X( 1), 4,0xa4beea44L);
		R2(D,A,B,C,X( 4),11,0x4bdecfa9L);
		R2(C,D,A,B,X( 7),16,0xf6bb4b60L);
		R2(B,C,D,A,X(10),23,0xbebfbc70L);
		R2(A,B,C,D,X(13), 4,0x289b7ec6L);
		R2(D,A,B,C,X( 0),11,0xeaa127faL);
		R2(C,D,A,B,X( 3),16,0xd4ef3085L);
		R2(B,C,D,A,X( 6),23,0x04881d05L);
		R2(A,B,C,D,X( 9), 4,0xd9d4d039L);
		R2(D,A,B,C,X(12),11,0xe6db99e5L);
		R2(C,D,A,B,X(15),16,0x1fa27cf8L);
		R2(B,C,D,A,X( 2),23,0xc4ac5665L);
		/* Round 3 */
		R3(A,B,C,D,X( 0), 6,0xf4292244L);
		R3(D,A,B,C,X( 7),10,0x432aff97L);
		R3(C,D,A,B,X(14),15,0xab9423a7L);
		R3(B,C,D,A,X( 5),21,0xfc93a039L);
		R3(A,B,C,D,X(12), 6,0x655b59c3L);
		R3(D,A,B,C,X( 3),10,0x8f0ccc92L);
		R3(C,D,A,B,X(10),15,0xffeff47dL);
		R3(B,C,D,A,X( 1),21,0x85845dd1L);
		R3(A,B,C,D,X( 8), 6,0x6fa87e4fL);
		R3(D,A,B,C,X(15),10,0xfe2ce6e0L);
		R3(C,D,A,B,X( 6),15,0xa3014314L);
		R3(B,C,D,A,X(13),21,0x4e0811a1L);
		R3(A,B,C,D,X( 4), 6,0xf7537e82L);
		R3(D,A,B,C,X(11),10,0xbd3af235L);
		R3(C,D,A,B,X( 2),15,0x2ad7d2bbL);
		R3(B,C,D,A,X( 9),21,0xeb86d391L);

		A = c->A += A;
		B = c->B += B;
		C = c->C += C;
		D = c->D += D;
	}
}

int MD5Update (MD5_CTX *c, const void *data_, size_t len)
{
	const unsigned char *data=(const unsigned char *)data_;
	unsigned char *p;
	unsigned int l;
	size_t n;

	if (len==0) return 1;

	l=(c->Nl+(((unsigned int)len)<<3))&0xffffffffUL;
	/* 95-05-24 eay Fixed a bug with the overflow handling, thanks to
	 * Wei Dai <weidai@eskimo.com> for pointing it out. */
	if (l < c->Nl) /* overflow */
		c->Nh++;
	c->Nh+=(len>>29);	/* might cause compiler warning on 16-bit */
	c->Nl=l;

	n = c->num;
	if (n != 0) {
		p=(unsigned char *)c->data;

		if (len >= MD5_CBLOCK || len+n >= MD5_CBLOCK) {
			memcpy (p+n,data,MD5_CBLOCK-n);
			md5_block_data_order (c,p,1);
			n      = MD5_CBLOCK-n;
			data  += n;
			len   -= n;
			c->num = 0;
			memset (p,0,MD5_CBLOCK);	/* keep it zeroed */
		} else {
			memcpy (p+n,data,len);
			c->num += (unsigned int)len;
			return 1;
		}
	}

	n = len/MD5_CBLOCK;
	if (n > 0) {
		md5_block_data_order (c,data,n);
		n    *= MD5_CBLOCK;
		data += n;//lint !e662
		len  -= n;
	}

	if (len != 0) {
		p = (unsigned char *)c->data;
		c->num = len;
		memcpy (p,data,len);
	}
	return 1;
}

int MD5Final (unsigned char* md, MD5_CTX *c)
{
	unsigned char *p = (unsigned char *)c->data;
	size_t n = c->num;

	p[n] = 0x80; /* there is always room for one */
	n++;

	if (n > (MD5_CBLOCK-8)) {
		memset (p+n,0,MD5_CBLOCK-n);
		n=0;
		md5_block_data_order (c,p,1);
	}
	memset (p+n,0,MD5_CBLOCK-8-n);

	p += MD5_CBLOCK-8;
#if   defined(DATA_ORDER_IS_BIG_ENDIAN)
	(void)HOST_l2c(c->Nh,p);
	(void)HOST_l2c(c->Nl,p);
#elif defined(DATA_ORDER_IS_LITTLE_ENDIAN)
	(void)HOST_l2c(c->Nl,p);
	(void)HOST_l2c(c->Nh,p);
#endif
	p -= MD5_CBLOCK;
	md5_block_data_order (c,p,1);
	c->num=0;
	memset (p,0,MD5_CBLOCK);

	HASH_MAKE_STRING(c,md);

	return 1;
}

/*
 * output = MD5( input buffer )
 */
void md5_get( unsigned char *input, int ilen, unsigned char output[16] )
{
    md5_context ctx;

    md5_starts( &ctx );
    md5_update( &ctx, input, ilen );
    md5_finish( &ctx, output );
}

unsigned char* MD5 (const unsigned char* d, size_t n, unsigned char* md)
{
	MD5_CTX c;
	static unsigned char m[MD5_DIGEST_LENGTH];

	if (md == NULL) md=m;
	if (!MD5Init(&c))
		return NULL;
	MD5Update(&c,d,n);
	MD5Final(md,&c);
	return(md);
}

void calculate_md5(const char *filename, unsigned char *md5_result) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Error opening file for MD5 calculation");
        return;
    }

    MD5_CTX md5Context;
    MD5Init(&md5Context);

    unsigned char buffer[1024];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file))) {
        MD5Update(&md5Context, buffer, bytesRead);
    }

    MD5Final(md5_result, &md5Context);
    fclose(file);
}