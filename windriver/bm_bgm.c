#include "bm_common.h"

#include "bm_bgm.tmh"

 static avl_node_t* bmdrv_bgm_make_avl_node(bm_gmem_key_t key, bm_gmem_page_t *page)
{
    avl_node_t *node = (avl_node_t *)BGM_P_ALLOC(sizeof(avl_node_t));
    node->key     = key;
    node->page    = page;
    node->height  = 0;
    node->left    = NULL;
    node->right   = NULL;

    return node;
}

 static int bmdrv_bgm_get_balance_factor(avl_node_t *tree)
{
    int factor = 0;
    if (tree) {
        factor = BGM_HEIGHT(tree->right) - BGM_HEIGHT(tree->left);
    }

    return factor;
}

/*
 * Left Rotation
 *
 *      A                      B
 *       \                    / \
 *        B         =>       A   C
 *       /  \                 \
 *      D    C                 D
 *
 */
 static avl_node_t* bmdrv_bgm_rotation_left(avl_node_t *tree)
{
    avl_node_t *rchild;
    avl_node_t *lchild;

    if (tree == NULL)
        return NULL;

    rchild = tree->right;
    if (rchild == NULL) {
        return tree;
    }

    lchild = rchild->left;
    rchild->left = tree;
    tree->right = lchild;

    tree->height   = BM_BGM_MAX(BGM_HEIGHT(tree->left), BGM_HEIGHT(tree->right)) + 1;
    rchild->height = BM_BGM_MAX(BGM_HEIGHT(rchild->left),
    BGM_HEIGHT(rchild->right)) + 1;

    return rchild;
}

