#ifndef __NMS_REG_DEF_H__
#define __NMS_REG_DEF_H__

// NMS reg id defines
#define NMS_ID_SGE                  { 0, 1 }
#define NMS_ID_SIDR                 { 1, 1 }
#define NMS_ID_MS                  { 2, 1 }
#define NMS_ID_CFS                 { 8, 8 }
#define NMS_ID_CSI                 { 16, 8 }
#define NMS_ID_IEOD                { 32, 1 }
#define NMS_ID_IOD                 { 40, 1 }
#define NMS_ID_DA                  { 64, 32 }
#define NMS_ID_PME                 { 96 - NMS_DES_OFFSET, 1 }
#define NMS_ID_CE                  { 97 - NMS_DES_OFFSET, 1 }
#define NMS_ID_BE                  { 98 - NMS_DES_OFFSET, 1 }
#define NMS_ID_CID                 { 104 - NMS_DES_OFFSET, 8 }
#define NMS_ID_TSI                 { 128 - NMS_DES_OFFSET, 16 }
#define NMS_ID_CDMASI              { 144 - NMS_DES_OFFSET, 16 }
#define NMS_ID_GDESI               { 160 - NMS_DES_OFFSET, 16 }
#define NMS_ID_GDMASI              { 176 - NMS_DES_OFFSET, 16 }
#define NMS_ID_SNT                 { 192 - NMS_DES_OFFSET, 32 }
#define NMS_ID_POPN                 { 224 - NMS_DES_OFFSET, 16 }
#define NMS_ID_PRPN                 { 240 - NMS_DES_OFFSET, 16 }
#define NMS_ID_PPSA                 { 256 - NMS_DES_OFFSET, 32 }
#define NMS_ID_SSA                 { 288 - NMS_DES_OFFSET, 32 }
#define NMS_ID_PPBS                 { 320 - NMS_DES_OFFSET, 32 }
#define NMS_ID_PSTRI                 { 352 - NMS_DES_OFFSET, 16 }
#define NMS_ID_PIDX                 { 368 - NMS_DES_OFFSET, 8 }
#define NMS_ID_PPRN                 { 384 - NMS_DES_OFFSET, 16 }

// NMS pack defines
#define NMS_PACK_SGE(val)                   { NMS_ID_SGE, (val) }
#define NMS_PACK_IEOD(val)                   { NMS_ID_IEOD, (val) }
#define NMS_PACK_PME(val)                   { NMS_ID_PME, (val) }


// NMS default values
#define NMS_PACK_SCR_DEFAULT                NMS_PACK_IEOD(0x0)

// default list
#define NMS_PACK_DEFAULTS { \
  NMS_PACK_SCR_DEFAULT \
}


#endif  // __NMS_REG_DEF_H__
