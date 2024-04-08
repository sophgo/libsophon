#ifndef _BM_BGM_H_
#define _BM_BGM_H_

typedef unsigned long long bm_gmem_key_t;

#define BM_GMEM_PAGE_SIZE (4 * 1024)
#define BM_GMEM_MAKE_KEY(_a, _b) (((bm_gmem_key_t)_a) << 26 | _b)
#define BM_GMEM_KEY_TO_VALUE(_key) (_key >> 26)
#define BGM_4K_ALIGN(x) (((x) + BM_GMEM_PAGE_SIZE - 1) / BM_GMEM_PAGE_SIZE * BM_GMEM_PAGE_SIZE)

#define BGM_P_ALLOC(_x) ExAllocatePoolWithTag(NonPagedPoolNx, _x, 'BGM')
#define BGM_P_FREE(_x)  ExFreePoolWithTag(_x, 'BGM')

#define BGM_HEIGHT(_tree) (_tree == NULL ? -1 : _tree->height)

#define BM_BGM_MAX(_a, _b) (_a >= _b ? _a : _b)

typedef enum rotation_dir { LEFT, RIGHT } rotation_dir_t;

typedef struct bm_gmem_page_struct {
    s64 pageno;
    u64 addr;
    s32 used;
    u64 alloc_pages;
    s64 first_pageno;
} bm_gmem_page_t;

typedef struct avl_node_struct {
    bm_gmem_key_t           key;
    s32                     height;
    bm_gmem_page_t *        page;
    struct avl_node_struct *left;
    struct avl_node_struct *right;
} avl_node_t;

typedef struct bm_gmem_heap_struct {
    avl_node_t *    free_tree;
    avl_node_t *    alloc_tree;
    bm_gmem_page_t *page_list;
    u64             num_pages;
    u64             base_addr;
    u64             heap_size;
    u64             free_page_count;
    u64             alloc_page_count;
    s32             heap_id;
} bm_gmem_heap_t;

typedef struct bm_gmem_heap_struct jpu_mm_t;
typedef struct bm_gmem_heap_struct video_mm_t;

 typedef struct bm_gmem_heap_info {
    u64   total_pages;
    u64   alloc_pages;
    u64   free_pages;
    u64   page_size;
 } bm_gmem_heap_info_t;

int bmdrv_bgm_heap_cteate(bm_gmem_heap_t *heap, u64 addr, u64 size);
int bmdrv_bgm_heap_destroy(bm_gmem_heap_t *heap);
u64 bmdrv_bgm_heap_alloc(bm_gmem_heap_t *heap, u64 size);
int bmdrv_bgm_heap_free(bm_gmem_heap_t *heap, u64 addr);
int bmdrv_bgm_get_heap_info(bm_gmem_heap_t *heap, bm_gmem_heap_info_t *info);

#endif