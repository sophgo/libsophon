#ifndef OP_CODE_H_
#define OP_CODE_H_

typedef enum align_tensor_op {
    ALIGN_TENSOR_ADD,
    ALIGN_TENSOR_SUB,
    ALIGN_TENSOR_MUL,
    ALIGN_TENSOR_DIV,
    TENSOR_INVALID
} ALIGN_TENSOR_OP;

typedef enum linear_op {
    LINEAR_MAC,
    LINEAR_ADD_SQR,
    LINEAR_SUB_SQR
} LINEAR_OP;

typedef enum sfu_op {
    SFU_XN,
    SFU_EX,
    SFU_LNX,
    SFU_RSQ,
    SFU_NORMA,
    SFU_NORMB,
    SFU_F2I,
    SFU_TAILOR,
    SFU_INVALID
} SFU_OP;

typedef enum {
    VEC_CORR_MUL,            // 0
    VEC_CORR_MAC_INVALID,    // 1
    VEC_CORR_ADD,            // 2
    VEC_CORR_SUB,            // 3
    VEC_CORR_MAX,            // 4
    VEC_CORR_MIN,            // 5
    VEC_CORR_SHIFT_INVALID,  // 6
    VEC_CORR_AND,            // 7
    VEC_CORR_OR,             // 8
    VEC_CORR_XOR,            // 9
    VEC_CORR_SEL_GREAT_INVALID,  // 10
    VEC_CORR_SEL_EQUAL_INVALID,  // 11
    VEC_CORR_DIV             // 12
}VEC_CORR_OP;

typedef struct tensor_4d_t {
    int n;
    int c;
    int h;
    int w;
}bm_tensor_4d_t;

typedef enum {
    UNARY_F32_ABS,
    UNARY_F32_ACOSH,
    UNARY_F32_ARCCOS,
    UNARY_F32_ARCSIN,
    UNARY_F32_ASINH,
    UNARY_F32_ATANH,
    UNARY_F32_CEIL,
    UNARY_F32_COS,
    UNARY_F32_COSH,
    UNARY_F32_COT,
    UNARY_F32_EXP,
    UNARY_F32_EXPM1,
    UNARY_F32_FLOOR,
    UNARY_F32_IPOWER,
    UNARY_F32_SQUARE,
    UNARY_F32_LOG,
    UNARY_F32_LOG1P,
    UNARY_F32_PRELU,
    UNARY_F32_PRELU_N,
    UNARY_F32_RELU,
    UNARY_F32_RELU_N,
    UNARY_F32_ELU,
    UNARY_F32_ROUND,
    UNARY_F32_RSQRT,
    UNARY_F32_SIGMOID,
    UNARY_F32_SIGN,
    UNARY_F32_SIN,
    UNARY_F32_SINH,
    UNARY_F32_SQRT,
    UNARY_F32_TAN,
    UNARY_F32_TANH,
    UNARY_F32_TO_I32,
    UNARY_F32_TRIM,
    UNARY_I32_TO_F32,
    UNARY_U32_TO_F32,
    UNARY_U8_TO_F32
} UNARY_FUNC_TYPE;


typedef enum {
    BINARY_FUNC_ADD,
    BINARY_FUNC_SUB,
    BINARY_FUNC_MUL,
    BINARY_FUNC_DIV,
    BINARY_FUNC_POW,
    BINARY_FUNC_MAXIMUM,
    BINARY_FUNC_MINIMUM,
    BINARY_FUNC_LOGICAL_AND,
    BINARY_FUNC_LOGICAL_OR,
    BINARY_FUNC_EQUAL,
    BINARY_FUNC_NOT_EQUAL,
    BINARY_FUNC_GREATER,
    BINARY_FUNC_GREATER_EQUAL,
    BINARY_FUNC_LESS,
    BINARY_FUNC_LESS_EQUAL,
    BINARY_FUNC_SQUARED_DIFFERENCE,
    BINARY_FUNC_SQRT_GRAD,
    BINARY_FUNC_RSQRT_GRAD,
    BINARY_FUNC_TANH_GRAD,
    BINARY_FUNC_SIGMOID_GRAD,
    BINARY_FUNC_INVERSE_GRAD,
    BINARY_FUNC_CNT
} BINARY_FUNC;