/*
 * Reft Rotation
 *
 *         A                  B
 *       \                  /  \
 *      B         =>       D    A
 *    /  \                     /
 *   D    C                   C
 *
 */
 static avl_node_t *bmdrv_bgm_rotation_right(avl_node_t *tree)
{
    avl_node_t *rchild;
    avl_node_t *lchild;

    if (tree == NULL)
        return NULL;

    lchild = tree->left;
    if (lchild == NULL)
        return NULL;

    rchild = lchild->right;
    lchild->right = tree;
    tree->left = rchild;

    tree->height   = BM_BGM_MAX(BGM_HEIGHT(tree->left), BGM_HEIGHT(tree->right)) + 1;
    lchild->height = BM_BGM_MAX(BGM_HEIGHT(lchild->left), BGM_HEIGHT(lchild->right)) + 1;

    return lchild;
}

 static avl_node_t* bmdrv_bgm_do_balance(avl_node_t *tree)
{
    int bfactor = 0, child_bfactor;       /* balancing factor */

    bfactor = bmdrv_bgm_get_balance_factor(tree);

    if (bfactor >= 2) {
        child_bfactor = bmdrv_bgm_get_balance_factor(tree->right);
        if (child_bfactor == 1 || child_bfactor == 0) {
            tree = bmdrv_bgm_rotation_left(tree);
        } else if (child_bfactor == -1) {
            tree->right = bmdrv_bgm_rotation_right(tree->right);
            tree        = bmdrv_bgm_rotation_left(tree);
        } else {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BGM, "invalid balancing factor: %d\n", child_bfactor);
            TraceEvents(TRACE_LEVEL_INFORMATION,
                        TRACE_BGM,
                        "BGM_ASSERT at %!FILE!:%!FUNC!:%!LINE!\n");
            return NULL;
        }
    } else if (bfactor <= -2) {
        child_bfactor = bmdrv_bgm_get_balance_factor(tree->left);
        if (child_bfactor == -1 || child_bfactor == 0) {
            tree = bmdrv_bgm_rotation_right(tree);
        } else if (child_bfactor == 1) {
            tree->left = bmdrv_bgm_rotation_left(tree->left);
            tree       = bmdrv_bgm_rotation_right(tree);
        } else {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BGM, "invalid balancing factor: %d\n", child_bfactor);
            TraceEvents(TRACE_LEVEL_INFORMATION,
                        TRACE_BGM,
                        "BGM_ASSERT at %!FILE!:%!FUNC!:%!LINE!\n");
            return NULL;
        }
    }

    return tree;
}

 static avl_node_t *bmdrv_bgm_unlink_end_node(avl_node_t *tree, int dir, avl_node_t **found_node)
{
    avl_node_t *node;
    *found_node = NULL;

    if (tree == NULL)
        return NULL;

    if (dir == LEFT) {
        if (tree->left == NULL) {
            *found_node = tree;
            return NULL;
        }
    } else {
        if (tree->right == NULL) {
            *found_node = tree;
            return NULL;
        }
    }

    if (dir == LEFT) {
        node = tree->left;
        tree->left = bmdrv_bgm_unlink_end_node(tree->left, LEFT, found_node);
        if (tree->left == NULL) {
            tree->left = (*found_node)->right;
            (*found_node)->left  = NULL;
            (*found_node)->right = NULL;
        }
    } else {
        node = tree->right;
        tree->right = bmdrv_bgm_unlink_end_node(tree->right, RIGHT, found_node);
        if (tree->right == NULL) {
            tree->right = (*found_node)->left;
            (*found_node)->left  = NULL;
            (*found_node)->right = NULL;
        }
    }

    tree->height = BM_BGM_MAX(BGM_HEIGHT(tree->left), BGM_HEIGHT(tree->right)) + 1;

    return bmdrv_bgm_do_balance(tree);
}

 static avl_node_t *bmdrv_bgm_avltree_insert(avl_node_t *tree, bm_gmem_key_t key, bm_gmem_page_t *page)
{
    if (tree == NULL) {
        tree = bmdrv_bgm_make_avl_node(key, page);
    } else {
        if (key >= tree->key) {
            tree->right = bmdrv_bgm_avltree_insert(tree->right, key, page);
        } else {
            tree->left  = bmdrv_bgm_avltree_insert(tree->left, key, page);
        }
    }

    tree = bmdrv_bgm_do_balance(tree);

    tree->height = BM_BGM_MAX(BGM_HEIGHT(tree->left), BGM_HEIGHT(tree->right)) + 1;

    return tree;
}

 static avl_node_t* bmdrv_bgm_do_unlink(avl_node_t *tree)
{
    avl_node_t *node;
    avl_node_t *end_node;
    node = bmdrv_bgm_unlink_end_node(tree->right, LEFT, &end_node);
    if (node) {
        tree->right = node;
    } else {
        node = bmdrv_bgm_unlink_end_node(tree->left, RIGHT, &end_node);
        if (node)
            tree->left = node;
    }

    if (node == NULL) {
        node = tree->right ? tree->right : tree->left;
        end_node = node;
    }

    if (end_node) {
        end_node->left  = (tree->left != end_node) ? tree->left : end_node->left;
        end_node->right = (tree->right != end_node) ? tree->right : end_node->right;
        end_node->height = BM_BGM_MAX(BGM_HEIGHT(end_node->left), BGM_HEIGHT(end_node->right)) + 1;
    }

    tree = end_node;

    return tree;
}

 static avl_node_t* bmdrv_bgm_avltree_remove(avl_node_t *tree, avl_node_t **found_node, bm_gmem_key_t key)
{
    *found_node = NULL;
    if (tree == NULL) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BGM, "failed to find key %d\n", (int)key);
        return NULL;
    }

    if (key == tree->key) {
        *found_node = tree;
        tree = bmdrv_bgm_do_unlink(tree);
    } else if (key > tree->key) {
        tree->right = bmdrv_bgm_avltree_remove(tree->right, found_node, key);
    } else {
        tree->left  = bmdrv_bgm_avltree_remove(tree->left, found_node, key);
    }

    if (tree)
        tree->height = BM_BGM_MAX(BGM_HEIGHT(tree->left), BGM_HEIGHT(tree->right)) + 1;

    tree = bmdrv_bgm_do_balance(tree);

    return tree;
}

 static void bmdrv_bgm_avltree_free(avl_node_t *tree)
{
    if (tree == NULL)
        return;
    if (tree->left == NULL && tree->right == NULL) {
        BGM_P_FREE(tree);
        return;
    }

    bmdrv_bgm_avltree_free(tree->left);
    tree->left = NULL;
    bmdrv_bgm_avltree_free(tree->right);
    tree->right = NULL;
    BGM_P_FREE(tree);
}

 static avl_node_t* bmdrv_bgm_remove_approx_value(avl_node_t *tree, avl_node_t **found, bm_gmem_key_t key)
{
    *found = NULL;
    if (tree == NULL) {
        return NULL;
    }

    if (key == tree->key) {
        *found = tree;
        tree = bmdrv_bgm_do_unlink(tree);
    } else if (key > tree->key) {
        tree->right = bmdrv_bgm_remove_approx_value(tree->right, found, key);
    } else {
        tree->left  = bmdrv_bgm_remove_approx_value(tree->left, found, key);
        if (*found == NULL) {
            *found = tree;
            tree = bmdrv_bgm_do_unlink(tree);
        }
    }
    if (tree)
        tree->height = BM_BGM_MAX(BGM_HEIGHT(tree->left), BGM_HEIGHT(tree->right)) + 1;

    tree = bmdrv_bgm_do_balance(tree);

    return tree;
}

 static void bmdrv_bgm_heap_set_blocks_free(bm_gmem_heap_t *heap, s64 pageno, u64 npages)
{
    s64 last_pageno     = pageno + npages - 1;
    bm_gmem_page_t *page;
    bm_gmem_page_t *last_page;

    if (!npages) {
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_BGM,
                    "BGM_ASSERT at %!FILE!:%!FUNC!:%!LINE!\n");
        return;
    }

    if (last_pageno >= (s64)heap->num_pages) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BGM, "bmdrv_bgm_heap_set_blocks_free: invalid last page number: %lld\n", last_pageno);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BGM, "BGM_ASSERT at %!FILE!:%!FUNC!:%!LINE!\n");
        return;
    }

    page        = &heap->page_list[pageno];
    page->alloc_pages = npages;
    page->used = 0;
    page->first_pageno = -1;

    last_page   = &heap->page_list[last_pageno];
    last_page->first_pageno = pageno;
    last_page->used = 0;

    heap->free_tree = bmdrv_bgm_avltree_insert(heap->free_tree, BM_GMEM_MAKE_KEY(npages, pageno), page);
}

 static void bmdrv_bgm_heap_set_blocks_alloc(bm_gmem_heap_t *heap, s64 pageno, u64 npages)
{
    s64 last_pageno     = pageno + npages - 1;
    bm_gmem_page_t *page;
    bm_gmem_page_t *last_page;

    if (last_pageno >= (s64)heap->num_pages) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BGM, "bmdrv_bgm_heap_set_blocks_free:invalid last page number: %lld\n", last_pageno);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BGM, "BGM_ASSERT at %!FILE!:%!FUNC!:%!LINE!\n");
        return;
    }

    page        = &heap->page_list[pageno];
    page->alloc_pages = npages;
    page->used         = 1;
    page->first_pageno = -1;

    last_page   = &heap->page_list[last_pageno];
    last_page->first_pageno = pageno;
    last_page->used         = 1;

    heap->alloc_tree = bmdrv_bgm_avltree_insert(heap->alloc_tree, BM_GMEM_MAKE_KEY(page->addr, 0), page);
}

