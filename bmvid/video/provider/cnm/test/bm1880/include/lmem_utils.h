#ifndef _LMEM_UTILS_H_
#define _LMEM_UTILS_H_

#include <common.h>

typedef struct {
  int nr_mems;
  int nr_bad_mems;
  u8 mem_is_bad[MAX_NPU_NUM + 2];
} lmem_info_t;

void localmem_fill_rand(u32 la, u32 size, float range);
void localmem_fill(u32 la, u32 size, float val, float npu_step, float step);
void localmem_clear(u32 la, u32 size);
int localmem_cmp(u32 la_exp, u32 la_got, u32 size, int exact_match, float delta);
int localmem_cmp_u32_const(u32 la, u32 val, u32 size);
void localmem_dump(u32 la, u32 size);

u32 poll_bad_localmem(u32 la, u32 val, u32 size);
int localmem_repair(u32 lmem);
void localmem_mark_bad(int i);
void localmem_reset_info();
const lmem_info_t * localmem_query_info();

void localmem_enable_error_print();
void localmem_disable_error_print();
#endif