#define TENSOR_MUL 0
#define TENSOR_MAC 1
#define TENSOR_ADD 2
// Note the div should be implmented by KAMAKE algorithm
#define TENSOR_SUB 3
#define TENSOR_MAX 4
#define TENSOR_MIN 5
#define TENSOR_SHIFT 6
// Note new add EU operation
#define TENSOR_AND 7
#define TENSOR_OR 8
#define TENSOR_XOR 9
#define TENSOR_SG 10
#define TENSOR_SE 11
#define TENSOR_DIV 12
#define TENSOR_TAYLOR 13
#define TENSOR_FP32_INT32 14
#define TENSOR_NORMALIZE_INT32 15
#define TENSOR_NORMALIZE_FP32 16
#define TENSOR_RSQRT 17
#define TENSOR_ADDERTREE 18
#define TENSOR_CPY 19
#define TENSOR_SQR_SUM 20
#define TENSOR_SQR_DIFF 21

#define TENSOR_N_DIM 0
#define TENSOR_C_DIM 1
#define TENSOR_H_DIM 2
#define TENSOR_W_DIM 3

#define SHARE_REG_MESSAGE_WP       0
#define SHARE_REG_MESSAGE_RP       1
#define SHARE_REG_MSI_DATA         3
#define SHARE_REG_ARM9_READ_REG_ADDR     8
#define SHARE_REG_FW_STATUS             9
#define SHARE_REG_ARM9FW_LOG_RP             11
#define SHARE_REG_ARM9FW_LOG_WP            12

#define GP_REG14_OFFSET  0xb8
#define GP_REG15_OFFSET 0xbc
#define GP_REG14_SET_OFFSET 0x190
#define GP_REG14_CLEAR_OFFSET 0x194
#define GP_REG15_SET_OFFSET 0x198
#define GP_REG15_CLEAR_OFFSET 0x19c

#define GP_REG_MSG_IRQ_VALUE  0x1

#define SHAREMEM_SIZE_BIT  12  // 4K words
#define SHAREMEM_MASK      ((1 << SHAREMEM_SIZE_BIT) - 1)
#define SHARE_REG_CNT      14

// used for constant_flag
#define CONSTANT_IN_NONE 0
#define CONSTANT_IN_HOST 1
#define CONSTANT_IN_DEVICE 2
#define CONSTANT_MAX_FLAG 2

// used in f32_is
#define F32_INF 0
#define F32_NAN 1
#define F32_FINITE 2

#define REDUCE_F32_MIN 0
#define REDUCE_F32_MAX 1
#define REDUCE_F32_PROD 2

#define SEGMENT_REDUCE_SUM  0
#define SEGMENT_REDUCE_PROD 1
#define SEGMENT_REDUCE_MEAN 2
#define SEGMENT_REDUCE_MAX  3
#define SEGMENT_REDUCE_MIN  4
#define SEGMENT_REDUCE_MAX_INIT0  5
#define SEGMENT_REDUCE_MIN_INIT0  6

#define BM_REDUCE_MEAN 0
#define BM_REDUCE_SUM  1
#define BM_REDUCE_MAX  2
#define BM_REDUCE_MIN  3
#define BM_REDUCE_PROD 4

#define BM_BINARY_ADD 0
#define BM_BINARY_SUB 1
#define BM_BINARY_MUL 2
#define BM_BINARY_DIV 3
#define BM_BINARY_MAX 4
#define BM_BINARY_MIN 10000

#define BM_BINARY_GT 10001
#define BM_BINARY_GE 10002
#define BM_BINARY_LT 10003
#define BM_BINARY_LE 10004
#define BM_BINARY_EQ 10005
#define BM_BINARY_NE 10006
#define BM_BINARY_SQUARED_DIFF 10007
#define BM_BINARY_FLOOR_MOD 10008

#define ACTIVE_ROUND    10  //only for float shape tensor
#define ACTIVE_CEIL     11  //only for float shape tensor
#define ACTIVE_FLOOR    12  //only for float shape tensor

// Channel shift macro(left,right,circle left,circle right)
#define CH_SHIFT_L      0
#define CH_SHIFT_R      1
#define CH_SHIFT_CL     2
#define CH_SHIFT_CR     3

#endif /* OP_CODE_H_ */
