/*
 * Copyright (c) 2016, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __PMU_REGS_H__
#define __PMU_REGS_H__

#define PMU_WKUP_CFG0        0x00
#define PMU_WKUP_CFG1        0x04
#define PMU_WKUP_CFG2        0x08
#define PMU_WKUP_CFG3        0x0c
#define PMU_WKUP_CFG4        0x10
#define PMU_PWRDN_CON        0x14
#define PMU_PWRDN_ST        0x18
#define PMU_PLL_CON        0x1c
#define PMU_PWRMODE_CON        0x20
#define PMU_SFT_CON        0x24
#define PMU_INT_CON        0x28
#define PMU_INT_ST        0x2c
#define PMU_GPIO0_POS_INT_CON    0x30
#define PMU_GPIO0_NEG_INT_CON    0x34
#define PMU_GPIO1_POS_INT_CON    0x38
#define PMU_GPIO1_NEG_INT_CON    0x3c
#define PMU_GPIO0_POS_INT_ST    0x40
#define PMU_GPIO0_NEG_INT_ST    0x44
#define PMU_GPIO1_POS_INT_ST    0x48
#define PMU_GPIO1_NEG_INT_ST    0x4c
#define PMU_PWRDN_INTEN        0x50
#define PMU_PWRDN_STATUS    0x54
#define PMU_WAKEUP_STATUS    0x58
#define PMU_BUS_CLR        0x5c
#define PMU_BUS_IDLE_REQ    0x60
#define PMU_BUS_IDLE_ST        0x64
#define PMU_BUS_IDLE_ACK    0x68
#define PMU_CCI500_CON        0x6c
#define PMU_ADB400_CON        0x70
#define PMU_ADB400_ST        0x74
#define PMU_POWER_ST        0x78
#define PMU_CORE_PWR_ST        0x7c
#define PMU_OSC_CNT        0x80
#define PMU_PLLLOCK_CNT        0x84
#define PMU_PLLRST_CNT        0x88
#define PMU_STABLE_CNT        0x8c
#define PMU_DDRIO_PWRON_CNT    0x90
#define PMU_WAKEUP_RST_CLR_CNT    0x94
#define PMU_DDR_SREF_ST        0x98
#define PMU_SCU_L_PWRDN_CNT    0x9c
#define PMU_SCU_L_PWRUP_CNT    0xa0
#define PMU_SCU_B_PWRDN_CNT    0xa4
#define PMU_SCU_B_PWRUP_CNT    0xa8
#define PMU_GPU_PWRDN_CNT    0xac
#define PMU_GPU_PWRUP_CNT    0xb0
#define PMU_CENTER_PWRDN_CNT    0xb4
#define PMU_CENTER_PWRUP_CNT    0xb8
#define PMU_TIMEOUT_CNT        0xbc
#define PMU_CPU0APM_CON        0xc0
#define PMU_CPU1APM_CON        0xc4
#define PMU_CPU2APM_CON        0xc8
#define PMU_CPU3APM_CON        0xcc
#define PMU_CPU0BPM_CON        0xd0
#define PMU_CPU1BPM_CON        0xd4
#define PMU_NOC_AUTO_ENA    0xd8
#define PMU_PWRDN_CON1        0xdc

#define PMUGRF_GPIO0A_IOMUX    0x00
#define PMUGRF_GPIO1A_IOMUX    0x10
#define PMUGRF_GPIO1C_IOMUX    0x18

#define PMUGRF_GPIO0A6_IOMUX_SHIFT      12
#define PMUGRF_GPIO0A6_IOMUX_PWM        0x1
#define PMUGRF_GPIO1C3_IOMUX_SHIFT      6
#define PMUGRF_GPIO1C3_IOMUX_PWM        0x1

#define CPU_AXI_QOS_ID_COREID        0x00
#define CPU_AXI_QOS_REVISIONID        0x04
#define CPU_AXI_QOS_PRIORITY        0x08
#define CPU_AXI_QOS_MODE        0x0c
#define CPU_AXI_QOS_BANDWIDTH        0x10
#define CPU_AXI_QOS_SATURATION        0x14
#define CPU_AXI_QOS_EXTCONTROL        0x18
#define CPU_AXI_QOS_NUM_REGS        0x07

#define CPU_AXI_CCI_M0_QOS_BASE        0xffa50000
#define CPU_AXI_CCI_M1_QOS_BASE        0xffad8000
#define CPU_AXI_DMAC0_QOS_BASE        0xffa64200
#define CPU_AXI_DMAC1_QOS_BASE        0xffa64280
#define CPU_AXI_DCF_QOS_BASE        0xffa64180
#define CPU_AXI_CRYPTO0_QOS_BASE    0xffa64100
#define CPU_AXI_CRYPTO1_QOS_BASE    0xffa64080
#define CPU_AXI_PMU_CM0_QOS_BASE    0xffa68000
#define CPU_AXI_PERI_CM1_QOS_BASE    0xffa64300
#define CPU_AXI_GIC_QOS_BASE        0xffa78000
#define CPU_AXI_SDIO_QOS_BASE        0xffa76000
#define CPU_AXI_SDMMC_QOS_BASE        0xffa74000
#define CPU_AXI_EMMC_QOS_BASE        0xffa58000
#define CPU_AXI_GMAC_QOS_BASE        0xffa5c000
#define CPU_AXI_USB_OTG0_QOS_BASE    0xffa70000
#define CPU_AXI_USB_OTG1_QOS_BASE    0xffa70080
#define CPU_AXI_USB_HOST0_QOS_BASE    0xffa60100
#define CPU_AXI_USB_HOST1_QOS_BASE    0xffa60180
#define CPU_AXI_GPU_QOS_BASE        0xffae0000
#define CPU_AXI_VIDEO_M0_QOS_BASE    0xffab8000
#define CPU_AXI_VIDEO_M1_R_QOS_BASE    0xffac0000
#define CPU_AXI_VIDEO_M1_W_QOS_BASE    0xffac0080
#define CPU_AXI_RGA_R_QOS_BASE        0xffab0000
#define CPU_AXI_RGA_W_QOS_BASE        0xffab0080
#define CPU_AXI_IEP_QOS_BASE        0xffa98000
#define CPU_AXI_VOP_BIG_R_QOS_BASE    0xffac8000
#define CPU_AXI_VOP_BIG_W_QOS_BASE    0xffac8080
#define CPU_AXI_VOP_LITTLE_QOS_BASE    0xffad0000
#define CPU_AXI_ISP0_M0_QOS_BASE    0xffaa0000
#define CPU_AXI_ISP0_M1_QOS_BASE    0xffaa0080
#define CPU_AXI_ISP1_M0_QOS_BASE    0xffaa8000
#define CPU_AXI_ISP1_M1_QOS_BASE    0xffaa8080
#define CPU_AXI_HDCP_QOS_BASE        0xffa90000
#define CPU_AXI_PERIHP_NSP_QOS_BASE    0xffad8080
#define CPU_AXI_PERILP_NSP_QOS_BASE    0xffad8180
#define CPU_AXI_PERILPSLV_NSP_QOS_BASE    0xffad8100

#define GRF_GPIO2A_IOMUX    0xe000
#define GRF_GPIO2B_IOMUX    0xe004
#define GRF_GPIO2C_IOMUX    0xe008
#define GRF_GPIO2D_IOMUX    0xe00c
#define GRF_GPIO3A_IOMUX    0xe010
#define GRF_GPIO3B_IOMUX    0xe014
#define GRF_GPIO3C_IOMUX    0xe018
#define GRF_GPIO3D_IOMUX    0xe01c
#define GRF_GPIO4A_IOMUX    0xe020
#define GRF_GPIO4B_IOMUX    0xe024
#define GRF_GPIO4C_IOMUX    0xe028
#define GRF_GPIO4D_IOMUX    0xe02c

#define GRF_GPIO2A_P        0xe040
#define GRF_GPIO2B_P        0xe044
#define GRF_GPIO2C_P        0xe048
#define GRF_GPIO2D_P        0xe04C
#define GRF_GPIO3A_P        0xe050
#define GRF_GPIO3B_P        0xe054
#define GRF_GPIO3C_P        0xe058
#define GRF_GPIO3D_P        0xe05C
#define GRF_GPIO4A_P        0xe060
#define GRF_GPIO4B_P        0xe064
#define GRF_GPIO4C_P        0xe068
#define GRF_GPIO4D_P        0xe06C

#endif /* __PMU_REGS_H__ */
