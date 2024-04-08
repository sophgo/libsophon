#ifndef _BM_DEF_H
#define _BM_DEF_H


//--------------------------------------
// Some utility macros
//--------------------------------------
#ifndef min
#define min(_a, _b)     (((_a) < (_b)) ? (_a) : (_b))
#endif

#ifndef max
#define max(_a, _b)     (((_a) > (_b)) ? (_a) : (_b))
#endif

#define BM_EINVAL 22
//--------------------------------------
// data type define  for windows
//--------------------------------------

typedef unsigned char u8;
typedef unsigned char U8;

typedef signed char s8;
typedef signed char S8;

typedef unsigned short u16;
typedef unsigned short U16;

typedef signed short s16;
typedef signed short S16;

typedef unsigned int u32;
typedef unsigned int U32;
typedef signed int s32;
typedef signed int S32;

typedef unsigned long long int u64;
typedef unsigned long long int U64;
typedef signed long long int s64;
typedef signed long long int S64;

//typedef unsigned long uint_t;
//typedef unsigned long long u64;
//typedef unsigned long u32;
//typedef long int       s32;
//typedef unsigned char      u8;
//typedef unsigned long      u_int;
//typedef unsigned long long u_long;
//typedef unsigned int  UINT;
//typedef unsigned int *PUINT;

#ifndef bool
#define  bool int
#endif  // !bool

typedef unsigned long long phys_addr_t;
typedef long long          ssize_t;
typedef unsigned long long dma_addr_t;
typedef unsigned long long pid_t;
typedef long long loff_t;

#endif