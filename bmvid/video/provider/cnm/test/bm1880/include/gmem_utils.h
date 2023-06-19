#ifndef _GMEM_UTILS_H_
#define _GMEM_UTILS_H_

#include "memmap.h"

#define GA_TO_PTR(ga)          ((float *)(GLOBAL_MEM_START_ADDR + (ga)))
#define GA_TO_PTR_INTEGER(ga)    ((u32 *)(GLOBAL_MEM_START_ADDR + (ga)))

void globalmem_fill(u64 ga, u64 size, float val, float step);
void globalmem_fill_u32(u64 ga, u64 size, u32 val, u32 step);
void globalmem_fill_rand(u64 ga, u64 size, float range);
void globalmem_fill_rand_ival(u64 ga, u32 count);
void globalmem_fill_rand_fixed_seed(u64 ga, u64 size, float range, u32 seed);
void globalmem_fill_u32_const(u64 ga, u32 val, u32 count);

void globalmem_clear(u64 ga, u32 size);
void globalmem_copy(u64 ga_dest, u64 ga_src, u64 nr_floats);
int globalmem_cmp(u64 ga_exp, u64 ga_got, u32 size, int exact_match, float delta);
int globalmem_cmp_epsilon(u64 ga_exp, u64 ga_got, u32 size, int exact_match, float epsilon);

int globalmem_cmp_u32_rand(u64 ga, u32 size);
int globalmem_cmp_u32_const(u64 ga, u32 val, u32 size);


void globalmem_dump(const char *desc, u64 ga, u32 size);

void globalmem_enable_error_print();
void globalmem_disable_error_print();

u64 globalmem_query_info(void);

#endif
