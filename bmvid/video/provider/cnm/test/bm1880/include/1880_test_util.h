#ifndef INC_1880_TEST_UTIL_H
#define INC_1880_TEST_UTIL_H

#include "bm_kernel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

typedef struct {
  u64 gmem_size;
} bmctx_t;

typedef struct dim_s {
  int n, c, h, w;
} dim_t;

static inline int dim_size(const dim_t *dim)
{
  return dim->n * dim->c * dim->h * dim->w;
}

static inline dim_t dim_of_ith_element(int i, dim_t *dim, int transpose)
{
  int channel_offset = i % (dim->h * dim->w);
  int hidx = channel_offset / dim->w;
  int widx = channel_offset % dim->w;
  int channel_index = i / (dim->h * dim->w);
  int nidx = channel_index / dim->c;
  int cidx = channel_index % dim->c;
  if (transpose) {
    nidx = channel_index % dim->n;
    cidx = channel_index / dim->n;
  }
  dim_t r = { nidx, cidx, hidx, widx };
  return r;
}

static inline int shape_size(shape_t s)
{
  int n, c, h, w;
  __shape_to_lmem_nchw(s, &n, &c, &h, &w);
  return n * c * h * w;
}

static inline void * xmalloc(size_t size)
{
  void *p = malloc(size);
  assert(p);
  return p;
}

static inline void test_init(bmctx_t *ctx)
{
  ctx->gmem_size = 10000;
}

static inline void test_exit(bmctx_t *ctx)
{
  return;
}

static inline gaddr_t test_gmem_alloc(bmctx_t *ctx, u64 size)
{
  return 1024;
}

static inline void test_gmem_free(bmctx_t *ctx, gaddr_t mem)
{
  /* Nothing */
}

static inline
gaddr_t bm_tensor_s2d(bmctx_t *ctx, shape_t s, uint8_t *data)
{
  assert(s.dim == 4);
  int n, c, h, w;
  __shape_to_lmem_nchw(s, &n, &c, &h, &w);

  u64 size = n * c * h * w;
  gaddr_t gaddr = test_gmem_alloc(ctx, size);
  memcpy((void *)gaddr, data, size);
  return gaddr;
}

static inline
gaddr_t bm_matrix_s2d(bmctx_t *ctx, shape_t s, uint8_t *data)
{
  assert(s.dim == 2);
  int row = s.d1;
  int col = s.d0;

  u64 size = row * col;
  gaddr_t gaddr = test_gmem_alloc(ctx, size);
  memcpy((void *)gaddr, data, size);
  return gaddr;
}

static inline void put_tensor(bmctx_t *ctx, tensor_lmem *tl, uint8_t *data)
{
  gaddr_t gaddr = bm_tensor_s2d(ctx, tl->shape, data);
  tl_load_fixed(tl, gaddr, CTRL_NULL);
  kernel_submit();
  test_gmem_free(ctx, gaddr);
}

static inline void put_tensor_tp(bmctx_t *ctx, tensor_lmem *tl, uint8_t *data)
{
  gaddr_t gaddr = bm_tensor_s2d(ctx, tl->shape, data);
  tl_load_fixed(tl, gaddr, CTRL_TP);
  kernel_submit();
  test_gmem_free(ctx, gaddr);
}

static inline void put_tensor_stride(
    bmctx_t *ctx, tensor_lmem *tl, stride_t stride, uint8_t *data)
{
  int n, c, h, w;
  __shape_to_lmem_nchw(tl->shape, &n, &c, &h, &w);
  shape_t gmem_shape = shape_t4(n, stride.n, 1, 1);
  gaddr_t gaddr = bm_tensor_s2d(ctx, gmem_shape, data);
  tl_load_stride_fixed(tl, gaddr, stride, CTRL_NULL);
  kernel_submit();
  test_gmem_free(ctx, gaddr);
}

static inline tensor_lmem * put_matrix(bmctx_t *ctx, shape_t s, uint8_t *data)
{
  gaddr_t gaddr = bm_matrix_s2d(ctx, s, data);
  tensor_lmem *tl = tl_alloc(s, DATA_FMT_I8, CTRL_AL);
  tl_load_fixed(tl, gaddr, CTRL_NULL);
  kernel_submit();
  test_gmem_free(ctx, gaddr);
  return tl;
}

static inline
tensor_lmem * put_matrix_tp(bmctx_t *ctx, shape_t s, uint8_t *data)
{
  gaddr_t gaddr = bm_matrix_s2d(ctx, s, data);
  tensor_lmem *tl = tl_alloc(s, DATA_FMT_I8, CTRL_AL);
  tl_load_fixed(tl, gaddr, CTRL_TP);
  kernel_submit();
  test_gmem_free(ctx, gaddr);
  return tl;
}

