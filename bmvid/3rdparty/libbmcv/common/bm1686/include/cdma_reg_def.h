#ifndef __CDMA_REG_DEF_H__
#define __CDMA_REG_DEF_H__

// cdma reg id defines
#define CDMA_ID_DMA_EN             {0, 1}
#define CDMA_ID_MODE_SELECT        {1, 1}
#define CDMA_ID_SYNC_ID_RESET      {2, 1}
#define CDMA_ID_INT_DISABLE        {3, 1}
#define CDMA_ID_RF_DES_FLUSH       {4, 1}
#define CDMA_ID_TASK_STEP_EN       {32, 1}

#define CDMA_ID_DES_INT_DISABLE    {64, 1}
#define CDMA_ID_INVAL_DES_INT_DISABLE    {65, 1}
#define CDMA_ID_CMD_EPT_INT_DISABLE    {67, 1}
#define CDMA_ID_DATA_PKT_INT_DISABLE    {73, 1}
#define CDMA_ID_INT_STAT           {96, 10}
#define CDMA_ID_DES_INT_STATUS     {96, 1}
#define CDMA_ID_INVAL_DES_INT_STATUS {97, 1}
#define CDMA_ID_CMD_EPT_INT_STATUS {99, 1}
#define CDMA_ID_DATA_PKT_INT_STATUS {105, 1}

#define CDMA_ID_CMD_FIFO_STAT      {128, 8}
#define CDMA_ID_REG_SYNCID         {160, 16}
#define CDMA_ID_CUR_SYNCID         {176, 16}

#define CDMA_ID_CMD_ACCP0          {960, 32}
#define CDMA_ID_DES_VAL_CHECK      {960, 1}
#define CDMA_ID_DES_TYPE           {961, 1}
#define CDMA_ID_EOD                {962, 1}
#define CDMA_ID_INT_ENABLE         {963, 1}
#define CDMA_ID_BARRIER_ENABLE     {964, 1}
#define CDMA_ID_MISC_EN            {965, 1}
#define CDMA_ID_MISC_SELECT        {966, 1}
#define CDMA_ID_MIN_MAX            {976, 1}
#define CDMA_ID_INDEX_ENABLE       {977, 1}
#define CDMA_ID_AUTO_INDEX         {978, 1}

#define CDMA_ID_CMD_ACCP1          {992, 32}
#define CDMA_ID_CMD_ID             {992, 16}
#define CDMA_ID_ENG0_ID             {1008, 16}
#define CDMA_ID_CMD_ACCP2          {1024, 32}
#define CDMA_ID_ENG1_ID             {1024, 16}
#define CDMA_ID_ENG2_ID             {1040, 16}
#define CDMA_ID_CMD_ACCP3          {1056, 32}
#define CDMA_ID_SRC_ADDR_L         {1056, 32}
#define CDMA_ID_CMD_ACCP4          {1088, 32}
#define CDMA_ID_SRC_ADDR_H         {1088, 32}
#define CDMA_ID_CMD_ACCP5          {1120, 32}
#define CDMA_ID_DST_ADDR_L         {1120, 32}
#define CDMA_ID_CMD_ACCP6          {1152, 32}
#define CDMA_ID_DST_ADDR_H          {1152, 32}
#define CDMA_ID_CMD_ACCP7          {1184, 32}
#define CDMA_ID_DMA_AMNT           {1184, 32}
#define CDMA_ID_CMD_ACCP8          {1216, 32}
#define CDMA_ID_CMD_ACCP9          {1248, 32}
#define CDMA_ID_CMD_ACCP10         {1280, 32}
#define CDMA_ID_MISC_INPUT_L       {1280, 32}
#define CDMA_ID_CMD_ACCP11         {1312, 32}
#define CDMA_ID_MISC_INPUT_H       {1312, 32}
#define CDMA_ID_CMD_ACCP12         {1344, 32}
#define CDMA_ID_MISC_OUTPUT_L      {1344, 32}
#define CDMA_ID_CMD_ACCP13         {1376, 32}
#define CDMA_ID_MISC_OUTPUT_H      {1376, 32}
#define CDMA_ID_CMD_ACCP14         {1408, 32}
#define CDMA_ID_MISC_CNT           {1408, 32}
#define CDMA_ID_DES_BASE_L         {2592, 32}
#define CDMA_ID_DES_BASE_H         {2624, 32}
#define CDMA_ID_DES_ADDR_L         {3200, 32}
#define CDMA_ID_DES_ADDR_H         {3232, 32}
#define CDMA_ID_INT_RAW_STAT       {3264, 10}
#define CDMA_ID_RF_CHL_DES_SYNID   {3328, 16}
#define CDMA_ID_MAX_PAYLOAD        {4032, 3}

