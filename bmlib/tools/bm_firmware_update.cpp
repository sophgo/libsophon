#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "bmlib_runtime.h"
#include "bmlib_internal.h"
#include "gflags/gflags.h"
#ifdef __linux__
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#else
#undef false
#undef true
#endif

#ifndef USING_CMODEL

#define FLASH_SIZE        (64 * 1024)
#define FLASH_SIZE_SC7P   (80 * 1024)
#define PROGRAM_LIMIT        (FLASH_SIZE - 128)
#define PROGRAM_LIMIT_SC7P   (FLASH_SIZE_SC7P - 128)

#define LOADER_START        0
#define LOADER_SIZE        (28 * 1024)
#define LOADER_SIZE_SC7P   (32 * 1024)
#define LOADER_END        (LOADER_START + LOADER_SIZE)
#define LOADER_END_SC7P   (LOADER_START + LOADER_SIZE_SC7P)

#define EFIT_START        LOADER_END
#define EFIT_START_SC7P   LOADER_END_SC7P
#define EFIT_SIZE        (4 * 1024)
#define EFIT_SIZE_SC7P   (8 * 1024)
#define EFIT_END        (EFIT_START + EFIT_SIZE)
#define EFIT_END_SIZE   (EFIT_START_SC7P + EFIT_SIZE_SC7P)
#define EFIE_SIZE        (128)

/*  type id
EVB	0
SA5	1
SC5	2
SE5	3
SM5P	4
SM5S	5
SA6	6

SC5PLUS	7
SC5H	8
SC5PRO	9
SM5ME	10

SM5MP	11
SM5MS	12
SM5MA	13
*/

#define EVB "EVB"
#define SA5 "SA5"
#define SC5 "SC5"
#define SE5 "SE5"
#define SM5P "SM5_P"
#define SM5S "SM5_S"
#define SM5W "SM5_W"
#define SA6 "SA6"

#define SC5PLUS "SC5+"
#define SC5H "SC5H"
#define SC5PRO "SC5P"
#define SM5ME "SM5M"

#define SM5MP "SM5M"
#define SM5MS "SM5M"
#define SM5MA "SM5M"
#define BM1684X_EVB "BM1684X_EVB"
#define SC7P "SC7P"
#define SC7PLUS "SC7+"

DEFINE_int32(dev, 0, "device id");
DEFINE_string(file, "", "bin file with pathname");
DEFINE_string(target, "", "target of the bin file to program; a53/mcu");
DEFINE_bool(full, false, "program full mcu firmware or only app");

typedef struct bin_buffer {
  unsigned char *buf;
  unsigned int size;
  unsigned int target_addr;
}Bin_buffer;

#pragma pack(push, 1)
struct efie {
    u32    offset;
    u32    length;
    u8    checksum[16];
    u8    reserved[104];
};
#pragma pack(pop)

struct {
    char const *name;
    int id[2];
    char const *type[6];
} firmware_table[] = {
    {"EVB", {0, -1}, {EVB, SC5, "Error"}},
    {"SA5", {1, -1}, {SA5, SE5, SM5P, SM5S, SM5W, "Error"}},
    {"SC5PLUS", {7, -1}, {SC5PLUS, "Error"}},
    {"SC5H", {8, -1}, {SC5H, "Error"}},
    {"SC5PRO", {9, -1}, {SC5PRO, "Error"}},
    {"SM5MINI", {10, -1}, {SM5ME, SM5MP, SM5MS, SM5MA, "Error"}},
    {"BM1684X_EVB", {32, -1}, {EVB, "Error"}},
    {"SC7P", {33, -1}, {SC7P, "Error"}},
    {"SC7+", {34, -1}, {SC7PLUS, "Error"}}
};

#pragma pack(push, 1)
struct fwinfo {
    uint8_t magic[4];
    uint8_t type;
    uint8_t r0[3];
    uint32_t timestamp;
};
#pragma pack(pop)

