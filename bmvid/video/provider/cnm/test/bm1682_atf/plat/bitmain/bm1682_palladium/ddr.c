/*
 * Copyright (c) 2015-2016, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <mmio.h>
#include <debug.h>
#include "platform_def.h"

#ifdef THIS_IS_FPGA
void plat_bm_ddr_init()
{
#ifndef DDR_INTERLEAVE_ENABLE
    //ddr0 addr configure
    mmio_write_32(0x50008180, 0x0);
    mmio_write_32(0x50008184, 0x1);
    mmio_write_32(0x50008188, 0xffffffff);
    mmio_write_32(0x5000818c, 0x1);
    //ddr1 addr configure
    mmio_write_32(0x50008190, 0x0);
    mmio_write_32(0x50008194, 0x2);
    mmio_write_32(0x50008198, 0xffffffff);
    mmio_write_32(0x5000819c, 0x2);
    //ddr2 addr configure
    mmio_write_32(0x500081a0, 0x0);
    mmio_write_32(0x500081a4, 0x3);
    mmio_write_32(0x500081a8, 0xffffffff);
    mmio_write_32(0x500081ac, 0x3);
    //ddr3 addr configure
    mmio_write_32(0x500081b0, 0x0);
    mmio_write_32(0x500081b4, 0x4);
    mmio_write_32(0x500081b8, 0xffffffff);
    mmio_write_32(0x500081bc, 0x4);
    //none-interleave
    mmio_write_32(0x500081c0, 0x2);
#endif
}
#else // for Palladium
static inline char poll(int addr, int exp_data, int msk)
{
    int  tmp_val;
    char tmp_flag = 1;

    while (tmp_flag) {
        tmp_val = mmio_read_32(addr);
        if ((tmp_val &~ msk) == (exp_data &~ msk))
            tmp_flag = 0;
    }
    return 1;
}

static void ecc_enable(uint32_t offset, uint8_t flag)
{
    uint32_t data = mmio_read_32(offset + 0x50000000);
    if (flag)
        mmio_write_32(offset + 0x50000000, (data | 0xc00)); //enable ecc bit[11..10]
    else
        mmio_write_32(offset + 0x50000000, (data & 0xfffff3ff)); //disable ecc bit[11..10]
}

static void ddr_init_pxp(uint32_t offset)
{
    uint32_t rdata_return;

    mmio_write_32(offset + 0x500007fc,0x60000000);
    mmio_write_32(offset + 0x50000100,0x022a0092);
    mmio_write_32(offset + 0x50000104,0x00002042);
    mmio_write_32(offset + 0x50000108,0x1a7a0063);
    mmio_write_32(offset + 0x5000010c,0x1a7a0063);
    mmio_write_32(offset + 0x50000110,0x1a7a0063);
    mmio_write_32(offset + 0x50000114,0x1a7a0063);
    mmio_write_32(offset + 0x50000118,0x20000000);
    mmio_write_32(offset + 0x5000011c,0x20000000);
    mmio_write_32(offset + 0x50000120,0x20000000);
    mmio_write_32(offset + 0x50000124,0x20000000);
    mmio_write_32(offset + 0x50000128,0x00000008);
    mmio_write_32(offset + 0x5000012c,0x00000000);
    mmio_write_32(offset + 0x50000130,0x00000000);
    mmio_write_32(offset + 0x50000134,0x00000000);
    mmio_write_32(offset + 0x50000138,0x00000000);
    mmio_write_32(offset + 0x5000013c,0x20000000);
    mmio_write_32(offset + 0x50000140,0x00000000);
    mmio_write_32(offset + 0x50000144,0x00000000);
    mmio_write_32(offset + 0x50000148,0x00000000);
    mmio_write_32(offset + 0x5000014c,0x00003080);
    mmio_write_32(offset + 0x50000150,0xf400f000);
    mmio_write_32(offset + 0x50000154,0x00000020);
    mmio_write_32(offset + 0x50000158,0x07700001);
    mmio_write_32(offset + 0x5000015c,0x04000000);
    mmio_write_32(offset + 0x50000160,0x00000400);
    mmio_write_32(offset + 0x50000164,0x00000000);
    mmio_write_32(offset + 0x50000168,0x000c0004);
    mmio_write_32(offset + 0x5000016c,0x00bb0200);
    mmio_write_32(offset + 0x50000170,0x000c0006);
    mmio_write_32(offset + 0x50000174,0x000f0000);
    mmio_write_32(offset + 0x50000178,0x0012c10c);
    mmio_write_32(offset + 0x5000017c,0x06080606);
    mmio_write_32(offset + 0x50000180,0x40001100);
    mmio_write_32(offset + 0x50000184,0x0c400a28);
    mmio_write_32(offset + 0x50000188,0x00001000);
    mmio_write_32(offset + 0x5000018c,0x05701016);
    mmio_write_32(offset + 0x50000190,0x00000000);
    mmio_write_32(offset + 0x5000019c,0x20202020);
    mmio_write_32(offset + 0x500001a0,0x20202020);
    mmio_write_32(offset + 0x500001a4,0x00002020);
    mmio_write_32(offset + 0x500001a8,0x80002020);
    mmio_write_32(offset + 0x500001ac,0x00000001);
    mmio_write_32(offset + 0x500001cc,0x01010101);
    mmio_write_32(offset + 0x500001d0,0x01010101);
    mmio_write_32(offset + 0x500001d4,0x08080808);
    mmio_write_32(offset + 0x500001d8,0x08080808);
    mmio_write_32(offset + 0x50000820,0x80800180);
    mmio_write_32(offset + 0x50000688,0x80808080);
    mmio_write_32(offset + 0x5000068c,0x80808080);
    mmio_write_32(offset + 0x50000690,0x80808080);
    mmio_write_32(offset + 0x50000694,0x80808080);
    mmio_write_32(offset + 0x500006f8,0x90909090);
    mmio_write_32(offset + 0x500006fc,0x90909090);
    mmio_write_32(offset + 0x50000800,0x00000000);
    mmio_write_32(offset + 0x50000824,0x00000090);
    mmio_write_32(offset + 0x50000194,0x0111ffe0);
    mmio_write_32(offset + 0x50000150,0x05000080);
    mmio_write_32(offset + 0x50000198,0x08060000);
    mmio_write_32(offset + 0x500001b0,0x00000000);
    mmio_write_32(offset + 0x500001b4,0x00000000);
    mmio_write_32(offset + 0x500001b8,0x00000000);
    mmio_write_32(offset + 0x500001bc,0x00000000);
    mmio_write_32(offset + 0x500001c0,0x00000000);
    mmio_write_32(offset + 0x500001c4,0x00000000);
    mmio_write_32(offset + 0x500001c8,0x000aff2c);
    mmio_write_32(offset + 0x500001dc,0x00080000);
    mmio_write_32(offset + 0x500001e0,0x00000000);
    mmio_write_32(offset + 0x500001e4,0xaa55aa55);
    mmio_write_32(offset + 0x500001e8,0x55aa55aa);
    mmio_write_32(offset + 0x500001ec,0xaaaa5555);
    mmio_write_32(offset + 0x500001f0,0x5555aaaa);
    mmio_write_32(offset + 0x500001f4,0xaa55aa55);
    mmio_write_32(offset + 0x500001f8,0x55aa55aa);
    mmio_write_32(offset + 0x500001fc,0xaaaa5555);
    mmio_write_32(offset + 0x50000600,0x5555aaaa);
    mmio_write_32(offset + 0x50000604,0xaa55aa55);
    mmio_write_32(offset + 0x50000608,0x55aa55aa);
    mmio_write_32(offset + 0x5000060c,0xaaaa5555);
    mmio_write_32(offset + 0x50000610,0x5555aaaa);
    mmio_write_32(offset + 0x50000614,0xaa55aa55);
    mmio_write_32(offset + 0x50000618,0x55aa55aa);
    mmio_write_32(offset + 0x5000061c,0xaaaa5555);
    mmio_write_32(offset + 0x50000620,0x5555aaaa);
    mmio_write_32(offset + 0x50000624,0x3f3f3f3f);
    mmio_write_32(offset + 0x50000628,0x3f3f3f3f);
    mmio_write_32(offset + 0x5000062c,0x3f3f3f3f);
    mmio_write_32(offset + 0x50000630,0x3f3f3f3f);
    mmio_write_32(offset + 0x50000634,0x3f3f3f3f);
    mmio_write_32(offset + 0x50000638,0x3f3f3f3f);
    mmio_write_32(offset + 0x5000063c,0x3f3f3f3f);
    mmio_write_32(offset + 0x50000640,0x3f3f3f3f);
    mmio_write_32(offset + 0x50000644,0x3f3f3f3f);
    mmio_write_32(offset + 0x50000648,0x3f3f3f3f);
    mmio_write_32(offset + 0x5000064c,0x3f3f3f3f);
    mmio_write_32(offset + 0x50000650,0x3f3f3f3f);
    mmio_write_32(offset + 0x50000654,0x3f3f3f3f);
    mmio_write_32(offset + 0x50000658,0x3f3f3f3f);
    mmio_write_32(offset + 0x5000065c,0x3f3f3f3f);
    mmio_write_32(offset + 0x50000660,0x3f3f3f3f);
    mmio_write_32(offset + 0x50000664,0x3f3f3f3f);
    mmio_write_32(offset + 0x50000668,0x3f3f3f3f);
    mmio_write_32(offset + 0x5000066c,0x3f3f3f3f);
    mmio_write_32(offset + 0x50000670,0x3f3f3f3f);
    mmio_write_32(offset + 0x50000698,0x3f3f0800);
    mmio_write_32(offset + 0x5000069c,0x3f3f3f3f);
    mmio_write_32(offset + 0x500006a0,0x3f3f3f3f);
    mmio_write_32(offset + 0x500006a4,0x3f3f3f3f);
    mmio_write_32(offset + 0x500006a8,0x3f3f3f3f);
    mmio_write_32(offset + 0x500006ac,0x3f3f3f3f);
    mmio_write_32(offset + 0x500006b0,0x3f3f3f3f);
    mmio_write_32(offset + 0x500006b4,0x3f3f3f3f);
    mmio_write_32(offset + 0x500006b8,0x3f3f3f3f);
    mmio_write_32(offset + 0x500006bc,0x3f3f3f3f);
    mmio_write_32(offset + 0x500006c0,0x3f3f3f3f);
    mmio_write_32(offset + 0x500006c4,0x3f3f3f3f);
    mmio_write_32(offset + 0x500006c8,0x3f3f3f3f);
    mmio_write_32(offset + 0x500006cc,0x3f3f3f3f);
    mmio_write_32(offset + 0x500006d0,0x3f3f3f3f);
    mmio_write_32(offset + 0x500006d4,0x3f3f3f3f);
    mmio_write_32(offset + 0x500006d8,0x3f3f3f3f);
    mmio_write_32(offset + 0x500006dc,0x3f3f3f3f);
    mmio_write_32(offset + 0x500006e0,0x3f3f3f3f);
    mmio_write_32(offset + 0x500006e4,0x3f3f3f3f);
    mmio_write_32(offset + 0x500006e8,0x00003f3f);
    mmio_write_32(offset + 0x50000804,0x00000800);
    mmio_write_32(offset + 0x50000828,0x3f3f3f3f);
    mmio_write_32(offset + 0x5000082c,0x3f3f3f3f);
    mmio_write_32(offset + 0x50000830,0x3f3f3f3f);
    mmio_write_32(offset + 0x50000834,0x3f3f3f3f);
    mmio_write_32(offset + 0x50000838,0x3f3f3f3f);
    mmio_write_32(offset + 0x5000083c,0x00003f3f);

    //disable ecc bit[11:10]
    mmio_write_32(offset + 0x50000000,0x00113001);

    mmio_write_32(offset + 0x50000004,0x0008001e);
    mmio_write_32(offset + 0x50000008,0x0000144e);
    mmio_write_32(offset + 0x5000000c,0x00010c11);
    mmio_write_32(offset + 0x50000030,0x00000000);
    mmio_write_32(offset + 0x50000040,0x751c180b);
    mmio_write_32(offset + 0x50000044,0x0a01d328);
    mmio_write_32(offset + 0x50000048,0x00142b14);
    mmio_write_32(offset + 0x5000004c,0x060e0926);
    mmio_write_32(offset + 0x50000050,0x4000e30e);
    mmio_write_32(offset + 0x50000054,0x00000020);
    mmio_write_32(offset + 0x5000005c,0x01440000);
    mmio_write_32(offset + 0x50000060,0x00000807);
    mmio_write_32(offset + 0x50000064,0x00000000);
    mmio_write_32(offset + 0x50000068,0x00001400);
    mmio_write_32(offset + 0x50000090,0x10731483);
    mmio_write_32(offset + 0x50000094,0x3ff62d49);
    mmio_write_32(offset + 0x50000098,0x144d2450);
    mmio_write_32(offset + 0x5000009c,0x19617595);
    mmio_write_32(offset + 0x500000a0,0x1e75c6da);
    mmio_write_32(offset + 0x500000a4,0x0003ffdf);
    mmio_write_32(offset + 0x500000a8,0x0003f38d);
    mmio_write_32(offset + 0x500000ac,0x00ffffcf);
    //-ddrc_tb.u_cfg_model- GUC DDR MC setting done 3556 ns
    mmio_write_32(offset + 0x500007fc,0x60000001);

    poll(offset + 0x50000004,0x80000000,0x7fffffff);
    poll(offset + 0x50000004,0xc008001e,0x0);
    mmio_write_32(offset + 0x50000004,0xc0080016);
    //[ddrc_tb.u_cfg_model][65510 ns] Release FORCE_RESETN_LOW.
    poll(offset + 0x50000004,0xc0080016,0x0);
    mmio_write_32(offset + 0x50000004,0xc0080012);
    //-ddrc_tb.u_cfg_model- Release FORCE_CKE_LOW at 67052 ns

    mmio_write_32(offset + 0x50000058,0x00000032);
    poll(offset + 0x50000058,0x00000000,0xfffffffd);

    mmio_write_32(offset + 0x50000058,0x00c00062);
    poll(offset + 0x50000058,0x00000000,0xfffffffd);

    mmio_write_32(offset + 0x50000058,0x00400052);
    poll(offset + 0x50000058,0x00000000,0xfffffffd);

    mmio_write_32(offset + 0x50000058,0x00000042);
    poll(offset + 0x50000058,0x00000000,0xfffffffd);

    mmio_write_32(offset + 0x50000058,0x00020022);
    poll(offset + 0x50000058,0x00000000,0xfffffffd);

    mmio_write_32(offset + 0x50000058,0x00001012);
    poll(offset + 0x50000058,0x00000000,0xfffffffd);

    mmio_write_32(offset + 0x50000058,0x00b70002);
    poll(offset + 0x50000058,0x00000000,0xfffffffd);

    poll(offset + 0x50000068,0x00001400,0x0);
    mmio_write_32(offset + 0x50000068,0x00003400);
    poll(offset + 0x50000068,0x00000000,0xffffdfff);

    poll(offset + 0x50000004,0xc0080012,0x0);
    mmio_write_32(offset + 0x50000004,0xc0080012);
    poll(offset + 0x50000004,0xc0080012,0x0);
    mmio_write_32(offset + 0x50000004,0xc0080010);
    rdata_return = mmio_read_32(offset + 0x50000004);

    if ((rdata_return & 0x00000002) /*[1]*/ == 0x00000002 /*1'b1*/) {
        mmio_write_32(0xffffff10,0x0B0B0B0B);
    }
