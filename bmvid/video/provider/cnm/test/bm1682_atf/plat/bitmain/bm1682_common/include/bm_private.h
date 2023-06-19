/*
 * Copyright (c) 2015-2016, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __BM_PRIVATE_H
#define __BM_PRIVATE_H

#include <sys/types.h>

void bm_configure_mmu_el1(unsigned long total_base, unsigned long total_size,
            unsigned long ro_start, unsigned long ro_limit,
            unsigned long coh_start, unsigned long coh_limit);

void bm_configure_mmu_el3(unsigned long total_base, unsigned long total_size,
            unsigned long ro_start, unsigned long ro_limit,
            unsigned long coh_start, unsigned long coh_limit);

void plat_bm_io_setup(void);
unsigned int plat_bm_calc_core_pos(u_register_t mpidr);

void plat_bm_ddr_init(void);

int dt_add_psci_node(void *fdt);
int dt_add_psci_cpu_enable_methods(void *fdt);

extern struct gicv2_driver_data plat_gicv2_driver_data;

#endif /*__BM_PRIVATE_H*/