int bmdrv_bgm_heap_cteate(bm_gmem_heap_t *heap, u64 addr, u64 size)
{
    u64 i;

    if (NULL == heap)
        return -1;

    heap->base_addr  = (addr+(BM_GMEM_PAGE_SIZE-1))&~(BM_GMEM_PAGE_SIZE-1);
    heap->heap_size   = size&~BM_GMEM_PAGE_SIZE;
    heap->num_pages  = heap->heap_size/BM_GMEM_PAGE_SIZE;
    heap->free_tree  = NULL;
    heap->alloc_tree = NULL;
    heap->free_page_count = heap->num_pages;
    heap->alloc_page_count = 0;
    heap->page_list  = (bm_gmem_page_t *)BGM_P_ALLOC(heap->num_pages*sizeof(bm_gmem_page_t));
    if (heap->page_list == NULL) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BGM, "%!FUNC!:%!LINE! failed to kmalloc(%d)\n",
        (int)(heap->num_pages*sizeof(bm_gmem_page_t)));
        return -1;
    }

    for (i = 0; i < heap->num_pages; i++) {
        heap->page_list[i].pageno       = i;
        heap->page_list[i].addr         = heap->base_addr + i*(u64)BM_GMEM_PAGE_SIZE; 
        heap->page_list[i].alloc_pages  = 0;
        heap->page_list[i].used         = 0;
        heap->page_list[i].first_pageno = -1;
    }

    bmdrv_bgm_heap_set_blocks_free(heap, 0, heap->num_pages);

    return 0;
}

int bmdrv_bgm_heap_destroy(bm_gmem_heap_t *heap)
{
    if (heap == NULL) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BGM, "bmdrv_bgm_heap_destroy: invalid handle\n");
        return -1;
    }

    if (heap->free_tree) {
        bmdrv_bgm_avltree_free(heap->free_tree);
    }
    if (heap->alloc_tree) {
        bmdrv_bgm_avltree_free(heap->alloc_tree);
    }

    if (heap->page_list) {
    BGM_P_FREE(heap->page_list);
        heap->page_list = NULL;
    }

    heap->base_addr  = 0;
    heap->heap_size   = 0;
    heap->num_pages  = 0;
    heap->page_list  = NULL;
    heap->free_tree  = NULL;
    heap->alloc_tree = NULL;
    heap->free_page_count = 0;
    heap->alloc_page_count = 0;
    return 0;
}

