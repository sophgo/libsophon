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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "bmqueue.h"

bm_queue_t* bm_queue_create(size_t nmemb, size_t size)
{
    bm_queue_t* q = NULL;
    q = (bm_queue_t *)malloc(sizeof(bm_queue_t));
    if (q == NULL)
        return NULL;

    q->nmemb  = nmemb;
    q->size   = size;
    q->count  = 0;
    q->front  = 0;
    q->rear   = 0;
    q->buffer = (uint8_t*)calloc(nmemb, size);

    return q;
}

void bm_queue_destroy(bm_queue_t* q)
{
    if (q == NULL)
        return;

    if (q->buffer)
        free(q->buffer);

    free(q);
}

bool bm_queue_push(bm_queue_t* q, void* data)
{
    uint8_t* ptr;
    int      offset;

    if (q == NULL)
        return false;

    if (data  == NULL)
        return false;

    /* check if queue is full */
    if (bm_queue_is_full(q))
        return false;

    offset = q->rear * q->size;
    ptr = &q->buffer[offset];
    memcpy(ptr, data, q->size);

    q->rear++;
    q->rear %= q->nmemb;

    q->count++;

    return true;
}

void* bm_queue_pop(bm_queue_t* q)
{
    void* data;
    int   offset;

    if (q == NULL)
        return NULL;

    /* check if queue is empty */
    if (bm_queue_is_empty(q))
    {
        return NULL;
    }

    offset = q->front * q->size;
    data   = (void*)&q->buffer[offset];

    q->front++;
    q->front %= q->nmemb;

    q->count--;

    return data;
}

bool bm_queue_is_full(bm_queue_t* q)
{
    bool ret;
    if (q == NULL)
        return false;

    ret = (q->count >= q->nmemb);

    return ret;
}

bool bm_queue_is_empty(bm_queue_t* q)
{
    bool ret;
    if (q == NULL)
        return false;

    ret = (q->count <= 0);

    return ret;
}

bool bm_queue_show(bm_queue_t* q)
{
    int start;

    if (q == NULL)
        return false;

    if (bm_queue_is_empty(q))
        return true;

    start = q->front;
    do
    {
        int offset   = start * q->size;
        int* ptr = (int*)(&q->buffer[offset]);

        printf("%s:%d(%s) count=%d. front=%d, rear=%d. %dth: 0x%08x\n",
               __FILE__, __LINE__, __func__,
               q->count, q->front, q->rear, start, *ptr);

        start++;
        start %= q->nmemb;
    }
    while (start != q->rear);

    return true;
}

