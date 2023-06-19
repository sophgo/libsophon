//------------------------------------------------------------------------------
// File: config.h
//
// Copyright (c) 2006, Chips & Media.  All rights reserved.
// This file should be modified by some developers of C&M according to product version.
//------------------------------------------------------------------------------
#ifndef __WINDATATYPE_H__
#define __WINDATATYPE_H__

#ifdef _WIN32
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

typedef unsigned long long u64;
typedef unsigned long long int U64;
typedef signed long long int s64;
typedef signed long long int S64;
#endif

#ifdef __linux__
typedef unsigned long u64;
#endif

#endif /* __CONFIG_H__ */