static inline tensor_lmem *
put_matrix_stride(bmctx_t *ctx, shape_t s, stride_t stride, uint8_t *data)
{
  assert(s.dim == 2);
  int row = s.d1;
  int row_stride = stride.h;
  int size = row * row_stride;

  gaddr_t gaddr = test_gmem_alloc(ctx, size);
  memcpy((void *)gaddr, data, size);

  tensor_lmem *tl = tl_alloc(s, DATA_FMT_I8, CTRL_AL);
  tl_load_stride_fixed(tl, gaddr, stride, CTRL_NULL);
  kernel_submit();

  test_gmem_free(ctx, gaddr);
  return tl;
}

static inline uint8_t * get_tensor(bmctx_t *ctx, tensor_lmem *tl)
{
  shape_t s = tl->shape;
  assert(s.dim == 4);

  int n, c, h, w;
  __shape_to_lmem_nchw(s, &n, &c, &h, &w);
  int size = n * c * h * w;

  gaddr_t gaddr = test_gmem_alloc(ctx, size);
  tl_store_fixed(tl, gaddr, CTRL_NULL);
  kernel_submit();

  uint8_t *data = (uint8_t *)xmalloc(size);
  memcpy(data, (void *)gaddr, size);

  test_gmem_free(ctx, gaddr);
  return data;
}

static inline uint8_t * get_tensor_tp(bmctx_t *ctx, tensor_lmem *tl)
{
  shape_t s = tl->shape;
  assert(s.dim == 4);

  int n, c, h, w;
  __shape_to_lmem_nchw(s, &n, &c, &h, &w);
  int size = n * c * h * w;

  gaddr_t gaddr = test_gmem_alloc(ctx, size);
  tl_store_fixed(tl, gaddr, CTRL_TP);
  kernel_submit();

  uint8_t *data = (uint8_t *)xmalloc(size);
  memcpy(data, (void *)gaddr, size);

  test_gmem_free(ctx, gaddr);
  return data;
}

static inline
uint8_t * get_tensor_stride(bmctx_t *ctx, tensor_lmem *tl, stride_t stride)
{
  shape_t s = tl->shape;
  assert(s.dim == 4);

  int n, c, h, w;
  __shape_to_lmem_nchw(s, &n, &c, &h, &w);

  int stride_size = n * stride.n;
  uint8_t *data = (uint8_t *)xmalloc(stride_size);
  for (int i = 0; i < stride_size; i++)
    data[i] = 0xcf;

  shape_t gmem_shape = shape_t4(n, stride.n, 1, 1);
  gaddr_t gaddr = bm_tensor_s2d(ctx, gmem_shape, data);
  tl_store_stride_fixed(tl, gaddr, stride, CTRL_NULL);
  kernel_submit();
  memcpy(data, (void *)gaddr, stride_size);

  test_gmem_free(ctx, gaddr);
  return data;
}

static inline uint8_t * get_matrix(bmctx_t *ctx, tensor_lmem *tl)
{
  shape_t s = tl->shape;
  assert(s.dim == 2);
  int row = s.d1;
  int col = s.d0;
  int size = row * col;

  gaddr_t gaddr = test_gmem_alloc(ctx, size);
  tl_store_fixed(tl, gaddr, CTRL_NULL);
  kernel_submit();

  uint8_t *data = (uint8_t *)xmalloc(size);
  memcpy(data, (void *)gaddr, size);

  test_gmem_free(ctx, gaddr);
  return data;
}

static inline uint8_t * get_matrix_tp(bmctx_t *ctx, tensor_lmem *tl)
{
  shape_t s = tl->shape;
  assert(s.dim == 2);
  int row = s.d0;
  int col = s.d1;
  int size = row * col;

  gaddr_t gaddr = test_gmem_alloc(ctx, size);
  tl_store_fixed(tl, gaddr, CTRL_TP);
  kernel_submit();

  uint8_t *data = (uint8_t *)xmalloc(size);
  memcpy(data, (void *)gaddr, size);

  test_gmem_free(ctx, gaddr);
  return data;
}

static inline
uint8_t * get_matrix_stride(bmctx_t *ctx, tensor_lmem *tl, stride_t stride)
{
  shape_t s = tl->shape;
  assert(s.dim == 2);
  int row = s.d1;
  int row_stride = stride.h;
  int stride_size = row * row_stride;

  uint8_t *data = (uint8_t *)xmalloc(stride_size);
  for (int i = 0; i < stride_size; i++)
    data[i] = 0xaf;

  shape_t gmem_shape = shape_t4(row, row_stride, 1, 1);
  gaddr_t gaddr = bm_tensor_s2d(ctx, gmem_shape, data);
  tl_store_stride_fixed(tl, gaddr, stride, CTRL_NULL);
  kernel_submit();

  memcpy(data, (void *)gaddr, stride_size);

  test_gmem_free(ctx, gaddr);
  return data;
}

#endif /* INC_1880_TEST_UTIL_H */