static int bm_load_bin_buffer(const char *fn, Bin_buffer *b_buf) {
  FILE *fp;

  fp = fopen(fn, "rb");
  if (!fp) {
    printf("open file %s failed\n", fn);
    return -1;
  }
  fseek(fp, 0, SEEK_END);
  b_buf->size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  printf("bin file size %d\n", b_buf->size);

  if (b_buf->size) {
    b_buf->buf = (unsigned char *)malloc(b_buf->size);
    if (!b_buf->buf) {
      printf("mfulloc buffer failed\n");
      return -1;
    }
    if (fread(b_buf->buf, 1, b_buf->size, fp) != (size_t)b_buf->size) {
      printf("read file length %d failed\n", b_buf->size);
      return -1;
    }
  } else {
    printf("bin file size %d is incorrect!\n", b_buf->size);
    fclose(fp);
    return -1;
  }
  fclose(fp);
  return 0;
}

static int bm_release_bin_buffer(Bin_buffer *b_buf) {
  free(b_buf->buf);
  return 0;
}

static bm_status_t bm_update_fw(bm_handle_t handle, Bin_buffer *itcm_buf, Bin_buffer *ddr_buf) {
  bm_status_t ret;
  bm_fw_desc fw_desc;

  fw_desc.itcm_fw = (unsigned int *)itcm_buf->buf;
  fw_desc.itcmfw_size = itcm_buf->size;
  fw_desc.ddr_fw = (unsigned int *)ddr_buf->buf;
  fw_desc.ddrfw_size = ddr_buf->size;

  ret = bm_update_firmware_a9(handle, &fw_desc);
  if (ret != BM_SUCCESS) {
    printf("upgrade firmware failed ret = %d\n", ret);
  }
  printf("update firmware successfully.\n");
  return ret;
}

#ifdef __linux__
static void * print_wait(void *) {
  printf("programming SPI flash ...\n");
  while (1) {
    printf(".");
    fflush(stdout);
    usleep(1000000);
  }
  return NULL;
}
#endif

bm_status_t bm1684_firmware_update_a53(bm_handle_t handle, Bin_buffer *bin_buf) {
  printf("Programming A53 firmware...\n");

  bin_buf->target_addr = 0;
  #ifdef __linux
  if (0 == ioctl(handle->dev_fd, BMDEV_PROGRAM_A53, bin_buf)) {
  #else
  if (0 == platform_ioctl(handle, BMDEV_PROGRAM_A53, bin_buf)) {
  #endif
    return BM_SUCCESS;
  } else {
    return BM_ERR_FAILURE;
  }
}

void checksum(void *out, void *in, unsigned long len) {
  unsigned int *src = (unsigned int *)in;
  const char *init = "*BITMAIN-SOPHON*";
  unsigned int result[4];

  memcpy(result, init, sizeof(result));
  unsigned long block = len >> 4;
  unsigned long left = len & 0xf;
  unsigned long i, j;
  for (i = 0; i < block; ++i, src += 4) {
    for (j = 0; j < 4; ++j) {
      result[j] ^= src[j];
    }
  }
  for (i = 0; i < left; ++i) {
    ((uint8_t *)result)[i] ^= ((uint8_t *)src)[i];
  }
  memcpy(out, result, sizeof(result));
}

/*********************MD5 Definition Start ***********************/

/* POINTER defines a generic pointer type */
typedef unsigned char *POINTER;

/* UINT2 defines a two byte word */
// typedef unsigned short int UINT2;
typedef uint16_t UINT2;

/* UINT4 defines a four byte word */
// typedef unsigned long int UINT4;
typedef uint32_t UINT4;

/* MD5 context. */
typedef struct {
  UINT4 state[4];                                   /* state (ABCD) */
  UINT4 count[2];        /* number of bits, modulo 2^64 (lsb first) */
  unsigned char buffer[64];                         /* input buffer */
} MD5_CTX;

/* Constants for MD5Transform routine.
*/

#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

static void MD5Transform(UINT4[4], unsigned char[64]);
static void Encode(unsigned char *, UINT4 *, unsigned int);
static void Decode(UINT4 *, unsigned char *, unsigned int);
static void MD5_memcpy(POINTER, POINTER, unsigned int);
static void MD5_memset(POINTER, int, unsigned int);

static unsigned char PADDING[64] = {
  0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* F, G, H and I are basic MD5 functions.
*/
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left n bits.
*/
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
   Rotation is separate from addition to prevent recomputation.
   */
#define FF(a, b, c, d, x, s, ac) { \
  (a) += F ((b), (c), (d)) + (x) + (UINT4)(ac); \
  (a) = ROTATE_LEFT((a), (s)); \
  (a) += (b); \
}
#define GG(a, b, c, d, x, s, ac) { \
  (a) += G ((b), (c), (d)) + (x) + (UINT4)(ac); \
  (a) = ROTATE_LEFT((a), (s)); \
  (a) += (b); \
}
#define HH(a, b, c, d, x, s, ac) { \
  (a) += H ((b), (c), (d)) + (x) + (UINT4)(ac); \
  (a) = ROTATE_LEFT((a), (s)); \
  (a) += (b); \
}
#define II(a, b, c, d, x, s, ac) { \
  (a) += I ((b), (c), (d)) + (x) + (UINT4)(ac); \
  (a) = ROTATE_LEFT((a), (s)); \
  (a) += (b); \
}

