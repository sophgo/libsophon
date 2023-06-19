/*****************************************************************************
 *
 *    Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
 *
 *    bmvid is licensed under the 2-Clause BSD License except for the
 *    third-party components.
 *
 *****************************************************************************/
/* This library provides a high-level interface for controlling the BitMain
 * Sophon VPU en/decoder.
 */


#ifndef __BM_QUEUE_H__
#define __BM_QUEUE_H__

#include <stdint.h>
#include <stdbool.h>
#if defined(_WIN32) || defined(WIN32) || defined(__WIN32__)
#define ATTRIBUTE 
#define DECL_EXPORT __declspec(dllexport)
#define DECL_IMPORT __declspec(dllimport)
#else
#define ATTRIBUTE __attribute__((deprecated))
#define DECL_EXPORT
#define DECL_IMPORT
#endif
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t* buffer;

    size_t   nmemb;
    size_t   size; /* bytes each element */
    int      count;

    int      front;
    int      rear;
} bm_queue_t;

/* Create a queue */
DECL_EXPORT bm_queue_t* bm_queue_create(size_t nmemb, size_t size);

/* Destroy a queue */
DECL_EXPORT void bm_queue_destroy(bm_queue_t* q);

/**
 * Enqueue with deep copy
 */
DECL_EXPORT bool bm_queue_push(bm_queue_t* q, void* data);

/**
 * Caller has responsibility for releasing the returned data
 */
DECL_EXPORT void* bm_queue_pop(bm_queue_t* q);

/**
 * Check if the queue is full.
 */
DECL_EXPORT bool bm_queue_is_full(bm_queue_t* q);

/**
 * Check if the queue is empty.
 */
DECL_EXPORT bool bm_queue_is_empty(bm_queue_t* q);

/**
 * Show the queue info.
 */
DECL_EXPORT bool bm_queue_show(bm_queue_t* q);

#ifdef __cplusplus
}
#endif

#endif /* __BM_QUEUE_H__ */

