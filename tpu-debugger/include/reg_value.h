#ifndef REG_VALUE_
#define REG_VALUE_
#include <magic_enum/magic_enum.hpp>

enum class TSK_TYPE {
    CONV = 0,
    PD   = 1,
    MM   = 2,
    AR   = 3,
    RQDQ = 4,
    TRANS_BC = 5,
    SG   = 6,
    LAR  = 7,
    SFU  = 9,
    LIN  = 10,
    CMP  = 13,
    VC   = 14,
    SYS  = 15
};

enum class CONV_OP {
    CONV_NORMAL = 0,
    CONV_WRQ = 1,
    CONV_WRQ_RELU = 2
};

enum class PAD_MODE {
    PAD_CONSTANT    = 0,
    PAD_REFLECTION  = 1,
    PAD_REPLICATION = 2,
    PAD_CIRCULAR    = 3
};

enum class LIN_OP {
    LIN_MAC = 1,
    LIN_ADD_SQR = 20,
    LIN_SUB_SQR = 21
};

enum class SFU_OP {
    SFU_TAYLOR_4X = 12,
    SFU_TAYLOR    = 13,
    SFU_NORM      = 15,
    SFU_RSQ       = 17
};

enum class CMP_OP {
    CMP_GT_AND_SG = 22,
    CMP_SG = 23,
    CMP_SE = 24,
    CMP_LT_AND_SL = 25,
    CMP_SL = 26
};

enum class MM_OP {
    MM_NORMAL = 1,
    MM_WRQ = 2,
    MM_WRQ_RELU = 3,
    MM_NN = 4,
    MM_NT = 5,
    MM_TT = 6,
};

enum class AR_OP {
    AR_MUL = 0,
    AR_NOT = 1,
    AR_ADD = 2,
    AR_SUB = 3,
    AR_MAX = 4,
    AR_MIN = 5,
    AR_LOGIC_SHIFT = 6,
    AR_AND = 7,
    AR_OR = 8,
    AR_XOR = 9,
    AR_SG = 10,
    AR_SE = 11,
    AR_DIV = 12,
    AR_SL = 13,
    AR_DATA_CONVERT = 14,
    AR_ADD_SATU = 15,
    AR_SUB_SATU = 16,
    AR_CLAMP = 17,
    AR_MAC = 18,
    AR_COPY = 19,
    AR_MUL_SATU = 20,
    AR_ARITH_SHIFT = 21,
    AR_ROTATE_SHIFT = 22,
    AR_MULDHR = 23,
    AR_EU_IDX_GEN = 24,
    AR_NPU_IDX_GEN = 25,
    AR_ABS = 26,
    AR_FSUBABS = 27,
    AR_COPY_MB = 28,
    AR_GET_FIRST_ONE = 29,
    AR_GET_FIRST_ZERO = 30
};

enum class PD_OP {
    PD_DEPTHWISE = 0,
    PD_AVG = 1,
    PD_DEPTHWISE_RELU = 2,
    PD_MAX = 4,
    PD_ROI_DEPTHWISE = 5,
    PD_ROI_AVG = 6,
    PD_ROI_MAX = 7
};

enum class TRANS_BC_OP {
    TRAN_C_W_TRANSPOSE = 0,
    TRAN_W_C_TRANSPOSE = 1,
    LANE_COPY = 2,
    LANE_BROAD = 3,
    STATIC_BROAD = 4,
    STATIC_DISTRIBUTE = 5,
};

enum class SG_OP {
    PL_gather_d1coor = 0,
    PL_gather_d2coor = 1,
    PL_gather_rec = 2,
    PL_scatter_d1coor = 3,
    PL_scatter_d2coor = 4,
    PE_S_gather_d1coor = 5,
    PE_S_scatter_d1coor = 6,
    PE_M_gather_d1coor = 7,
    PE_S_mask_select = 8,
    PE_S_nonzero = 9,
    PE_S_scatter_pp_d1coor = 10,
    PE_S_gather_hzd = 13,
    PE_S_scatter_hzd = 14,
    PE_S_mask_selhzd = 15,
    PE_S_nonzero_hzd = 16,
    PE_S_gather_line = 17,
    PE_S_scatter_line = 18,
    PE_S_mask_seline = 19,
};

enum class RQDQ_OP {
    RQ_0 = 0,
    RQ_1 = 1,
    DQ_0 = 3,
    DQ_1 = 4,
};

enum class SYS_TYPE {
    INSTR_BARRIER = 0, // no use
    SPB = 1,
    SWR = 2,
    SWR_FROM_LMEM = 3,
    SWR_COL_FROM_LMEM = 4,
    SYNC_ID = 5,
    DATA_BARRIER = 6, // no use
    SYS_END = 31
};

