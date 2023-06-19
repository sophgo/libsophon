#ifndef __BD_REG_DEF_H__
#define __BD_REG_DEF_H__

#define BD_REG_COUNT (32)

// bd reg id defines
#define BD_ID_DES_CMD_EN                { 0, 1 }
#define BD_ID_DES_CMD_END               { 1, 1 }
#define BD_ID_DES_CMD_ID_EN             { 2, 1 }
#define BD_ID_DES_CMD_ID_TPU            { 3, 16 }
#define BD_ID_DES_CMD_ID_GDMA           { 19, 16 }
#define BD_ID_DES_CMD_KEEP              { 35, 1 }
#define BD_ID_DES_CMD_INTR_EN           { 36, 1 }
#define BD_ID_DES_TSK_TYP               { 37, 4 }
#define BD_ID_DES_TSK_EU_TYP            { 41, 5 }
#define BD_ID_DES_TSK_OPD_NUM           { 46, 2 }
#define BD_ID_DES_OPT_RIGHT_SHIFT       { 48, 5 }
#define BD_ID_DES_OPT_LEFT_SHIFT        { 53, 5 }
#define BD_ID_DES_OPT_SHIFT_TYP         { 58, 1 }
#define BD_ID_DES_OPT_RES_ADD           { 59, 1 }
#define BD_ID_DES_OPT_RELU              { 60, 1 }
#define BD_ID_DES_OPT_LEFT_TRAN         { 61, 1 }
#define BD_ID_DES_OPT_WINOGRAD          { 62, 1 }
#define BD_ID_DES_OPT_KERNEL_ROTATE     { 63, 1 }
#define BD_ID_DES_OPT_OPD0_SIGN         { 64, 1 }
#define BD_ID_DES_OPT_OPD1_SIGN         { 65, 1 }
#define BD_ID_DES_OPT_OPD2_SIGN         { 66, 1 }
#define BD_ID_DES_OPT_RES0_PREC         { 67, 3 }
#define BD_ID_DES_OPT_OPD0_PREC         { 70, 3 }
#define BD_ID_DES_OPT_OPD1_PREC         { 73, 3 }
#define BD_ID_DES_OPT_OPD2_PREC         { 76, 3 }
#define BD_ID_DES_OPT_OPD0_CONST        { 79, 1 }
#define BD_ID_DES_OPT_OPD1_CONST        { 80, 1 }
#define BD_ID_DES_OPT_OPD2_CONST        { 81, 1 }
#define BD_ID_DES_SHORT_RES0_STR        { 82, 3 }
#define BD_ID_DES_SHORT_OPD0_STR        { 85, 3 }
#define BD_ID_DES_SHORT_OPD1_STR        { 88, 3 }
#define BD_ID_DES_SHORT_OPD2_STR        { 91, 3 }
#define BD_ID_DES_OPD0_X_INS0           { 94, 4 }
#define BD_ID_DES_OPD0_Y_INS0           { 98, 4 }
#define BD_ID_DES_OPD1_X_INS0           { 102, 4 }
#define BD_ID_DES_OPD1_Y_INS0           { 106, 4 }
#define BD_ID_DES_OPD0_UP_PAD           { 110, 4 }
#define BD_ID_DES_OPD0_DN_PAD           { 114, 4 }
#define BD_ID_DES_OPD0_LF_PAD           { 118, 4 }
#define BD_ID_DES_OPD0_RT_PAD           { 122, 4 }
#define BD_ID_DES_RES_OP_X_STR          { 126, 4 }
#define BD_ID_DES_RES_OP_Y_STR          { 130, 4 }
#define BD_ID_DES_TSK_LANE_NUM_0        { 134, 32 }
#define BD_ID_DES_TSK_LANE_NUM_1        { 166, 32 }
#define BD_ID_DES_RES0_N                { 198, 16 }
#define BD_ID_DES_RES0_C                { 214, 12 }
#define BD_ID_DES_RES0_H                { 226, 16 }
#define BD_ID_DES_RES0_W                { 242, 16 }
#define BD_ID_DES_OPD0_N                { 258, 16 }
#define BD_ID_DES_OPD0_C                { 274, 12 }
#define BD_ID_DES_OPD0_H                { 286, 16 }
#define BD_ID_DES_OPD0_W                { 302, 16 }
#define BD_ID_DES_OPD1_N                { 318, 12 }
#define BD_ID_DES_OPD1_C                { 330, 12 }
#define BD_ID_DES_OPD1_H                { 342, 16 }
#define BD_ID_DES_OPD1_W                { 358, 16 }
#define BD_ID_DES_RES0_H_SHIFT          { 374, 4 }
#define BD_ID_DES_RES0_W_SHIFT          { 378, 4 }
#define BD_ID_DES_OPD0_H_SHIFT          { 382, 4 }
#define BD_ID_DES_OPD0_W_SHIFT          { 386, 4 }
#define BD_ID_DES_OPD1_H_SHIFT          { 390, 4 }
#define BD_ID_DES_OPD1_W_SHIFT          { 394, 4 }
#define BD_ID_DES_RES0_N_STR            { 398, 19 }
#define BD_ID_DES_RES0_C_STR            { 417, 19 }
#define BD_ID_DES_OPD0_N_STR            { 436, 19 }
#define BD_ID_DES_OPD0_C_STR            { 455, 19 }
#define BD_ID_DES_OPD1_N_STR            { 474, 19 }
#define BD_ID_DES_OPD1_C_STR            { 493, 19 }
#define BD_ID_DES_OPD2_N_STR            { 512, 19 }
#define BD_ID_DES_OPD2_C_STR            { 531, 19 }
#define BD_ID_RES_ADD_SIGN              { 550, 1 }
#define BD_ID_OPD0_NEQ1                 { 551, 1 }
#define BD_ID_OPD1_NEQ1                 { 552, 1 }
#define BD_ID_DES_OPT_OPD3_CONST        { 553, 1 }
#define BD_ID_DES_RES0_ADDR             { 576, 32 }
#define BD_ID_DES_OPD0_ADDR             { 608, 32 }
#define BD_ID_DES_OPD1_ADDR             { 640, 32 }
#define BD_ID_DES_OPD2_ADDR             { 672, 32 }
#define BD_ID_DES_RES0_H_STR            { 704, 32 }
#define BD_ID_DES_RES0_W_STR            { 736, 32 }
#define BD_ID_DES_OPD0_H_STR            { 768, 32 }
#define BD_ID_DES_OPD0_W_STR            { 800, 32 }
#define BD_ID_DES_OPD1_H_STR            { 832, 32 }
#define BD_ID_DES_OPD1_W_STR            { 864, 32 }
#define BD_ID_DES_OPD2_H_STR            { 896, 32 }
#define BD_ID_DES_OPD2_W_STR            { 928, 32 }
#define BD_ID_DES_RES1_ADDR             { 960, 32 }
#define BD_ID_DES_OPD3_ADDR             { 992, 32 }
// bd pack defines
#define BD_PACK_DES_CMD_EN(val)              { BD_ID_DES_CMD_EN, (val) }
#define BD_PACK_DES_CMD_END(val)             { BD_ID_DES_CMD_END, (val) }
#define BD_PACK_DES_CMD_ID_EN(val)           { BD_ID_DES_CMD_ID_EN, (val) }
#define BD_PACK_DES_CMD_ID_TPU(val)          { BD_ID_DES_CMD_ID_TPU, (val) }
#define BD_PACK_DES_CMD_ID_GDMA(val)         { BD_ID_DES_CMD_ID_GDMA, (val) }
#define BD_PACK_DES_CMD_KEEP(val)            { BD_ID_DES_CMD_KEEP, (val) }
#define BD_PACK_DES_CMD_INTR_EN(val)         { BD_ID_DES_CMD_INTR_EN, (val) }
#define BD_PACK_DES_TSK_TYP(val)             { BD_ID_DES_TSK_TYP, (val) }
#define BD_PACK_DES_TSK_EU_TYP(val)          { BD_ID_DES_TSK_EU_TYP, (val) }
#define BD_PACK_DES_TSK_OPD_NUM(val)         { BD_ID_DES_TSK_OPD_NUM, (val) }
#define BD_PACK_DES_OPT_RIGHT_SHIFT(val)     { BD_ID_DES_OPT_RIGHT_SHIFT, (val) }
#define BD_PACK_DES_OPT_LEFT_SHIFT(val)      { BD_ID_DES_OPT_LEFT_SHIFT, (val) }
#define BD_PACK_DES_OPT_SHIFT_TYP(val)       { BD_ID_DES_OPT_SHIFT_TYP, (val) }
#define BD_PACK_DES_OPT_RES_ADD(val)         { BD_ID_DES_OPT_RES_ADD, (val) }
#define BD_PACK_DES_OPT_RELU(val)            { BD_ID_DES_OPT_RELU, (val) }
#define BD_PACK_DES_OPT_LEFT_TRAN(val)       { BD_ID_DES_OPT_LEFT_TRAN, (val) }
#define BD_PACK_DES_OPT_WINOGRAD(val)        { BD_ID_DES_OPT_WINOGRAD, (val) }
#define BD_PACK_DES_OPT_KERNEL_ROTATE(val)   { BD_ID_DES_OPT_KERNEL_ROTATE, (val) }
#define BD_PACK_DES_OPT_OPD0_SIGN(val)       { BD_ID_DES_OPT_OPD0_SIGN, (val) }
#define BD_PACK_DES_OPT_OPD1_SIGN(val)       { BD_ID_DES_OPT_OPD1_SIGN, (val) }
#define BD_PACK_DES_OPT_OPD2_SIGN(val)       { BD_ID_DES_OPT_OPD2_SIGN, (val) }
#define BD_PACK_DES_OPT_RES0_PREC(val)       { BD_ID_DES_OPT_RES0_PREC, (val) }
#define BD_PACK_DES_OPT_OPD0_PREC(val)       { BD_ID_DES_OPT_OPD0_PREC, (val) }
#define BD_PACK_DES_OPT_OPD1_PREC(val)       { BD_ID_DES_OPT_OPD1_PREC, (val) }
#define BD_PACK_DES_OPT_OPD2_PREC(val)       { BD_ID_DES_OPT_OPD2_PREC, (val) }
#define BD_PACK_DES_OPT_OPD0_CONST(val)      { BD_ID_DES_OPT_OPD0_CONST, (val) }
#define BD_PACK_DES_OPT_OPD1_CONST(val)      { BD_ID_DES_OPT_OPD1_CONST, (val) }
#define BD_PACK_DES_OPT_OPD2_CONST(val)      { BD_ID_DES_OPT_OPD2_CONST, (val) }
#define BD_PACK_DES_OPT_OPD3_CONST(val)      { BD_ID_DES_OPT_OPD3_CONST, (val) }
#define BD_PACK_DES_SHORT_RES0_STR(val)      { BD_ID_DES_SHORT_RES0_STR, (val) }
#define BD_PACK_DES_SHORT_OPD0_STR(val)      { BD_ID_DES_SHORT_OPD0_STR, (val) }
#define BD_PACK_DES_SHORT_OPD1_STR(val)      { BD_ID_DES_SHORT_OPD1_STR, (val) }
#define BD_PACK_DES_SHORT_OPD2_STR(val)      { BD_ID_DES_SHORT_OPD2_STR, (val) }
#define BD_PACK_DES_OPD0_X_INS0(val)         { BD_ID_DES_OPD0_X_INS0, (val) }
#define BD_PACK_DES_OPD0_Y_INS0(val)         { BD_ID_DES_OPD0_Y_INS0, (val) }
#define BD_PACK_DES_OPD1_X_INS0(val)         { BD_ID_DES_OPD1_X_INS0, (val) }
#define BD_PACK_DES_OPD1_Y_INS0(val)         { BD_ID_DES_OPD1_Y_INS0, (val) }
#define BD_PACK_DES_OPD0_UP_PAD(val)         { BD_ID_DES_OPD0_UP_PAD, (val) }
#define BD_PACK_DES_OPD0_DN_PAD(val)         { BD_ID_DES_OPD0_DN_PAD, (val) }
#define BD_PACK_DES_OPD0_LF_PAD(val)         { BD_ID_DES_OPD0_LF_PAD, (val) }
#define BD_PACK_DES_OPD0_RT_PAD(val)         { BD_ID_DES_OPD0_RT_PAD, (val) }
#define BD_PACK_DES_RES_OP_X_STR(val)        { BD_ID_DES_RES_OP_X_STR, (val) }
#define BD_PACK_DES_RES_OP_Y_STR(val)        { BD_ID_DES_RES_OP_Y_STR, (val) }
#define BD_PACK_DES_TSK_LANE_NUM_0(val)      { BD_ID_DES_TSK_LANE_NUM_0, (val) }
#define BD_PACK_DES_TSK_LANE_NUM_1(val)      { BD_ID_DES_TSK_LANE_NUM_1, (val) }
#define BD_PACK_DES_RES0_N(val)              { BD_ID_DES_RES0_N, (val) }
#define BD_PACK_DES_RES0_C(val)              { BD_ID_DES_RES0_C, (val) }
#define BD_PACK_DES_RES0_H(val)              { BD_ID_DES_RES0_H, (val) }
#define BD_PACK_DES_RES0_W(val)              { BD_ID_DES_RES0_W, (val) }
#define BD_PACK_DES_OPD0_N(val)              { BD_ID_DES_OPD0_N, (val) }
#define BD_PACK_DES_OPD0_C(val)              { BD_ID_DES_OPD0_C, (val) }
#define BD_PACK_DES_OPD0_H(val)              { BD_ID_DES_OPD0_H, (val) }
#define BD_PACK_DES_OPD0_W(val)              { BD_ID_DES_OPD0_W, (val) }
#define BD_PACK_DES_OPD1_N(val)              { BD_ID_DES_OPD1_N, (val) }
#define BD_PACK_DES_OPD1_C(val)              { BD_ID_DES_OPD1_C, (val) }
#define BD_PACK_DES_OPD1_H(val)              { BD_ID_DES_OPD1_H, (val) }
#define BD_PACK_DES_OPD1_W(val)              { BD_ID_DES_OPD1_W, (val) }
#define BD_PACK_DES_RES0_H_SHIFT(val)        { BD_ID_DES_RES0_H_SHIFT, (val) }
#define BD_PACK_DES_RES0_W_SHIFT(val)        { BD_ID_DES_RES0_W_SHIFT, (val) }
#define BD_PACK_DES_OPD0_H_SHIFT(val)        { BD_ID_DES_OPD0_H_SHIFT, (val) }
#define BD_PACK_DES_OPD0_W_SHIFT(val)        { BD_ID_DES_OPD0_W_SHIFT, (val) }
#define BD_PACK_DES_OPD1_H_SHIFT(val)        { BD_ID_DES_OPD1_H_SHIFT, (val) }
#define BD_PACK_DES_OPD1_W_SHIFT(val)        { BD_ID_DES_OPD1_W_SHIFT, (val) }
#define BD_PACK_DES_RES0_N_STR(val)          { BD_ID_DES_RES0_N_STR, (val) }
#define BD_PACK_DES_RES0_C_STR(val)          { BD_ID_DES_RES0_C_STR, (val) }
#define BD_PACK_DES_OPD0_N_STR(val)          { BD_ID_DES_OPD0_N_STR, (val) }
#define BD_PACK_DES_OPD0_C_STR(val)          { BD_ID_DES_OPD0_C_STR, (val) }
#define BD_PACK_DES_OPD1_N_STR(val)          { BD_ID_DES_OPD1_N_STR, (val) }
#define BD_PACK_DES_OPD1_C_STR(val)          { BD_ID_DES_OPD1_C_STR, (val) }
#define BD_PACK_DES_OPD2_N_STR(val)          { BD_ID_DES_OPD2_N_STR, (val) }
#define BD_PACK_RES_ADD_SIGN(val)            { BD_ID_RES_ADD_SIGN, (val) }
#define BD_PACK_OPD0_NEQ1(val)               { BD_ID_OPD0_NEQ1, (val) }
#define BD_PACK_OPD1_NEQ1(val)               { BD_ID_OPD1_NEQ1, (val) }
#define BD_PACK_DES_OPD2_C_STR(val)          { BD_ID_DES_OPD2_C_STR, (val) }
#define BD_PACK_DES_RES0_ADDR(val)           { BD_ID_DES_RES0_ADDR, (val) }
#define BD_PACK_DES_RES1_ADDR(val)           { BD_ID_DES_RES1_ADDR, (val) }
#define BD_PACK_DES_OPD0_ADDR(val)           { BD_ID_DES_OPD0_ADDR, (val) }
#define BD_PACK_DES_OPD1_ADDR(val)           { BD_ID_DES_OPD1_ADDR, (val) }
#define BD_PACK_DES_OPD2_ADDR(val)           { BD_ID_DES_OPD2_ADDR, (val) }
#define BD_PACK_DES_OPD3_ADDR(val)           { BD_ID_DES_OPD3_ADDR, (val) }
#define BD_PACK_DES_RES0_H_STR(val)          { BD_ID_DES_RES0_H_STR, (val) }
#define BD_PACK_DES_RES0_W_STR(val)          { BD_ID_DES_RES0_W_STR, (val) }
#define BD_PACK_DES_OPD0_H_STR(val)          { BD_ID_DES_OPD0_H_STR, (val) }
#define BD_PACK_DES_OPD0_W_STR(val)          { BD_ID_DES_OPD0_W_STR, (val) }
#define BD_PACK_DES_OPD1_H_STR(val)          { BD_ID_DES_OPD1_H_STR, (val) }
#define BD_PACK_DES_OPD1_W_STR(val)          { BD_ID_DES_OPD1_W_STR, (val) }
#define BD_PACK_DES_OPD2_H_STR(val)          { BD_ID_DES_OPD2_H_STR, (val) }
#define BD_PACK_DES_OPD2_W_STR(val)          { BD_ID_DES_OPD2_W_STR, (val) }