/* MD5 initialization. Begins an MD5 operation, writing a new context.
*/
void MD5Init(MD5_CTX *context) {                                        /* context */
  context->count[0] = context->count[1] = 0;
  /* Load magic initialization constants.
  */
  context->state[0] = 0x67452301;
  context->state[1] = 0xefcdab89;
  context->state[2] = 0x98badcfe;
  context->state[3] = 0x10325476;
}

/* MD5 block update operation. Continues an MD5 message-digest
   operation, processing another message block, and updating the
   context.
   */
void MD5Update(MD5_CTX *context, unsigned char *input, unsigned int inputLen) {
  unsigned int i, index, partLen;

  /* Compute number of bytes mod 64 */
  index = (unsigned int)((context->count[0] >> 3) & 0x3F);

  /* Update number of bits */
  if ((context->count[0] += ((UINT4)inputLen << 3))
      < ((UINT4)inputLen << 3))
    context->count[1]++;
  context->count[1] += ((UINT4)inputLen >> 29);

  partLen = 64 - index;

  /* Transform as many times as possible.
  */
  if (inputLen >= partLen) {
    MD5_memcpy
      ((POINTER)&context->buffer[index], (POINTER)input, partLen);
    MD5Transform(context->state, context->buffer);

    for (i = partLen; i + 63 < inputLen; i += 64)
      MD5Transform(context->state, &input[i]);

    index = 0;
  } else {
    i = 0;
  }

  /* Buffer remaining input */
  MD5_memcpy
    ((POINTER)&context->buffer[index], (POINTER)&input[i],
     inputLen-i);
}

/* MD5 finalization. Ends an MD5 message-digest operation, writing the
   the message digest and zeroizing the context.
   */
void MD5Final(unsigned char digest[16], MD5_CTX *context) {
  unsigned char bits[8];
  unsigned int index, padLen;

  /* Save number of bits */
  Encode(bits, context->count, 8);

  /* Pad out to 56 mod 64.
  */
  index = (unsigned int)((context->count[0] >> 3) & 0x3f);
  padLen = (index < 56) ? (56 - index) : (120 - index);
  MD5Update(context, PADDING, padLen);

  /* Append length (before padding) */
  MD5Update(context, bits, 8);
  /* Store state in digest */
  Encode(digest, context->state, 16);

  /* Zeroize sensitive information.
  */
  MD5_memset((POINTER)context, 0, sizeof (*context));
}