// cdma pack defines
#define CDMA_PACK_DMA_EN(val)                  {CDMA_ID_DMA_EN, (val)}
#define CDMA_PACK_MODE_SELECT(val)             {CDMA_ID_MODE_SELECT, (val)}
#define CDMA_PACK_SYNC_ID_RESET(val)           {CDMA_ID_SYNC_ID_RESET, (val)}
#define CDMA_PACK_INT_DISABLE(val)             {CDMA_ID_INT_DISABLE, (val)}
#define CDMA_PACK_RF_DES_FLUSH(val)            {CDMA_ID_RF_DES_FLUSH, (val)}
#define CDMA_PACK_TASK_STEP_EN(val)            {CDMA_ID_TASK_STEP_EN, (val)}
#define CDMA_PACK_DES_INT_DISABLE(val)         {CDMA_ID_DES_INT_DISABLE, (val)}
#define CDMA_PACK_INVAL_DES_INT_DISABLE(val)   {CDMA_ID_DES_INT_DISABLE, (val)}
#define CDMA_PACK_CMD_EPT_INT_DISABLE(val)     {CDMA_ID_CMD_EPT_INT_DISABLE, (val)}
#define CDMA_PACK_DATA_PKT_INT_DISABLE(val)    {CDMA_ID_DATA_PKT_INT_DISABLE, (val)}
#define CDMA_PACK_DES_INT_STATUS(val)          {CDMA_ID_DES_INT_STATUS, (val)}
#define CDMA_PACK_INVAL_DES_INT_STATUS(val)    {CDMA_ID_DES_INT_STATUS, (val)}
#define CDMA_PACK_CMD_EPT_INT_STATUS(val)      {CDMA_ID_CMD_EPT_INT_STATUS, (val)}
#define CDMA_PACK_DATA_PKT_INT_STATUS(val)     {CDMA_ID_DATA_PKT_INT_STATUS, (val)}

#define CDMA_PACK_CMD_ACCP0(val)               {CDMA_ID_CMD_ACCP0, (val)}
#define CDMA_PACK_DES_VAL_CHECK(val)           {CDMA_ID_DES_VAL_CHECK, (val)}
#define CDMA_PACK_DES_TYPE(val)                {CDMA_ID_DES_TYPE, (val)}
#define CDMA_PACK_CMD_EOD(val)                 {CDMA_ID_EOD, (val)}
#define CDMA_PACK_INT_ENABLE(val)              {CDMA_ID_INT_ENABLE, (val)}
#define CDMA_PACK_BARRIER_ENABLE(val)          {CDMA_ID_BARRIER_ENABLE, (val)}
#define CDMA_PACK_CMD_MISC_EN(val)             {CDMA_ID_MISC_EN, (val)}
#define CDMA_PACK_CMD_MISC_SELECT(val)         {CDMA_ID_MISC_SELECT, (val)}
#define CDMA_PACK_CMD_MIN_MAX(val)             {CDMA_ID_MIN_MAX, (val)}
#define CDMA_PACK_CMD_INDEX_ENABLE(val)        {CDMA_ID_INDEX_ENABLE, (val)}
#define CDMA_PACK_CMD_AUTO_INDEX(val)          {CDMA_ID_AUTO_INDEX, (val)}

#define CDMA_PACK_CMD_ACCP1(val)   {CDMA_ID_CMD_ACCP1, (val)}
#define CDMA_PACK_CMD_ID(val)      {CDMA_ID_CMD_ID, (val)}
#define CDMA_PACK_ENG0_ID(val)     {CDMA_ID_ENG0_ID, (val)}
#define CDMA_PACK_CMD_ACCP2(val)   {CDMA_ID_CMD_ACCP2, (val)}
#define CDMA_PACK_ENG1_ID(val)     {CDMA_ID_ENG1_ID, (val)}
#define CDMA_PACK_ENG2_ID(val)     {CDMA_ID_ENG2_ID, (val)}