// bd default values
#define BD_PACK_DES_TSK_OPD_NUM_DEFAULT         BD_PACK_DES_TSK_OPD_NUM(0x2)
#define BD_PACK_DES_OPT_SHIFT_TYP_DEFAULT       BD_PACK_DES_OPT_SHIFT_TYP(0x1)
#define BD_PACK_DES_OPT_RELU_DEFAULT            BD_PACK_DES_OPT_RELU(0x1)
#define BD_PACK_DES_OPT_OPD1_SIGN_DEFAULT       BD_PACK_DES_OPT_OPD1_SIGN(0x1)
#define BD_PACK_DES_OPT_OPD2_SIGN_DEFAULT       BD_PACK_DES_OPT_OPD2_SIGN(0x1)
#define BD_PACK_DES_OPT_RES0_PREC_DEFAULT       BD_PACK_DES_OPT_RES0_PREC(0x2)
#define BD_PACK_DES_OPT_OPD0_PREC_DEFAULT       BD_PACK_DES_OPT_OPD0_PREC(0x2)
#define BD_PACK_DES_OPT_OPD1_PREC_DEFAULT       BD_PACK_DES_OPT_OPD1_PREC(0x2)
#define BD_PACK_DES_OPT_OPD2_PREC_DEFAULT       BD_PACK_DES_OPT_OPD2_PREC(0x2)
#define BD_PACK_DES_SHORT_OPD2_STR_DEFAULT      BD_PACK_DES_SHORT_OPD2_STR(0x2)
#define BD_PACK_DES_RES_OP_X_STR_DEFAULT        BD_PACK_DES_RES_OP_X_STR(0x1)
#define BD_PACK_DES_RES_OP_Y_STR_DEFAULT        BD_PACK_DES_RES_OP_Y_STR(0x1)
#define BD_PACK_DES_TSK_LANE_NUM_0_DEFAULT      BD_PACK_DES_TSK_LANE_NUM_0(0xffffffffL)
#define BD_PACK_DES_TSK_LANE_NUM_1_DEFAULT      BD_PACK_DES_TSK_LANE_NUM_1(0xffffffffL)
#define BD_PACK_DES_RES0_N_DEFAULT              BD_PACK_DES_RES0_N(0x1)
#define BD_PACK_DES_RES0_C_DEFAULT              BD_PACK_DES_RES0_C(0x1)
#define BD_PACK_DES_RES0_H_DEFAULT              BD_PACK_DES_RES0_H(0x1)
#define BD_PACK_DES_RES0_W_DEFAULT              BD_PACK_DES_RES0_W(0x1)
#define BD_PACK_DES_OPD0_N_DEFAULT              BD_PACK_DES_OPD0_N(0x1)
#define BD_PACK_DES_OPD0_C_DEFAULT              BD_PACK_DES_OPD0_C(0x1)
#define BD_PACK_DES_OPD0_H_DEFAULT              BD_PACK_DES_OPD0_H(0x1)
#define BD_PACK_DES_OPD0_W_DEFAULT              BD_PACK_DES_OPD0_W(0x1)
#define BD_PACK_DES_OPD1_N_DEFAULT              BD_PACK_DES_OPD1_N(0x1)
#define BD_PACK_DES_OPD1_C_DEFAULT              BD_PACK_DES_OPD1_C(0x1)
#define BD_PACK_DES_OPD1_H_DEFAULT              BD_PACK_DES_OPD1_H(0x1)
#define BD_PACK_DES_OPD1_W_DEFAULT              BD_PACK_DES_OPD1_W(0x1)
#define BD_PACK_DES_RES0_N_STR_DEFAULT          BD_PACK_DES_RES0_N_STR(0x1)
#define BD_PACK_DES_RES0_C_STR_DEFAULT          BD_PACK_DES_RES0_C_STR(0x1)
#define BD_PACK_DES_OPD0_N_STR_DEFAULT          BD_PACK_DES_OPD0_N_STR(0x1)
#define BD_PACK_DES_OPD0_C_STR_DEFAULT          BD_PACK_DES_OPD0_C_STR(0x1)
#define BD_PACK_DES_OPD1_N_STR_DEFAULT          BD_PACK_DES_OPD1_N_STR(0x1)
#define BD_PACK_DES_OPD1_C_STR_DEFAULT          BD_PACK_DES_OPD1_C_STR(0x1)
#define BD_PACK_DES_OPD2_N_STR_DEFAULT          BD_PACK_DES_OPD2_N_STR(0x1)
#define BD_PACK_DES_OPD2_C_STR_DEFAULT          BD_PACK_DES_OPD2_C_STR(0x1)
#define BD_PACK_DES_RES0_H_STR_DEFAULT          BD_PACK_DES_RES0_H_STR(0x1)
#define BD_PACK_DES_RES0_W_STR_DEFAULT          BD_PACK_DES_RES0_W_STR(0x1)
#define BD_PACK_DES_OPD0_H_STR_DEFAULT          BD_PACK_DES_OPD0_H_STR(0x1)
#define BD_PACK_DES_OPD0_W_STR_DEFAULT          BD_PACK_DES_OPD0_W_STR(0x1)
#define BD_PACK_DES_OPD1_H_STR_DEFAULT          BD_PACK_DES_OPD1_H_STR(0x1)
#define BD_PACK_DES_OPD1_W_STR_DEFAULT          BD_PACK_DES_OPD1_W_STR(0x1)
#define BD_PACK_DES_OPD2_H_STR_DEFAULT          BD_PACK_DES_OPD2_H_STR(0x1)
#define BD_PACK_DES_OPD2_W_STR_DEFAULT          BD_PACK_DES_OPD2_W_STR(0x1)

