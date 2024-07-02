
#ifndef _JPU_TYPES_H_
#define _JPU_TYPES_H_

#include <stdio.h>
#include <stdint.h>

typedef uint8_t          Uint8;
typedef uint16_t         Uint16;
typedef uint32_t         Uint32;
typedef uint64_t         Uint64;

typedef int8_t           Int8;
typedef int16_t          Int16;
typedef int32_t          Int32;
typedef int64_t          Int64;

typedef uint8_t          BYTE;
typedef unsigned int     UINT;
#ifndef BOOL
typedef int              BOOL;
#endif


#ifndef NULL
#define NULL            0
#endif

#if defined(linux) || defined(__linux) || defined(ANDROID)
#define TRUE            1
#define FALSE           0
#endif

#endif  /* _JPU_TYPES_H_ */