#define CDMA_PACK_CMD_ACCP3(val)         {CDMA_ID_CMD_ACCP3, (val)}
#define CDMA_PACK_CMD_SRC_ADDR_L(val)    {CDMA_ID_SRC_ADDR_L, (val)}
#define CDMA_PACK_CMD_ACCP4(val)         {CDMA_ID_CMD_ACCP4, (val)}
#define CDMA_PACK_CMD_SRC_ADDR_H(val)    {CDMA_ID_SRC_ADDR_H, (val)}
#define CDMA_PACK_CMD_ACCP5(val)         {CDMA_ID_CMD_ACCP5, (val)}
#define CDMA_PACK_CMD_DST_ADDR_L(val)    {CDMA_ID_DST_ADDR_L, (val)}
#define CDMA_PACK_CMD_ACCP6(val)         {CDMA_ID_CMD_ACCP6, (val)}
#define CDMA_PACK_CMD_DST_ADDR_H(val)    {CDMA_ID_DST_ADDR_H, (val)}
#define CDMA_PACK_CMD_ACCP7(val)         {CDMA_ID_CMD_ACCP7, (val)}
#define CDMA_PACK_CMD_DMA_AMNT(val)      {CDMA_ID_DMA_AMNT, (val)}
#define CDMA_PACK_CMD_ACCP8(val)         {CDMA_ID_CMD_ACCP8, (val)}
#define CDMA_PACK_CMD_ACCP9(val)         {CDMA_ID_CMD_ACCP9, (val)}
#define CDMA_PACK_CMD_ACCP10(val)        {CDMA_ID_CMD_ACCP10, (val)}
#define CDMA_PACK_CMD_MISC_INPUT_L(val)  {CDMA_ID_MISC_INPUT_L, (val)}
#define CDMA_PACK_CMD_ACCP11(val)        {CDMA_ID_CMD_ACCP11, (val)}
#define CDMA_PACK_CMD_MISC_INPUT_H(val)  {CDMA_ID_MISC_INPUT_H, (val)}
#define CDMA_PACK_CMD_ACCP12(val)        {CDMA_ID_CMD_ACCP12, (val)}
#define CDMA_PACK_CMD_MISC_OUTPUT_L(val) {CDMA_ID_MISC_OUTPUT_L, (val)}
#define CDMA_PACK_CMD_ACCP13(val)        {CDMA_ID_CMD_ACCP13, (val)}
#define CDMA_PACK_CMD_MISC_OUTPUT_H(val) {CDMA_ID_MISC_OUTPUT_H, (val)}
#define CDMA_PACK_CMD_ACCP14(val)        {CDMA_ID_CMD_ACCP14, (val)}
#define CDMA_PACK_CMD_MISC_CNT(val)      {CDMA_ID_MISC_CNT, (val)}
#define CDMA_PACK_DES_BASE_L(val)        {CDMA_ID_DES_BASE_L, (val)}
#define CDMA_PACK_DES_BASE_H(val)        {CDMA_ID_DES_BASE_H, (val)}
#define CDMA_PACK_MAX_PAYLOAD(val)       {CDMA_ID_MAX_PAYLOAD, (val)}

#define CDMA_PACK_DES_INT_DISABLE_DEFAULT       CDMA_PACK_DES_INT_DISABLE(1)
#define CDMA_PACK_INVAL_DES_INT_DISABLE_DEFAULT CDMA_PACK_INVAL_DES_INT_DISABLE(1)
#define CDMA_PACK_CMD_EPT_INT_DISABLE_DEFAULT   CDMA_PACK_CMD_EPT_INT_DISABLE(1)
#define CDMA_PACK_DATA_PKT_INT_DISABLE_DEFAULT  CDMA_PACK_DATA_PKT_INT_DISABLE(1)

#define CDMA_PACK_DEFAULTS { \
    CDMA_PACK_DES_INT_DISABLE_DEFAULT, \
    CDMA_PACK_INVAL_DES_INT_DISABLE_DEFAULT, \
    CDMA_PACK_CMD_EPT_INT_DISABLE_DEFAULT, \
    CDMA_PACK_DATA_PKT_INT_DISABLE_DEFAULT, \
}

extern int g_cdma_sync_flag;

#endif