// default list
#define BD_PACK_DEFAULTS {  \
  BD_PACK_DES_TSK_OPD_NUM_DEFAULT, \
  BD_PACK_DES_OPT_SHIFT_TYP_DEFAULT, \
  BD_PACK_DES_OPT_RELU_DEFAULT, \
  BD_PACK_DES_OPT_OPD1_SIGN_DEFAULT, \
  BD_PACK_DES_OPT_OPD2_SIGN_DEFAULT, \
  BD_PACK_DES_OPT_RES0_PREC_DEFAULT, \
  BD_PACK_DES_OPT_OPD0_PREC_DEFAULT, \
  BD_PACK_DES_OPT_OPD1_PREC_DEFAULT, \
  BD_PACK_DES_OPT_OPD2_PREC_DEFAULT, \
  BD_PACK_DES_SHORT_OPD2_STR_DEFAULT, \
  BD_PACK_DES_RES_OP_X_STR_DEFAULT, \
  BD_PACK_DES_RES_OP_Y_STR_DEFAULT, \
  BD_PACK_DES_TSK_LANE_NUM_0_DEFAULT, \
  BD_PACK_DES_TSK_LANE_NUM_1_DEFAULT, \
  BD_PACK_DES_RES0_N_DEFAULT, \
  BD_PACK_DES_RES0_C_DEFAULT, \
  BD_PACK_DES_RES0_H_DEFAULT, \
  BD_PACK_DES_RES0_W_DEFAULT, \
  BD_PACK_DES_OPD0_N_DEFAULT, \
  BD_PACK_DES_OPD0_C_DEFAULT, \
  BD_PACK_DES_OPD0_H_DEFAULT, \
  BD_PACK_DES_OPD0_W_DEFAULT, \
  BD_PACK_DES_OPD1_N_DEFAULT, \
  BD_PACK_DES_OPD1_C_DEFAULT, \
  BD_PACK_DES_OPD1_H_DEFAULT, \
  BD_PACK_DES_OPD1_W_DEFAULT, \
  BD_PACK_DES_RES0_N_STR_DEFAULT, \
  BD_PACK_DES_RES0_C_STR_DEFAULT, \
  BD_PACK_DES_OPD0_N_STR_DEFAULT, \
  BD_PACK_DES_OPD0_C_STR_DEFAULT, \
  BD_PACK_DES_OPD1_N_STR_DEFAULT, \
  BD_PACK_DES_OPD1_C_STR_DEFAULT, \
  BD_PACK_DES_OPD2_N_STR_DEFAULT, \
  BD_PACK_DES_OPD2_C_STR_DEFAULT, \
  BD_PACK_DES_RES0_H_STR_DEFAULT, \
  BD_PACK_DES_RES0_W_STR_DEFAULT, \
  BD_PACK_DES_OPD0_H_STR_DEFAULT, \
  BD_PACK_DES_OPD0_W_STR_DEFAULT, \
  BD_PACK_DES_OPD1_H_STR_DEFAULT, \
  BD_PACK_DES_OPD1_W_STR_DEFAULT, \
  BD_PACK_DES_OPD2_H_STR_DEFAULT, \
  BD_PACK_DES_OPD2_W_STR_DEFAULT \
}

#endif  // __BD_REG_DEF_H__
