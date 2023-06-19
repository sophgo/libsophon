/*
 * Copyright (c) 2015-2016, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <gicv2.h>
#include <platform_def.h>

static const unsigned int irq_sec_array[] = {
    QEMU_IRQ_SEC_SGI_0,
    QEMU_IRQ_SEC_SGI_1,
    QEMU_IRQ_SEC_SGI_2,
    QEMU_IRQ_SEC_SGI_3,
    QEMU_IRQ_SEC_SGI_4,
    QEMU_IRQ_SEC_SGI_5,
    QEMU_IRQ_SEC_SGI_6,
    QEMU_IRQ_SEC_SGI_7,
};

const struct gicv2_driver_data plat_gicv2_driver_data = {
    .gicd_base = GICD_BASE,
    .gicc_base = GICC_BASE,
    .g0_interrupt_num = ARRAY_SIZE(irq_sec_array),
    .g0_interrupt_array = irq_sec_array,
};