enum class GDMA_TYPE {
  GDMA_TENSOR = 0,
  GDMA_MATRIX = 1,
  GDMA_FILTER = 2,
  GDMA_GENERAL = 3,
  GDMA_CW_TRANS = 4,
  GDMA_NONZERO = 5,
  GDMA_SYS = 6,
  GDMA_GATHER = 7,
  GDMA_SCATTER = 8,
  GDMA_REVERSE = 9,
  GDMA_COMPRESS = 10,
  GDMA_DECOMPRESS = 11,
};

enum class DATA_FORMAT {
    INT8      = 0,
    FLOAT16   = 1,
    FLOAT32   = 2,
    INT16     = 3,
    INT32     = 4,
    BFLOAT16  = 5,
    INT64     = 6
};

enum class STORE_MODE{
    ALIGN_STORE       = 0,
    COMPACT_STORE     = 1,
    BIAS_STORE        = 2,
    STRIDE_STORE      = 3
};

// 为 DATA_FORMAT 指定范围（最小值 0，最大值 6）
template <>
struct magic_enum::customize::enum_range<DATA_FORMAT> {
    static constexpr int min = 0;
    static constexpr int max = 6;
};

// 为 TSK_TYPE 指定范围（最小值 0，最大值 15）
template <>
struct magic_enum::customize::enum_range<TSK_TYPE> {
    static constexpr int min = 0;
    static constexpr int max = 15;
};

// 为 CONV_OP 指定范围（最小值 0，最大值 2）
template <>
struct magic_enum::customize::enum_range<CONV_OP> {
    static constexpr int min = 0;
    static constexpr int max = 2;
};

// 为 PAD_MODE 指定范围（最小值 0，最大值 3）
template <>
struct magic_enum::customize::enum_range<PAD_MODE> {
    static constexpr int min = 0;
    static constexpr int max = 3;
};

// 为 LIN_OP 指定范围（最小值 1，最大值 21）
template <>
struct magic_enum::customize::enum_range<LIN_OP> {
    static constexpr int min = 1;
    static constexpr int max = 21;
};

// 为 SFU_OP 指定范围（最小值 12，最大值 17）
template <>
struct magic_enum::customize::enum_range<SFU_OP> {
    static constexpr int min = 12;
    static constexpr int max = 17;
};

// 为 CMP_OP 指定范围（最小值 22，最大值 26）
template <>
struct magic_enum::customize::enum_range<CMP_OP> {
    static constexpr int min = 22;
    static constexpr int max = 26;
};

// 为 MM_OP 指定范围（最小值 1，最大值 6）
template <>
struct magic_enum::customize::enum_range<MM_OP> {
    static constexpr int min = 1;
    static constexpr int max = 6;
};

// 为 AR_OP 指定范围（最小值 0，最大值 30）
template <>
struct magic_enum::customize::enum_range<AR_OP> {
    static constexpr int min = 0;
    static constexpr int max = 30;
};

// 为 PD_OP 指定范围（最小值 0，最大值 7）
template <>
struct magic_enum::customize::enum_range<PD_OP> {
    static constexpr int min = 0;
    static constexpr int max = 7;
};

// 为 TRANS_BC_OP 指定范围（最小值 0，最大值 5）
template <>
struct magic_enum::customize::enum_range<TRANS_BC_OP> {
    static constexpr int min = 0;
    static constexpr int max = 5;
};

// 为 SG_OP 指定范围（最小值 0，最大值 19）
template <>
struct magic_enum::customize::enum_range<SG_OP> {
    static constexpr int min = 0;
    static constexpr int max = 19;
};

// 为 RQDQ_OP 指定范围（最小值 0，最大值 4）
template <>
struct magic_enum::customize::enum_range<RQDQ_OP> {
    static constexpr int min = 0;
    static constexpr int max = 4;
};

// 为 SYS_TYPE 指定范围（最小值 0，最大值 31）
template <>
struct magic_enum::customize::enum_range<SYS_TYPE> {
    static constexpr int min = 0;
    static constexpr int max = 31;
};

// 为 GDMA_TYPE 指定范围（最小值 0，最大值 11）
template <>
struct magic_enum::customize::enum_range<GDMA_TYPE> {
    static constexpr int min = 0;
    static constexpr int max = 11;
};

// 为 STORE_MODE 指定范围（最小值 0，最大值 3）
template <>
struct magic_enum::customize::enum_range<STORE_MODE> {
    static constexpr int min = 0;
    static constexpr int max = 3;
};
#endif