#ifdef DDR_ECC_ENABLE
    INFO("ddr_%x ecc enable\n", offset);
    ecc_enable(offset, 1);
#else
    INFO("ddr_%x ecc disable\n", offset);
    ecc_enable(offset, 0);
#endif
}

void plat_bm_ddr_init()
{
#ifndef DDR_INTERLEAVE_ENABLE
    INFO("ddr interleave disable\n");
    mmio_write_32(0x50008180, 0x0);
    mmio_write_32(0x50008184, 0x1);
    mmio_write_32(0x50008188, 0xffffffff);
    mmio_write_32(0x5000818c, 0x1);

    mmio_write_32(0x50008190, 0x0);
    mmio_write_32(0x50008194, 0x2);
    mmio_write_32(0x50008198, 0xffffffff);
    mmio_write_32(0x5000819c, 0x2);

    mmio_write_32(0x500081a0, 0x0);
    mmio_write_32(0x500081a4, 0x3);
    mmio_write_32(0x500081a8, 0xffffffff);
    mmio_write_32(0x500081ac, 0x3);

    mmio_write_32(0x500081b0, 0x0);
    mmio_write_32(0x500081b4, 0x4);
    mmio_write_32(0x500081b8, 0xffffffff);
    mmio_write_32(0x500081bc, 0x4);

    mmio_write_32(0x500081c0, 0x2);
#else
    INFO("ddr interleave enable\n");
#endif

    ddr_init_pxp(0x0000);
    ddr_init_pxp(0x1000);
    //ddr_init_pxp(0x3000);
    //ddr_init_pxp(0x4000);
}
#endif