u64 bmdrv_bgm_heap_alloc(bm_gmem_heap_t *heap, u64 size)
{
    avl_node_t *node;
    bm_gmem_page_t *free_page;
    u64         npages, free_size;
    s64         alloc_pageno;
    u64  ptr;

    if (heap == NULL) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BGM, "bmdrv_bgm_heap_alloc: invalid handle\n");
        return (u64)-1;
    }

    if (size <= 0)
        return (u64)-1;

    npages = (size + BM_GMEM_PAGE_SIZE - 1)/BM_GMEM_PAGE_SIZE;

    heap->free_tree = bmdrv_bgm_remove_approx_value(heap->free_tree, &node, BM_GMEM_MAKE_KEY(npages, 0));
    if (node == NULL) {
        return (u64)-1;
    }
    free_page = node->page;
    free_size = BM_GMEM_KEY_TO_VALUE(node->key);

    alloc_pageno = free_page->pageno;
    bmdrv_bgm_heap_set_blocks_alloc(heap, alloc_pageno, npages);
    if (npages != free_size) {
        s64 free_pageno = alloc_pageno + npages;
        bmdrv_bgm_heap_set_blocks_free(heap, free_pageno, (free_size-npages));
    }

    BGM_P_FREE(node);

    ptr = heap->page_list[alloc_pageno].addr;
    heap->alloc_page_count += npages;
    heap->free_page_count  -= npages;

    return ptr;
}

int bmdrv_bgm_heap_free(bm_gmem_heap_t *heap, u64 addr)
{
    avl_node_t *found;
    bm_gmem_page_t *page;
    s64 pageno, prev_free_pageno, next_free_pageno;
    s64 prev_size, next_size;
    s64 merge_page_no;
    u64 merge_page_size, free_page_size;

    if (heap == NULL) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BGM, "bmdrv_bgm_heap_free: invalid handle\n");
        return -1;
    }

    heap->alloc_tree = bmdrv_bgm_avltree_remove(heap->alloc_tree, &found, BM_GMEM_MAKE_KEY(addr, 0));
    if (found == NULL) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BGM, "bmdrv_bgm_heap_free: 0x%llx not found\n", addr);
        TraceEvents(TRACE_LEVEL_INFORMATION,
                    TRACE_BGM,
                    "BGM_ASSERT at %!FILE!:%!FUNC!:%!LINE!\n");
        return -1;
    }

    /* find previous free block */
    page = found->page;
    pageno = page->pageno;
    free_page_size = page->alloc_pages;
    prev_free_pageno = pageno-1;
    prev_size = -1;
    if (prev_free_pageno >= 0) {
        if (heap->page_list[prev_free_pageno].used == 0) {
            prev_free_pageno = heap->page_list[prev_free_pageno].first_pageno;
            prev_size = heap->page_list[prev_free_pageno].alloc_pages;
        }
    }

    /* find next free block */
    next_free_pageno = pageno + page->alloc_pages;
    next_free_pageno = (next_free_pageno == (s64)heap->num_pages) ? -1 : next_free_pageno;
    next_size = -1;
    if (next_free_pageno >= 0) {
        if (heap->page_list[next_free_pageno].used == 0) {
            next_size = heap->page_list[next_free_pageno].alloc_pages;
        }
    }
    BGM_P_FREE(found);

    /* merge */
    merge_page_no = page->pageno;
    merge_page_size = page->alloc_pages;
    if (prev_size >= 0) {
        heap->free_tree = bmdrv_bgm_avltree_remove(heap->free_tree, &found, BM_GMEM_MAKE_KEY(prev_size, prev_free_pageno));
        if (found == NULL) {
            TraceEvents(TRACE_LEVEL_INFORMATION,
                        TRACE_BGM,
                        "BGM_ASSERT at %!FILE!:%!FUNC!:%!LINE!\n");
            return -1;
        }
        merge_page_no = found->page->pageno;
        merge_page_size += found->page->alloc_pages;
        BGM_P_FREE(found);
    }
    if (next_size >= 0) {
        heap->free_tree = bmdrv_bgm_avltree_remove(heap->free_tree, &found, BM_GMEM_MAKE_KEY(next_size, next_free_pageno));
        if (found == NULL) {
            TraceEvents(TRACE_LEVEL_INFORMATION,
                        TRACE_BGM,
                        "BGM_ASSERT at %!FILE!:%!FUNC!:%!LINE!\n");
            return -1;
        }
        merge_page_size += found->page->alloc_pages;
        BGM_P_FREE(found);
    }

    page->alloc_pages  = 0;
    page->first_pageno = -1;

    bmdrv_bgm_heap_set_blocks_free(heap, merge_page_no, merge_page_size);

    heap->alloc_page_count -= free_page_size;
    heap->free_page_count  += free_page_size;

    return 0;
}

 int bmdrv_bgm_get_heap_info(bm_gmem_heap_t *heap, bm_gmem_heap_info_t *info )
{
    if (heap == NULL) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_BGM, "bmdrv_bgm_get_info: invalid handle\n");
        return -1;
    }

    if (info == NULL) {
        return -1;
    }

    info->total_pages = heap->num_pages;
    info->alloc_pages = heap->alloc_page_count;
    info->free_pages  = heap->free_page_count;
    info->page_size   = BM_GMEM_PAGE_SIZE;

    return 0;
}