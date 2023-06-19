#ifndef __GDE_REG_DEF_H__
#define __GDE_REG_DEF_H__

// gde reg id defines
#define GDE_ID_SCR                  { 0, 32 }
#define GDE_ID_ICR                  { 32, 32 }
#define GDE_ID_DONE                 { 64, 1 }
#define GDE_ID_BUSY                 { 65, 1 }
#define GDE_ID_IN_ADDR_L32          { 96, 32 }
#define GDE_ID_IN_ADDR_H32          { 128, 32 }
#define GDE_ID_OUT_ADDR_L32         { 160, 32 }
#define GDE_ID_OUT_ADDR_H32         { 192, 32 }
#define GDE_ID_INDEX_ADDR_L32       { 224, 32 }
#define GDE_ID_INDEX_ADDR_H32       { 256, 32 }
#define GDE_ID_LEN                  { 288, 28 }
#define GDE_ID_TYPE                 { 316, 4 }

// gde pack defines
#define GDE_PACK_SCR(val)                   { GDE_ID_SCR, (val) }
#define GDE_PACK_ICR(val)                   { GDE_ID_ICR, (val) }
#define GDE_PACK_BUSY(val)                  { GDE_ID_BUSY, (val) }
#define GDE_PACK_DONE(val)                  { GDE_ID_DONE, (val) }
#define GDE_PACK_INDEX_ADDR_L32(val)        { GDE_ID_INDEX_ADDR_L32, (val) }
#define GDE_PACK_INDEX_ADDR_H32(val)        { GDE_ID_INDEX_ADDR_H32, (val) }
#define GDE_PACK_OUT_ADDR_L32(val)          { GDE_ID_OUT_ADDR_L32, (val) }
#define GDE_PACK_OUT_ADDR_H32(val)          { GDE_ID_OUT_ADDR_H32, (val) }
#define GDE_PACK_IN_ADDR_L32(val)           { GDE_ID_IN_ADDR_L32, (val) }
#define GDE_PACK_IN_ADDR_H32(val)           { GDE_ID_IN_ADDR_H32, (val) }
#define GDE_PACK_LEN(val)                   { GDE_ID_LEN, (val) }
#define GDE_PACK_TYPE(val)                  { GDE_ID_TYPE, (val) }

// gde default values
#define GDE_PACK_SCR_DEFAULT                GDE_PACK_SCR(0x0)

// default list
#define GDE_PACK_DEFAULTS { \
  GDE_PACK_SCR_DEFAULT \
}

typedef enum gde_type_enum {
  GDE_TYPE_BYTE = 0,
  GDE_TYPE_WORD = 2,
  GDE_TYPE_128BITS = 4
}gde_type_t;

#endif  // __GDE_REG_DEF_H__
