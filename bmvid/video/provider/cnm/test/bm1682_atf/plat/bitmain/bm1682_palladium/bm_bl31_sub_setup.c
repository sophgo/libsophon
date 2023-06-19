/*
 * Copyright (c) 2015-2016, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <gicv2.h>
#include <platform_def.h>

static const unsigned int irq_sec_array[] = {
    8, 9, 10, 11, // dummy, just for passing g0_interrupt_num assert in gicv2_driver_init
};

const struct gicv2_driver_data plat_gicv2_driver_data = {
    .gicd_base = GICD_BASE,
    .gicc_base = GICC_BASE,
    .g0_interrupt_num = ARRAY_SIZE(irq_sec_array),
    .g0_interrupt_array = irq_sec_array,
};