/* MD5 basic transformation. Transforms state based on block.
*/
static void MD5Transform(UINT4 state[4], unsigned char block[64]) {
  UINT4 a = state[0], b = state[1], c = state[2], d = state[3], x[16];

  Decode(x, block, 64);

  /* Round 1 */
  FF(a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
  FF(d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
  FF(c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
  FF(b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
  FF(a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
  FF(d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
  FF(c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
  FF(b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
  FF(a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
  FF(d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
  FF(c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
  FF(b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
  FF(a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
  FF(d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
  FF(c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
  FF(b, c, d, a, x[15], S14, 0x49b40821); /* 16 */

  /* Round 2 */
  GG(a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
  GG(d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
  GG(c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
  GG(b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
  GG(a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
  GG(d, a, b, c, x[10], S22,  0x2441453); /* 22 */
  GG(c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
  GG(b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
  GG(a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
  GG(d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
  GG(c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
  GG(b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
  GG(a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
  GG(d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
  GG(c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
  GG(b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */

  /* Round 3 */
  HH(a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
  HH(d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
  HH(c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
  HH(b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
  HH(a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
  HH(d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
  HH(c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
  HH(b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
  HH(a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
  HH(d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
  HH(c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
  HH(b, c, d, a, x[ 6], S34,  0x4881d05); /* 44 */
  HH(a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
  HH(d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
  HH(c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
  HH(b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */

  /* Round 4 */
  II(a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
  II(d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
  II(c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
  II(b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
  II(a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
  II(d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
  II(c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
  II(b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
  II(a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
  II(d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
  II(c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
  II(b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
  II(a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
  II(d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
  II(c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
  II(b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;

  /* Zeroize sensitive information.
  */
  MD5_memset((POINTER)x, 0, sizeof (x));
}

/* Encodes input (UINT4) into output (unsigned char). Assumes len is
   a multiple of 4.
   */
static void Encode(unsigned char *output, UINT4 *input, unsigned int len) {
  unsigned int i, j;

  for (i = 0, j = 0; j < len; i++, j += 4) {
    output[j] = (unsigned char)(input[i] & 0xff);
    output[j+1] = (unsigned char)((input[i] >> 8) & 0xff);
    output[j+2] = (unsigned char)((input[i] >> 16) & 0xff);
    output[j+3] = (unsigned char)((input[i] >> 24) & 0xff);
  }
}

/* Decodes input (unsigned char) into output (UINT4). Assumes len is
   a multiple of 4.
   */
static void Decode(UINT4 *output, unsigned char *input, unsigned int len) {
  unsigned int i, j;

  for (i = 0, j = 0; j < len; i++, j += 4)
    output[i] = ((UINT4)input[j]) | (((UINT4)input[j+1]) << 8) |
      (((UINT4)input[j+2]) << 16) | (((UINT4)input[j+3]) << 24);
}

/* Note: Replace "for loop" with standard memcpy if possible.
*/

static void MD5_memcpy(POINTER output, POINTER input, unsigned int len) {
  unsigned int i;

  for (i = 0; i < len; i++)
    output[i] = input[i];
}

/* Note: Replace "for loop" with standard memset if possible.
*/
static void MD5_memset(POINTER output, int value, unsigned int len) {
  unsigned int i;

  for (i = 0; i < len; i++)
    ((char *)output)[i] = (char)value;
}

/* Note: Check .bin firmware_type and board_type before update.
*/
static int validate_firmware_and_board_type(bm_handle_t handle, Bin_buffer *bin_buf) {
  struct fwinfo *fwinfo;
  void *temp = 0;
  int j, firmware_type;
  std::size_t i;
  char *board_name;
  char *board_type;
  bm_status_t ret;

  unsigned char * const image = bin_buf->buf;
  unsigned long size = bin_buf->size;
  board_name = (char *)malloc(25);
  board_type = (char *)malloc(20);

  temp = (void *)(((char *)image) + size - 128 + 16);
  fwinfo = (struct fwinfo*)temp;
  firmware_type = fwinfo->type;

  for (i = 0; i < sizeof(firmware_table)/sizeof(firmware_table[0]); ++i) {
    if (firmware_table[i].id[0] == firmware_type)
        break;
  }

  if (i == sizeof(firmware_table)/sizeof(firmware_table[0])) {
    printf("firmware are not supported by this util\n");
    free(board_name);
    free(board_type);
    return -1;
  }

  ret = bm_get_board_name(handle, board_name);
  char sep = '-';
  strncpy(board_type, strchr(board_name,sep)+1, 10);
  if (ret != BM_SUCCESS) {
    free(board_name);
    free(board_type);
    return -1;
  }
  printf("board type=%s\n", board_type);
  for (j = 0; strcmp(firmware_table[i].type[j], "Error") != 0; j++) {
    if (strcmp(board_type, firmware_table[i].type[j]) == 0) {
      free(board_name);
      free(board_type);
      return 0;
    }
  }
  free(board_name);
  free(board_type);
  return -1;
}

/*********************MD5 Definition End***********************/

bool is_valid_mcu(bm_handle_t handle, Bin_buffer *bin_buf) {
  unsigned char * const image = bin_buf->buf;
  unsigned long size = bin_buf->size;
  int board_type = (int)((handle->misc_info.board_version & 0x0000FFFF) >> 8);

  /* check file size */
  if ((board_type == 33) || (board_type == 34)) {
    if (size != FLASH_SIZE_SC7P) {
      printf("wrong upgrade file size %ld, it should %ld bytes\n",
        size, (unsigned long)FLASH_SIZE_SC7P);
      return false;
    }
  } else {
    if (size != FLASH_SIZE) {
      printf("wrong upgrade file size %ld, it should %ld bytes\n",
        size, (unsigned long)FLASH_SIZE);
      return false;
    }
  }

  /* check md5 sum */
  MD5_CTX md_ctx;
  MD5Init(&md_ctx);
  unsigned long md_size;
  if ((board_type == 33) || (board_type == 34)) {
    md_size = PROGRAM_LIMIT_SC7P;
  } else {
    md_size = PROGRAM_LIMIT;
  }
  void *expected_dgst = image + md_size;
  unsigned char calculated_dgst[16];

  MD5Update(&md_ctx, image, md_size);
  MD5Final(calculated_dgst, &md_ctx);

  if (memcmp(expected_dgst, calculated_dgst, 16)) {
    printf("md5 check failed\n");
    printf("maybe not a valid upgrade file or being damaged\n");
    return false;
  }

  /* check application efie */
  struct efie *app_efie;

  if ((board_type == 33) || (board_type == 34)) {
    app_efie = (struct efie *)(image + EFIT_START_SC7P);
    if (app_efie->offset + app_efie->length > PROGRAM_LIMIT_SC7P) {
      printf("wrong efie of app\n");
      return false;
    }
  } else {
    app_efie = (struct efie *)(image + EFIT_START);
    if (app_efie->offset + app_efie->length > PROGRAM_LIMIT) {
      printf("wrong efie of app\n");
      return false;
    }
  }

  void *app_data = image + app_efie->offset;
  unsigned long app_len = app_efie->length;

  unsigned char calc_cksum[16];

  checksum(calc_cksum, app_data, app_len);

  if (memcmp(calc_cksum, app_efie->checksum, 16)) {
    printf("checksum of app wrong\n");
    return false;
  }
  return true;
}

void print_efie(struct efie *efie) {
  printf("EFIE: offset 0x%08x length 0x%08x checksum ",
         efie->offset, efie->length);
  int i;
  for (i = 0; i < 16; ++i)
    printf("%02x", efie->checksum[i]);
  printf("\n");
}

bm_status_t bm1684_firmware_update_mcu_app(bm_handle_t handle, Bin_buffer *bin_buf) {
  Bin_buffer efie_buf, app_buf, chksum_buf;
  unsigned char calc_cksum[16];

  int board_type = (int)((handle->misc_info.board_version & 0x0000FFFF) >> 8);

  if ((board_type == 33) || (board_type == 34)) {
	efie_buf.buf = bin_buf->buf + EFIT_START_SC7P;
	efie_buf.size = sizeof(struct efie);
	efie_buf.target_addr = EFIT_START_SC7P;
  } else {
	efie_buf.buf = bin_buf->buf + EFIT_START;
	efie_buf.size = sizeof(struct efie);
	efie_buf.target_addr = EFIT_START;
  }

  struct efie *app_efie = (struct efie *)(efie_buf.buf);
  print_efie(app_efie);

  app_buf.buf = bin_buf->buf + app_efie->offset;
  app_buf.size = app_efie->length;
  app_buf.target_addr = app_efie->offset;

  //check type
  printf("Checking firmware type and board type ...\n");
  if (0 != validate_firmware_and_board_type(handle, bin_buf)) {
    printf("firmware type or board type not match!\n");
    return BM_ERR_FAILURE;
  }
  printf("firmware type success match board type!\n");

  printf("Programming MCU APP firmware ...\n");
  // program efie
  #ifdef __linux__
  if (0 != ioctl(handle->dev_fd, BMDEV_PROGRAM_MCU, &efie_buf)) {
  #else
  if (0 != platform_ioctl(handle, BMDEV_PROGRAM_MCU, &efie_buf)) {
  #endif
    printf("program efie failed!\n");
    return BM_ERR_FAILURE;
  }
  printf("program efie succeeds.\n");
  // program app
  #ifdef __linux__
  if (0 != ioctl(handle->dev_fd, BMDEV_PROGRAM_MCU, &app_buf)) {
  #else
  if (0 != platform_ioctl(handle, BMDEV_PROGRAM_MCU, &app_buf)) {
  #endif
    printf("program app failed!\n");
    return BM_ERR_FAILURE;
  }
  printf("program app succeeds.\n");
  // calculate checksum
  if((board_type != 33) && (board_type != 34)) {
    memset(calc_cksum, 0x0, sizeof(calc_cksum));
    chksum_buf.buf = calc_cksum;
    chksum_buf.size = app_efie->length;
    chksum_buf.target_addr = app_efie->offset;

    #ifdef __linux__
    if (0 != ioctl(handle->dev_fd, BMDEV_CHECKSUM_MCU, &chksum_buf)) {
    #else
    if (0 != platform_ioctl(handle, BMDEV_CHECKSUM_MCU, &chksum_buf)) {
    #endif
      printf("get checksum failed!\n");
      return BM_ERR_FAILURE;
    }

    if (memcmp(calc_cksum, app_efie->checksum, 16)) {
      printf("checksum compare failed!\n");
      printf("expected checksum:\n");
      for (int i = 0; i < 16; ++i)
        printf("%02x", app_efie->checksum[i]);
      printf("\n");
      printf("calc checksum:\n");
      for (int i = 0; i < 16; ++i)
        printf("%02x", calc_cksum[i]);
      printf("\n");
      return BM_ERR_FAILURE;
    } else {
      printf("checksum compare succeeds.\n");
      return BM_SUCCESS;
    }
  }
  return BM_SUCCESS;
}

bm_status_t bm1684_firmware_update_mcu_full(bm_handle_t handle, Bin_buffer *bin_buf) {

  printf("Checking firmware type and board type ...\n");
  if (0 != validate_firmware_and_board_type(handle, bin_buf)) {
    printf("firmware type or board type not match!\n");
    return BM_ERR_FAILURE;
  }
  printf("firmware type success match board type!\n");

  printf("Programming MCU FULL firmware...\n");

  bin_buf->target_addr = 0;
  #ifdef __linux__
  if (0 == ioctl(handle->dev_fd, BMDEV_PROGRAM_MCU, bin_buf)) {
  #else
  if (0 == platform_ioctl(handle, BMDEV_PROGRAM_MCU, bin_buf)) {
  #endif
    return BM_SUCCESS;
  } else {
    return BM_ERR_FAILURE;
  }
}

int main(int argc, char *argv[]) {
  bm_handle_t handle = NULL;
  bm_status_t ret = BM_SUCCESS;
  Bin_buffer itcm_buf, ddr_buf, bin_buf;
  int i = 0;
  int count = 0;
  int chip_id = 0;
  bm_status_t (*bm1684_firmware_update)(bm_handle_t, Bin_buffer *);

  gflags::SetUsageMessage(
    "command line format\n"
    "usage: update firmware of A53/MCU\n"
    "[--dev=0/1...]\n"
    "[--file=file_name with path]\n"
    "[--target=a53/mcu]\n"
    "[--full]\n"
    "dev specifes which device to update, 0/1...\n"
    "file specifies the binary file to use\n"
    "target specifies where the file to burn\n"
    "full specifies if program full mcu firmware or only app\n"
    "example: bm_firmware_update --dev=0 --file=bl1.bin --target=a53\n");

  printf("%s\n", argv[0]);

  gflags::ParseCommandLineFlags(&argc, &argv, true);
  printf("device id: %d\n", FLAGS_dev);
  printf("bin file: %s\n", FLAGS_file.c_str());
  printf("target: %s\n", FLAGS_target.c_str());

  if (FLAGS_file == "") {
    printf("No bin file, please specify bin file name\n");
    return -EINVAL;
  }
  if (FLAGS_target.compare("a53") && FLAGS_target.compare("mcu")) {
    printf("invalid target option; a53 and mcu are valid.\n");
    return -EINVAL;
  }
  if (FLAGS_dev == 0xff) {
    chip_id = 0;
    bm_dev_getcount(&count);
    if (count < 1 || count > 0xff) {
      printf("bm_firmware_update get device count fail: %d\n", count);
      return -EINVAL;
    }
    printf("bm_firmware_update get device count = %d\n", count);
  } else {
    count = 0x1;
    chip_id = FLAGS_dev;
  }

  if (bm_load_bin_buffer(FLAGS_file.c_str(), &bin_buf)) {
    printf("Unable to load bin file into buffer : %s\n", FLAGS_file.c_str());
    goto fail;
  }

  if (FLAGS_target == "a53") {
    bm1684_firmware_update = bm1684_firmware_update_a53;
  } else if (FLAGS_target == "mcu") {
    ret = bm_dev_request(&handle, chip_id);
    if (ret != BM_SUCCESS || handle == NULL) {
      printf("bm_dev_request device %d failed, ret = %d\n", chip_id, ret);
      if (handle != NULL) {
        bm_dev_free(handle);
        handle = NULL;
      }
      goto fail;
    }
    if (is_valid_mcu(handle, &bin_buf)) {
      if (FLAGS_full)
        bm1684_firmware_update = bm1684_firmware_update_mcu_full;
      else
        bm1684_firmware_update = bm1684_firmware_update_mcu_app;
      if (handle != NULL) {
        bm_dev_free(handle);
        handle = NULL;
      }
    } else {
      printf("invalid mcu firmware bin!\n");
      if (handle != NULL) {
        bm_dev_free(handle);
        handle = NULL;
      }
      goto fail;
    }
  } else {
    printf("invalid target!\n");
    goto fail;
  }

  for (i = chip_id; i < chip_id + count; i++) {
    /*bm handle init */
    ret = bm_dev_request(&handle, i);
    if (ret != BM_SUCCESS || handle == NULL) {
      printf("bm_dev_request device %d failed, ret = %d\n", i, ret);
      if (handle != NULL) {
        bm_dev_free(handle);
        handle = NULL;
      }
      goto fail;
    }
    if (handle->misc_info.chipid == 0x1682) {
      printf("BM1682 FLASH UPDATE\n");
      /* load firmware bin file */
      bm_load_bin_buffer(argv[1], &itcm_buf);
      bm_load_bin_buffer(argv[2], &ddr_buf);
      /* load flash bin file */
      bm_load_bin_buffer(argv[3], &bin_buf);
      bm_release_bin_buffer(&itcm_buf);
      bm_release_bin_buffer(&ddr_buf);
    } else if (handle->misc_info.chipid == 0x1684) {
      printf("+----------------------------------------------+\n");
      printf("BM1684 %s firmware update chip_id = %d started.\n", FLAGS_target.c_str(), i);
      ret = bm1684_firmware_update(handle, &bin_buf);
      if (ret != BM_SUCCESS) {
        printf("BM1684 %s firmware update chip_id = %d failed!\n", FLAGS_target.c_str(), i);
        if (handle != NULL) {
        bm_dev_free(handle);
        handle = NULL;
      }
        goto fail;
      }
      printf("BM1684 %s firmware update chip_id = %d completed.\n", FLAGS_target.c_str(), i);
    } else if (handle->misc_info.chipid == 0x1686) {
      printf("+----------------------------------------------+\n");
      /*
       * if there is a day to reconstruct this function,
       * please movethe following check for filename to the begining of this function
       * input check should stay together
       */
      if(FLAGS_target == "a53" && FLAGS_file != "spi_flash_bm1684x.bin"){
	printf("Wrong version of flash bin file, please check if it is the correct file again!\n");
	if(handle != NULL){
		bm_dev_free(handle);
		handle = NULL;
	}
	goto fail;
      }
      printf("BM1684X %s firmware update chip_id = %d started.\n", FLAGS_target.c_str(), i);
      ret = bm1684_firmware_update(handle, &bin_buf);
      if (ret != BM_SUCCESS) {
        printf("BM1684X %s firmware update chip_id = %d failed!\n", FLAGS_target.c_str(), i);
        if (handle != NULL) {
        bm_dev_free(handle);
        handle = NULL;
        }
        goto fail;
      }
      printf("BM1684X %s firmware update chip_id = %d completed.\n", FLAGS_target.c_str(), i);
    } else {
      printf("Unknown CHIP!\n");
      goto fail;
    }
    if (handle != NULL) {
        bm_dev_free(handle);
        handle = NULL;
    }
  }

  bm_release_bin_buffer(&bin_buf);
  return 0;

fail:
  bm_release_bin_buffer(&bin_buf);
  return -1;

}
#else
int main(int argc, char *argv[]) {
  return 0;
}
#endif
