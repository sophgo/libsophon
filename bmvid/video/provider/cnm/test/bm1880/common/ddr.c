#include <stdint.h>

#define ACT_PARITY_ERROR_INJECT_BIT    23
#define ACT_ROW_WIDTH                  17
#define ADDITIVE_LAT_F0_OFFSET         16
#define ADDITIVE_LAT_F0_WIDTH          6
#define ADDITIVE_LAT_F1_OFFSET         16
#define ADDITIVE_LAT_F1_WIDTH          6
#define ADDITIVE_LAT_F2_OFFSET         16
#define ADDITIVE_LAT_F2_WIDTH          6
#define ADDITIVE_LAT_WIDTH             6
#define ADDR_CMP_EN_OFFSET             0
#define ADDR_CMP_EN_WIDTH              1
#define ADDR_COLLISION_MPM_DIS_OFFSET  8
#define ADDR_COLLISION_MPM_DIS_WIDTH   1
#define ADDR_SPACE_OFFSET              24
#define ADDR_SPACE_WIDTH               6
#define ADDRESS_MIRRORING_OFFSET       8
#define ADDRESS_MIRRORING_WIDTH        2
#define ADDRESS_P                      4
#define ADDRESS_WIDTH                  24
#define AGE_COUNT_OFFSET               16
#define AGE_COUNT_WIDTH                8
#define AGE_P                          231
#define AGEWIDTH                       8
#define AP_ADDR                        71
#define AP_BIT                         5
#define AP_OFFSET                      16
#define AP_WIDTH                       1
#define APREBIT_CS_WIDTH               5
#define APREBIT_OFFSET                 8
#define APREBIT_WIDTH                  5
#define ARB_CMD_Q_THRESHOLD_OFFSET     0
#define ARB_CMD_Q_THRESHOLD_WIDTH      4
#define AREF_CMD_MAX_PER_TREFI_OFFSET  16
#define AREF_CMD_MAX_PER_TREFI_WIDTH   4
#define AREF_HIGH_THRESHOLD_OFFSET     24
#define AREF_HIGH_THRESHOLD_WIDTH      5
#define AREF_MAX_CMD_QUE               16
#define AREF_MAX_CREDIT_OFFSET         8
#define AREF_MAX_CREDIT_WIDTH          5
#define AREF_MAX_DEFICIT_OFFSET        0
#define AREF_MAX_DEFICIT_WIDTH         5
#define AREF_NORM_THRESHOLD_OFFSET     16
#define AREF_NORM_THRESHOLD_WIDTH      5
#define AREF_PBR_CONT_DIS_THRESHOLD_OFFSET 8
#define AREF_PBR_CONT_DIS_THRESHOLD_WIDTH 5
#define AREF_PBR_CONT_EN_THRESHOLD_OFFSET 0
#define AREF_PBR_CONT_EN_THRESHOLD_WIDTH 5
#define AREF_STATUS_BIT                40
#define AREF_STATUS_OFFSET             16
#define AREF_STATUS_WIDTH              2
#define AREFRESH_ADDR                  77
#define AREFRESH_OFFSET                8
#define AREFRESH_WIDTH                 1
#define ASYNC_CDC_STAGES_OFFSET        24
#define ASYNC_CDC_STAGES_WIDTH         8
#define ASYNCHRONOUS_RESET             1
#define AUTO_MR_DATA_WIDTH             16
#define AUTO_TEMPCHK_VAL_0_OFFSET      8
#define AUTO_TEMPCHK_VAL_0_WIDTH       16
#define AUTO_TEMPCHK_VAL_1_OFFSET      0
#define AUTO_TEMPCHK_VAL_1_WIDTH       16
#define AUTO_TEMPCHK_VAL_WIDTH         16
#define AXI0_ALL_STROBES_USED_ENABLE_OFFSET 0
#define AXI0_ALL_STROBES_USED_ENABLE_WIDTH 1
#define AXI0_BDW_OFFSET                8
#define AXI0_BDW_OVFLOW_OFFSET         16
#define AXI0_BDW_OVFLOW_WIDTH          1
#define AXI0_BDW_WIDTH                 7
#define AXI0_CMDFIFO_DEPTH             8
#define AXI0_CMDFIFO_DEPTH_WIDTH       3
#define AXI0_CMDFIFO_LOG2_DEPTH_OFFSET 0
#define AXI0_CMDFIFO_LOG2_DEPTH_WIDTH  8
#define AXI0_CMDFIFO_PTR_WIDTH         AXI0_CMDFIFO_LOG2_DEPTH + 1
#define AXI0_CMDFIFO_WIDTH             88
#define AXI0_CURRENT_BDW_ADDR          461
#define AXI0_CURRENT_BDW_OFFSET        24
#define AXI0_CURRENT_BDW_WIDTH         7
#define AXI0_DB_RESPFIFO_PTR_WIDTH     5
#define AXI0_DB_RESPFIFO_WIDTH         18
#define AXI0_DB_SPLIT_TRFIFO_PTR_WIDTH 5
#define AXI0_DB_SPLIT_TRFIFO_WIDTH     17
#define AXI0_END_RESP                  20
#define AXI0_END_RESP_BUFFERABLE       16
#define AXI0_END_RESP_CMD_AVAIL        17
#define AXI0_END_RESP_COBUF            22
#define AXI0_END_RESP_DB_PORT_LASTDATA 23
#define AXI0_END_RESP_EXPECTED_SPLIT_CNT 25
#define AXI0_END_RESP_ID               0
#define AXI0_END_RESP_MEM_LASTDATA     19
#define AXI0_END_RESP_PORT_LASTDATA    18
#define AXI0_END_RESP_RCV_SPLIT_CNT    30
#define AXI0_END_RESP_SPLIT_CMD_DONE   24
#define AXI0_FIFO_TYPE_REG_OFFSET      0
#define AXI0_FIFO_TYPE_REG_WIDTH       2
#define AXI0_FIXED_PORT_PRIORITY_ENABLE_OFFSET 8
#define AXI0_FIXED_PORT_PRIORITY_ENABLE_WIDTH 1
#define AXI0_PORT_COBUFFIFO_DEPTH      AXI0_CMDFIFO_DEPTH + 16 + 2
#define AXI0_PORT_COBUFFIFO_WIDTH      16
#define AXI0_PORT_RMODW_NUM_BEATS      16
#define AXI0_PORT_SPLIT_BLOCKS_ADDR_WIDTH 12
#define AXI0_PORT_SPLIT_MAX_NUM_BLOCKS_WIDTH 5
#define AXI0_R_PRIORITY_OFFSET         16
#define AXI0_R_PRIORITY_WIDTH          4
#define AXI0_RATE_SEL_WIDTH            4
#define AXI0_RDFIFO_LOG2_DEPTH_OFFSET  8
#define AXI0_RDFIFO_LOG2_DEPTH_WIDTH   8
#define AXI0_RDFIFO_PTR_WIDTH          AXI0_RDFIFO_LOG2_DEPTH + 1
#define AXI0_RDFIFO_WIDTH              152
#define AXI0_RESP_RCV_SPLIT_CNT_WIDTH  5
#define AXI0_RESP_STORAGE_DEPTH        16
#define AXI0_RESP_STORAGE_PTR_WIDTH    4
#define AXI0_RESP_STORAGE_WIDTH        35
#define AXI0_RMODWFIFO_DEPTH           AXI0_CMDFIFO_DEPTH
#define AXI0_RMODWFIFO_PTR_WIDTH       AXI0_CMDFIFO_PTR_WIDTH
#define AXI0_RMODWFIFO_WIDTH           25
#define AXI0_START_RESP                21
#define AXI0_START_RESP_BUFFERABLE     16
#define AXI0_START_RESP_CMD_AVAIL      17
#define AXI0_START_RESP_COBUF          22
#define AXI0_START_RESP_DB_PORT_LASTDATA 23
#define AXI0_START_RESP_EXPECTED_SPLIT_CNT 29
#define AXI0_START_RESP_ID             15
#define AXI0_START_RESP_MEM_LASTDATA   19
#define AXI0_START_RESP_PORT_LASTDATA  18
#define AXI0_START_RESP_RCV_SPLIT_CNT  34
#define AXI0_START_RESP_SPLIT_CMD_DONE 24
#define AXI0_STATS_REP_PERIOD_WIDTH    4
#define AXI0_TRANS_WRFIFO_LOG2_DEPTH_OFFSET 24
#define AXI0_TRANS_WRFIFO_LOG2_DEPTH_WIDTH 8
#define AXI0_TRANS_WRFIFO_PTR_WIDTH    AXI0_TRANS_WRFIFO_LOG2_DEPTH + 1
#define AXI0_TRANS_WRFIFO_WIDTH        146
#define AXI0_W_PRIORITY_OFFSET         24
#define AXI0_W_PRIORITY_WIDTH          4
#define AXI0_WR_ARRAY_LOG2_DEPTH_OFFSET 16
#define AXI0_WR_ARRAY_LOG2_DEPTH_WIDTH 8
#define AXI0_WR_ARRAY_PTR_WIDTH        AXI0_WR_ARRAY_LOG2_DEPTH
#define AXI0_WR_ARRAY_WIDTH            145
#define AXI0_WRCMD_PROC_FIFO_LOG2_DEPTH_OFFSET 0
#define AXI0_WRCMD_PROC_FIFO_LOG2_DEPTH_WIDTH 8
#define AXI0_WRCMD_PROC_FIFO_PTR_WIDTH AXI0_WRCMD_PROC_FIFO_LOG2_DEPTH + 1
#define AXI0_WRCMD_PROC_FIFO_WIDTH     19
#define AXI1_ALL_STROBES_USED_ENABLE_OFFSET 8
#define AXI1_ALL_STROBES_USED_ENABLE_WIDTH 1
#define AXI1_BDW_OFFSET                0
#define AXI1_BDW_OVFLOW_OFFSET         8
#define AXI1_BDW_OVFLOW_WIDTH          1
#define AXI1_BDW_WIDTH                 7
#define AXI1_CMDFIFO_DEPTH             8
#define AXI1_CMDFIFO_DEPTH_WIDTH       3
#define AXI1_CMDFIFO_LOG2_DEPTH_OFFSET 8
#define AXI1_CMDFIFO_LOG2_DEPTH_WIDTH  8
#define AXI1_CMDFIFO_PTR_WIDTH         AXI1_CMDFIFO_LOG2_DEPTH + 1
#define AXI1_CMDFIFO_WIDTH             88
#define AXI1_CURRENT_BDW_ADDR          462
#define AXI1_CURRENT_BDW_OFFSET        16
#define AXI1_CURRENT_BDW_WIDTH         7
#define AXI1_DB_RESPFIFO_PTR_WIDTH     5
#define AXI1_DB_RESPFIFO_WIDTH         18
#define AXI1_DB_SPLIT_TRFIFO_PTR_WIDTH 5
#define AXI1_DB_SPLIT_TRFIFO_WIDTH     17
#define AXI1_END_RESP                  20
#define AXI1_END_RESP_BUFFERABLE       16
#define AXI1_END_RESP_CMD_AVAIL        17
#define AXI1_END_RESP_COBUF            22
#define AXI1_END_RESP_DB_PORT_LASTDATA 23
#define AXI1_END_RESP_EXPECTED_SPLIT_CNT 25
#define AXI1_END_RESP_ID               0
#define AXI1_END_RESP_MEM_LASTDATA     19
#define AXI1_END_RESP_PORT_LASTDATA    18
#define AXI1_END_RESP_RCV_SPLIT_CNT    30
#define AXI1_END_RESP_SPLIT_CMD_DONE   24
#define AXI1_FIFO_TYPE_REG_OFFSET      8
#define AXI1_FIFO_TYPE_REG_WIDTH       2
#define AXI1_FIXED_PORT_PRIORITY_ENABLE_OFFSET 16
#define AXI1_FIXED_PORT_PRIORITY_ENABLE_WIDTH 1
#define AXI1_PORT_COBUFFIFO_DEPTH      AXI1_CMDFIFO_DEPTH + 16 + 2
#define AXI1_PORT_COBUFFIFO_WIDTH      16
#define AXI1_PORT_RMODW_NUM_BEATS      16
#define AXI1_PORT_SPLIT_BLOCKS_ADDR_WIDTH 12
#define AXI1_PORT_SPLIT_MAX_NUM_BLOCKS_WIDTH 5
#define AXI1_R_PRIORITY_OFFSET         24
#define AXI1_R_PRIORITY_WIDTH          4
#define AXI1_RATE_SEL_WIDTH            4
#define AXI1_RDFIFO_LOG2_DEPTH_OFFSET  16
#define AXI1_RDFIFO_LOG2_DEPTH_WIDTH   8
#define AXI1_RDFIFO_PTR_WIDTH          AXI1_RDFIFO_LOG2_DEPTH + 1
#define AXI1_RDFIFO_WIDTH              152
#define AXI1_RESP_RCV_SPLIT_CNT_WIDTH  5
#define AXI1_RESP_STORAGE_DEPTH        16
#define AXI1_RESP_STORAGE_PTR_WIDTH    4
#define AXI1_RESP_STORAGE_WIDTH        35
#define AXI1_RMODWFIFO_DEPTH           AXI1_CMDFIFO_DEPTH
#define AXI1_RMODWFIFO_PTR_WIDTH       AXI1_CMDFIFO_PTR_WIDTH
#define AXI1_RMODWFIFO_WIDTH           25
#define AXI1_START_RESP                21
#define AXI1_START_RESP_BUFFERABLE     16
#define AXI1_START_RESP_CMD_AVAIL      17
#define AXI1_START_RESP_COBUF          22
#define AXI1_START_RESP_DB_PORT_LASTDATA 23
#define AXI1_START_RESP_EXPECTED_SPLIT_CNT 29
#define AXI1_START_RESP_ID             15
#define AXI1_START_RESP_MEM_LASTDATA   19
#define AXI1_START_RESP_PORT_LASTDATA  18
#define AXI1_START_RESP_RCV_SPLIT_CNT  34
#define AXI1_START_RESP_SPLIT_CMD_DONE 24
#define AXI1_STATS_REP_PERIOD_WIDTH    4
#define AXI1_TRANS_WRFIFO_LOG2_DEPTH_OFFSET 0
#define AXI1_TRANS_WRFIFO_LOG2_DEPTH_WIDTH 8
#define AXI1_TRANS_WRFIFO_PTR_WIDTH    AXI1_TRANS_WRFIFO_LOG2_DEPTH + 1
#define AXI1_TRANS_WRFIFO_WIDTH        146
#define AXI1_W_PRIORITY_OFFSET         0
#define AXI1_W_PRIORITY_WIDTH          4
#define AXI1_WR_ARRAY_LOG2_DEPTH_OFFSET 24
#define AXI1_WR_ARRAY_LOG2_DEPTH_WIDTH 8
#define AXI1_WR_ARRAY_PTR_WIDTH        AXI1_WR_ARRAY_LOG2_DEPTH
#define AXI1_WR_ARRAY_WIDTH            145
#define AXI1_WRCMD_PROC_FIFO_LOG2_DEPTH_OFFSET 8
#define AXI1_WRCMD_PROC_FIFO_LOG2_DEPTH_WIDTH 8
#define AXI1_WRCMD_PROC_FIFO_PTR_WIDTH AXI1_WRCMD_PROC_FIFO_LOG2_DEPTH + 1
#define AXI1_WRCMD_PROC_FIFO_WIDTH     19
#define AXI2_ALL_STROBES_USED_ENABLE_OFFSET 16
#define AXI2_ALL_STROBES_USED_ENABLE_WIDTH 1
#define AXI2_BDW_OFFSET                24
#define AXI2_BDW_OVFLOW_OFFSET         0
#define AXI2_BDW_OVFLOW_WIDTH          1
#define AXI2_BDW_WIDTH                 7
#define AXI2_CMDFIFO_DEPTH             8
#define AXI2_CMDFIFO_DEPTH_WIDTH       3
#define AXI2_CMDFIFO_LOG2_DEPTH_OFFSET 16
#define AXI2_CMDFIFO_LOG2_DEPTH_WIDTH  8
#define AXI2_CMDFIFO_PTR_WIDTH         AXI2_CMDFIFO_LOG2_DEPTH + 1
#define AXI2_CMDFIFO_WIDTH             88
#define AXI2_CURRENT_BDW_ADDR          463
#define AXI2_CURRENT_BDW_OFFSET        8
#define AXI2_CURRENT_BDW_WIDTH         7
#define AXI2_DB_RESPFIFO_PTR_WIDTH     5
#define AXI2_DB_RESPFIFO_WIDTH         18
#define AXI2_DB_SPLIT_TRFIFO_PTR_WIDTH 5
#define AXI2_DB_SPLIT_TRFIFO_WIDTH     17
#define AXI2_END_RESP                  20
#define AXI2_END_RESP_BUFFERABLE       16
#define AXI2_END_RESP_CMD_AVAIL        17
#define AXI2_END_RESP_COBUF            22
#define AXI2_END_RESP_DB_PORT_LASTDATA 23
#define AXI2_END_RESP_EXPECTED_SPLIT_CNT 25
#define AXI2_END_RESP_ID               0
#define AXI2_END_RESP_MEM_LASTDATA     19
#define AXI2_END_RESP_PORT_LASTDATA    18
#define AXI2_END_RESP_RCV_SPLIT_CNT    30
#define AXI2_END_RESP_SPLIT_CMD_DONE   24
#define AXI2_FIFO_TYPE_REG_OFFSET      16
#define AXI2_FIFO_TYPE_REG_WIDTH       2
#define AXI2_FIXED_PORT_PRIORITY_ENABLE_OFFSET 24
#define AXI2_FIXED_PORT_PRIORITY_ENABLE_WIDTH 1
#define AXI2_PORT_COBUFFIFO_DEPTH      AXI2_CMDFIFO_DEPTH + 16 + 2
#define AXI2_PORT_COBUFFIFO_WIDTH      16
#define AXI2_PORT_RMODW_NUM_BEATS      16
#define AXI2_PORT_SPLIT_BLOCKS_ADDR_WIDTH 12
#define AXI2_PORT_SPLIT_MAX_NUM_BLOCKS_WIDTH 5
#define AXI2_R_PRIORITY_OFFSET         0
#define AXI2_R_PRIORITY_WIDTH          4
#define AXI2_RATE_SEL_WIDTH            4
#define AXI2_RDFIFO_LOG2_DEPTH_OFFSET  24
#define AXI2_RDFIFO_LOG2_DEPTH_WIDTH   8
#define AXI2_RDFIFO_PTR_WIDTH          AXI2_RDFIFO_LOG2_DEPTH + 1
#define AXI2_RDFIFO_WIDTH              152
#define AXI2_RESP_RCV_SPLIT_CNT_WIDTH  5
#define AXI2_RESP_STORAGE_DEPTH        16
#define AXI2_RESP_STORAGE_PTR_WIDTH    4
#define AXI2_RESP_STORAGE_WIDTH        35
#define AXI2_RMODWFIFO_DEPTH           AXI2_CMDFIFO_DEPTH
#define AXI2_RMODWFIFO_PTR_WIDTH       AXI2_CMDFIFO_PTR_WIDTH
#define AXI2_RMODWFIFO_WIDTH           25
#define AXI2_START_RESP                21
#define AXI2_START_RESP_BUFFERABLE     16
#define AXI2_START_RESP_CMD_AVAIL      17
#define AXI2_START_RESP_COBUF          22
#define AXI2_START_RESP_DB_PORT_LASTDATA 23
#define AXI2_START_RESP_EXPECTED_SPLIT_CNT 29
#define AXI2_START_RESP_ID             15
#define AXI2_START_RESP_MEM_LASTDATA   19
#define AXI2_START_RESP_PORT_LASTDATA  18
#define AXI2_START_RESP_RCV_SPLIT_CNT  34
#define AXI2_START_RESP_SPLIT_CMD_DONE 24
#define AXI2_STATS_REP_PERIOD_WIDTH    4
#define AXI2_TRANS_WRFIFO_LOG2_DEPTH_OFFSET 8
#define AXI2_TRANS_WRFIFO_LOG2_DEPTH_WIDTH 8
#define AXI2_TRANS_WRFIFO_PTR_WIDTH    AXI2_TRANS_WRFIFO_LOG2_DEPTH + 1
#define AXI2_TRANS_WRFIFO_WIDTH        146
#define AXI2_W_PRIORITY_OFFSET         8
#define AXI2_W_PRIORITY_WIDTH          4
#define AXI2_WR_ARRAY_LOG2_DEPTH_OFFSET 0
#define AXI2_WR_ARRAY_LOG2_DEPTH_WIDTH 8
#define AXI2_WR_ARRAY_PTR_WIDTH        AXI2_WR_ARRAY_LOG2_DEPTH
#define AXI2_WR_ARRAY_WIDTH            145
#define AXI2_WRCMD_PROC_FIFO_LOG2_DEPTH_OFFSET 16
#define AXI2_WRCMD_PROC_FIFO_LOG2_DEPTH_WIDTH 8
#define AXI2_WRCMD_PROC_FIFO_PTR_WIDTH AXI2_WRCMD_PROC_FIFO_LOG2_DEPTH + 1
#define AXI2_WRCMD_PROC_FIFO_WIDTH     19
#define AXI3_ALL_STROBES_USED_ENABLE_OFFSET 24
#define AXI3_ALL_STROBES_USED_ENABLE_WIDTH 1
#define AXI3_BDW_OFFSET                16
#define AXI3_BDW_OVFLOW_OFFSET         24
#define AXI3_BDW_OVFLOW_WIDTH          1
#define AXI3_BDW_WIDTH                 7
#define AXI3_CMDFIFO_DEPTH             8
#define AXI3_CMDFIFO_DEPTH_WIDTH       3
#define AXI3_CMDFIFO_LOG2_DEPTH_OFFSET 24
#define AXI3_CMDFIFO_LOG2_DEPTH_WIDTH  8
#define AXI3_CMDFIFO_PTR_WIDTH         AXI3_CMDFIFO_LOG2_DEPTH + 1
#define AXI3_CMDFIFO_WIDTH             88
#define AXI3_CURRENT_BDW_ADDR          464
#define AXI3_CURRENT_BDW_OFFSET        0
#define AXI3_CURRENT_BDW_WIDTH         7
#define AXI3_DB_RESPFIFO_PTR_WIDTH     5
#define AXI3_DB_RESPFIFO_WIDTH         18
#define AXI3_DB_SPLIT_TRFIFO_PTR_WIDTH 5
#define AXI3_DB_SPLIT_TRFIFO_WIDTH     17
#define AXI3_END_RESP                  20
#define AXI3_END_RESP_BUFFERABLE       16
#define AXI3_END_RESP_CMD_AVAIL        17
#define AXI3_END_RESP_COBUF            22
#define AXI3_END_RESP_DB_PORT_LASTDATA 23
#define AXI3_END_RESP_EXPECTED_SPLIT_CNT 25
#define AXI3_END_RESP_ID               0
#define AXI3_END_RESP_MEM_LASTDATA     19
#define AXI3_END_RESP_PORT_LASTDATA    18
#define AXI3_END_RESP_RCV_SPLIT_CNT    30
#define AXI3_END_RESP_SPLIT_CMD_DONE   24
#define AXI3_FIFO_TYPE_REG_OFFSET      24
#define AXI3_FIFO_TYPE_REG_WIDTH       2
#define AXI3_FIXED_PORT_PRIORITY_ENABLE_OFFSET 0
#define AXI3_FIXED_PORT_PRIORITY_ENABLE_WIDTH 1
#define AXI3_PORT_COBUFFIFO_DEPTH      AXI3_CMDFIFO_DEPTH + 16 + 2
#define AXI3_PORT_COBUFFIFO_WIDTH      16
#define AXI3_PORT_RMODW_NUM_BEATS      16
#define AXI3_PORT_SPLIT_BLOCKS_ADDR_WIDTH 12
#define AXI3_PORT_SPLIT_MAX_NUM_BLOCKS_WIDTH 5
#define AXI3_R_PRIORITY_OFFSET         8
#define AXI3_R_PRIORITY_WIDTH          4
#define AXI3_RATE_SEL_WIDTH            4
#define AXI3_RDFIFO_LOG2_DEPTH_OFFSET  0
#define AXI3_RDFIFO_LOG2_DEPTH_WIDTH   8
#define AXI3_RDFIFO_PTR_WIDTH          AXI3_RDFIFO_LOG2_DEPTH + 1
#define AXI3_RDFIFO_WIDTH              152
#define AXI3_RESP_RCV_SPLIT_CNT_WIDTH  5
#define AXI3_RESP_STORAGE_DEPTH        16
#define AXI3_RESP_STORAGE_PTR_WIDTH    4
#define AXI3_RESP_STORAGE_WIDTH        35
#define AXI3_RMODWFIFO_DEPTH           AXI3_CMDFIFO_DEPTH
#define AXI3_RMODWFIFO_PTR_WIDTH       AXI3_CMDFIFO_PTR_WIDTH
#define AXI3_RMODWFIFO_WIDTH           25
#define AXI3_START_RESP                21
#define AXI3_START_RESP_BUFFERABLE     16
#define AXI3_START_RESP_CMD_AVAIL      17
#define AXI3_START_RESP_COBUF          22
#define AXI3_START_RESP_DB_PORT_LASTDATA 23
#define AXI3_START_RESP_EXPECTED_SPLIT_CNT 29
#define AXI3_START_RESP_ID             15
#define AXI3_START_RESP_MEM_LASTDATA   19
#define AXI3_START_RESP_PORT_LASTDATA  18
#define AXI3_START_RESP_RCV_SPLIT_CNT  34
#define AXI3_START_RESP_SPLIT_CMD_DONE 24
#define AXI3_STATS_REP_PERIOD_WIDTH    4
#define AXI3_TRANS_WRFIFO_LOG2_DEPTH_OFFSET 16
#define AXI3_TRANS_WRFIFO_LOG2_DEPTH_WIDTH 8
#define AXI3_TRANS_WRFIFO_PTR_WIDTH    AXI3_TRANS_WRFIFO_LOG2_DEPTH + 1
#define AXI3_TRANS_WRFIFO_WIDTH        146
#define AXI3_W_PRIORITY_OFFSET         16
#define AXI3_W_PRIORITY_WIDTH          4
#define AXI3_WR_ARRAY_LOG2_DEPTH_OFFSET 8
#define AXI3_WR_ARRAY_LOG2_DEPTH_WIDTH 8
#define AXI3_WR_ARRAY_PTR_WIDTH        AXI3_WR_ARRAY_LOG2_DEPTH
#define AXI3_WR_ARRAY_WIDTH            145
#define AXI3_WRCMD_PROC_FIFO_LOG2_DEPTH_OFFSET 24
#define AXI3_WRCMD_PROC_FIFO_LOG2_DEPTH_WIDTH 8
#define AXI3_WRCMD_PROC_FIFO_PTR_WIDTH AXI3_WRCMD_PROC_FIFO_LOG2_DEPTH + 1
#define AXI3_WRCMD_PROC_FIFO_WIDTH     19
#define AXI_ADDR_WIDTH                 36
#define AXI_ALL_STROBES_USED_ENABLE_WIDTH 1
#define AXI_ARSNOOP_WIDTH              4
#define AXI_BEAT_WIDTH                 4
#define AXI_BURST_WIDTH                2
#define AXI_CACHE_WIDTH                4
#define AXI_CLK_CYC                    1.25 * TSCALE
#define AXI_EXCLUSIVE                  1
#define AXI_FIXED                      0
#define AXI_FIXED_PORT_PRIORITY_ENABLE_WIDTH 1
#define AXI_FULL_BEAT_WIDTH            5
#define AXI_ID_WIDTH                   16
#define AXI_INCR                       1
#define AXI_LEN_WIDTH                  8
#define AXI_LOCK_WIDTH                 1
#define AXI_PROT_WIDTH                 2
#define AXI_REGINFIFO_PTR_WIDTH        3
#define AXI_REGION_WIDTH               4
#define AXI_REGOUTFIFO_PTR_WIDTH       3
#define AXI_REGOUTFIFO_WIDTH           32
#define AXI_RESP_WIDTH                 2
#define AXI_RESPONSE_OK                0
#define AXI_SIZE_VAL_WIDTH             8
#define AXI_SIZE_WIDTH                 3
#define AXI_WRAP                       2
#define AXI_WRITE_INTERLEAVING_DEPTH   1
#define B2B_PLACEMENT_REQ_P            255
#define BANK_ACTIVATE_2TICK_COUNT_OFFSET 24
#define BANK_ACTIVATE_2TICK_COUNT_WIDTH 3
#define BANK_ACTIVATE_4TICK_COUNT_OFFSET 16
#define BANK_ACTIVATE_4TICK_COUNT_WIDTH 3
#define BANK_ADDR_INTLV_EN_OFFSET      0
#define BANK_ADDR_INTLV_EN_WIDTH       1
#define BANK_DIFF_ADDR                 325
#define BANK_DIFF_OFFSET               0
#define BANK_DIFF_WIDTH                2
#define BANK_GROUP_WIDTH               2
#define BANK_SPLIT_EN_OFFSET           16
#define BANK_SPLIT_EN_WIDTH            1
#define BANK_START_BIT_OFFSET          24
#define BANK_START_BIT_WIDTH           5
#define BANK_WIDTH                     4
#define BANKS_PER_BG_WIDTH             2
#define BDW_WIDTH                      7
#define BEAT_SIZE_P                    250
#define BEAT_WIDTH                     4
#define BG0_BIT                        15
#define BG_ROTATE_EN_ADDR              338
#define BG_ROTATE_EN_OFFSET            0
#define BG_ROTATE_EN_WIDTH             1
#define BIST_ADDR_CHECK_OFFSET         8
#define BIST_ADDR_CHECK_WIDTH          1
#define BIST_DATA_CHECK_OFFSET         0
#define BIST_DATA_CHECK_WIDTH          1
#define BIST_DATA_MASK_OFFSET          0
#define BIST_DATA_MASK_WIDTH           64
#define BIST_DATA_PATTERN_OFFSET       0
#define BIST_DATA_PATTERN_WIDTH        128
#define BIST_DATA_WIDTH                128
#define BIST_DONE_BIT                  6
#define BIST_DONE_WIDTH                1
#define BIST_ERR_COUNT_OFFSET          0
#define BIST_ERR_COUNT_WIDTH           12
#define BIST_ERR_STOP_OFFSET           16
#define BIST_ERR_STOP_WIDTH            12
#define BIST_EXP_DATA_OFFSET           0
#define BIST_FAIL_ADDR_OFFSET          0
#define BIST_FAIL_ADDR_WIDTH           36
#define BIST_FAIL_DATA_OFFSET          0
#define BIST_GO_OFFSET                 8
#define BIST_GO_WIDTH                  1
#define BIST_NUM_OF_BG_WIDTH           5
#define BIST_RD_FIFO_DEPTH             52
#define BIST_RD_FIFO_DEPTH_WIDTH       6
#define BIST_RESULT_OFFSET             16
#define BIST_RESULT_WIDTH              2
#define BIST_RET_STATE_EXIT_OFFSET     0
#define BIST_RET_STATE_EXIT_WIDTH      1
#define BIST_RET_STATE_OFFSET          8
#define BIST_RET_STATE_WIDTH           1
#define BIST_START_ADDRESS_OFFSET      0
#define BIST_START_ADDRESS_WIDTH       36
#define BIST_TEST_MODE_OFFSET          0
#define BIST_TEST_MODE_WIDTH           3
#define BIST_WRD_ADDR_WIDTH            36
#define BITS_PER_SLICE                 8
#define BL_ON_FLY_ENABLE_OFFSET        0
#define BL_ON_FLY_ENABLE_WIDTH         1
#define BOOT_CLK                       20.0*TSCALE
#define BST8_ADDR_WIDTH                5
#define BST8_DATA_WIDTH                256
#define BST_ADDR_LOWER                 1
#define BST_ADDR_UPPER                 4
#define BST_LAT_WIDTH                  5
#define BSTLEN_ADDR                    73
#define BSTLEN_OFFSET                  0
#define BSTLEN_WIDTH                   5
#define BSTOF2                         1
#define BSTOF4                         2
#define BSTOF8                         3
#define BSTOF16                        4
#define BSTOF32                        5
#define BURST_ON_FLY_BIT_OFFSET        24
#define BURST_ON_FLY_BIT_WIDTH         4
#define BYTE_P_USERWORD                16
#define BYTE_WIDTH                     3
#define BYTES_PER_USER_DW              16
#define BYTES_PER_USER_DW_ENC          4
#define CA_DEFAULT_VAL_F0_OFFSET       16
#define CA_DEFAULT_VAL_F0_WIDTH        1
#define CA_DEFAULT_VAL_F1_OFFSET       0
#define CA_DEFAULT_VAL_F1_WIDTH        1
#define CA_DEFAULT_VAL_F2_OFFSET       16
#define CA_DEFAULT_VAL_F2_WIDTH        1
#define CA_DEFAULT_VAL_WIDTH           1
#define CA_PAR_CRC_ERR_IN_RETRY        44
#define CA_PARITY_ERR_BIT              21
#define CA_PARITY_ERROR_INJECT_OFFSET  0
#define CA_PARITY_ERROR_INJECT_WIDTH   25
#define CA_PARITY_ERROR_OFFSET         0
#define CA_PARITY_ERROR_WIDTH          1
#define CA_PARITY_LAT_F0_OFFSET        24
#define CA_PARITY_LAT_F0_WIDTH         4
#define CA_PARITY_LAT_F1_OFFSET        24
#define CA_PARITY_LAT_F1_WIDTH         4
#define CA_PARITY_LAT_F2_OFFSET        24
#define CA_PARITY_LAT_F2_WIDTH         4
#define CA_PARITY_LAT_WIDTH            4
#define CA_WIDE_CS_ENABLE_OFFSET       0
#define CA_WIDE_CS_ENABLE_WIDTH        4
#define CALVL_AREF_EN_OFFSET           8
#define CALVL_AREF_EN_WIDTH            1
#define CALVL_BG_PAT_0_OFFSET          0
#define CALVL_BG_PAT_0_WIDTH           20
#define CALVL_BG_PAT_1_OFFSET          0
#define CALVL_BG_PAT_1_WIDTH           20
#define CALVL_BG_PAT_2_OFFSET          0
#define CALVL_BG_PAT_2_WIDTH           20
#define CALVL_BG_PAT_3_OFFSET          0
#define CALVL_BG_PAT_3_WIDTH           20
#define CALVL_BG_PAT_WIDTH             20
#define CALVL_CS_MAP_OFFSET            24
#define CALVL_CS_MAP_WIDTH             2
#define CALVL_CS_OFFSET                8
#define CALVL_CS_WIDTH                 1
#define CALVL_DFI_PROMOTE_THRESHOLD_F0_OFFSET 0
#define CALVL_DFI_PROMOTE_THRESHOLD_F0_WIDTH 16
#define CALVL_DFI_PROMOTE_THRESHOLD_F1_OFFSET 16
#define CALVL_DFI_PROMOTE_THRESHOLD_F1_WIDTH 16
#define CALVL_DFI_PROMOTE_THRESHOLD_F2_OFFSET 0
#define CALVL_DFI_PROMOTE_THRESHOLD_F2_WIDTH 16
#define CALVL_DFI_PROMOTE_THRESHOLD_WIDTH 16
#define CALVL_EN_OFFSET                8
#define CALVL_EN_WIDTH                 1
#define CALVL_ERROR_BIT                12
#define CALVL_ERROR_STATUS_OFFSET      16
#define CALVL_ERROR_STATUS_WIDTH       4
#define CALVL_HIGH_THRESHOLD_F0_OFFSET 16
#define CALVL_HIGH_THRESHOLD_F0_WIDTH  16
#define CALVL_HIGH_THRESHOLD_F1_OFFSET 0
#define CALVL_HIGH_THRESHOLD_F1_WIDTH  16
#define CALVL_HIGH_THRESHOLD_F2_OFFSET 16
#define CALVL_HIGH_THRESHOLD_F2_WIDTH  16
#define CALVL_HIGH_THRESHOLD_WIDTH     16
#define CALVL_NORM_THRESHOLD_F0_OFFSET 0
#define CALVL_NORM_THRESHOLD_F0_WIDTH  16
#define CALVL_NORM_THRESHOLD_F1_OFFSET 16
#define CALVL_NORM_THRESHOLD_F1_WIDTH  16
#define CALVL_NORM_THRESHOLD_F2_OFFSET 0
#define CALVL_NORM_THRESHOLD_F2_WIDTH  16
#define CALVL_NORM_THRESHOLD_WIDTH     16
#define CALVL_ON_SREF_EXIT_OFFSET      0
#define CALVL_ON_SREF_EXIT_WIDTH       1
#define CALVL_PAT_0_OFFSET             0
#define CALVL_PAT_0_WIDTH              20
#define CALVL_PAT_1_OFFSET             0
#define CALVL_PAT_1_WIDTH              20
#define CALVL_PAT_2_OFFSET             0
#define CALVL_PAT_2_WIDTH              20
#define CALVL_PAT_3_OFFSET             0
#define CALVL_PAT_3_WIDTH              20
#define CALVL_PAT_WIDTH                20
#define CALVL_PERIODIC_OFFSET          24
#define CALVL_PERIODIC_WIDTH           1
#define CALVL_REQ_BIT                  19
#define CALVL_REQ_OFFSET               0
#define CALVL_REQ_WIDTH                1
#define CALVL_RESP_MASK_OFFSET         0
#define CALVL_RESP_MASK_WIDTH          1
#define CALVL_ROTATE_OFFSET            16
#define CALVL_ROTATE_WIDTH             1
#define CALVL_SEQ_EN_OFFSET            8
#define CALVL_SEQ_EN_WIDTH             2
#define CALVL_SW_PROMOTE_THRESHOLD_F0_OFFSET 16
#define CALVL_SW_PROMOTE_THRESHOLD_F0_WIDTH 16
#define CALVL_SW_PROMOTE_THRESHOLD_F1_OFFSET 0
#define CALVL_SW_PROMOTE_THRESHOLD_F1_WIDTH 16
#define CALVL_SW_PROMOTE_THRESHOLD_F2_OFFSET 16
#define CALVL_SW_PROMOTE_THRESHOLD_F2_WIDTH 16
#define CALVL_SW_PROMOTE_THRESHOLD_WIDTH 16
#define CALVL_TIMEOUT_F0_OFFSET        0
#define CALVL_TIMEOUT_F0_WIDTH         16
#define CALVL_TIMEOUT_F1_OFFSET        16
#define CALVL_TIMEOUT_F1_WIDTH         16
#define CALVL_TIMEOUT_F2_OFFSET        0
#define CALVL_TIMEOUT_F2_WIDTH         16
#define CALVL_TIMEOUT_WIDTH            16
#define CAS_PARITY_ERROR_INJECT_BIT    21
#define CASLAT_LIN_F0_OFFSET           0
#define CASLAT_LIN_F0_WIDTH            7
#define CASLAT_LIN_F1_OFFSET           0
#define CASLAT_LIN_F1_WIDTH            7
#define CASLAT_LIN_F2_OFFSET           0
#define CASLAT_LIN_F2_WIDTH            7
#define CASLAT_LIN_WIDTH               7
#define CHANNELWIDTH                   16
#define CKE_DELAY_OFFSET               24
#define CKE_DELAY_WIDTH                3
#define CKE_INACTIVE_OFFSET            0
#define CKE_INACTIVE_WIDTH             32
#define CKE_SHIFT_WIDTH                8
#define CKE_STATUS_OFFSET              8
#define CKE_STATUS_WIDTH               2
#define CKSRE_F0_OFFSET                16
#define CKSRE_F0_WIDTH                 8
#define CKSRE_F1_OFFSET                0
#define CKSRE_F1_WIDTH                 8
#define CKSRE_F2_OFFSET                16
#define CKSRE_F2_WIDTH                 8
#define CKSRE_WIDTH                    8
#define CKSRX_F0_OFFSET                24
#define CKSRX_F0_WIDTH                 8
#define CKSRX_F1_OFFSET                8
#define CKSRX_F1_WIDTH                 8
#define CKSRX_F2_OFFSET                24
#define CKSRX_F2_WIDTH                 8
#define CKSRX_WIDTH                    8
#define CLK_CYC                        0.625 * TSCALE
#define CMD_ERR_P                      257
#define CMD_SEL_WIDTH                  5
#define COL_DIFF_ADDR                  325
#define COL_DIFF_OFFSET                16
#define COL_DIFF_WIDTH                 4
#define COL_WIDTH                      13
#define COMMAND_AGE_COUNT_OFFSET       24
#define COMMAND_AGE_COUNT_WIDTH        8
#define COMMAND_MUX_SELECT_WIDTH       2
#define COMMAND_RMODW_P                188
#define COMMAND_TYPE_P                 41
#define CONCURRENTAP_OFFSET            24
#define CONCURRENTAP_WIDTH             1
#define CONTROL_WORD_WIDTH             29
#define CONTROLLER_BUSY_OFFSET         16
#define CONTROLLER_BUSY_WIDTH          1
#define CRC_ALERT_N_MAX_PW_OFFSET      24
#define CRC_ALERT_N_MAX_PW_WIDTH       4
#define CRC_MC_MODE_BIT                1
#define CRC_MEM_DATA_WIDTH             32
#define CRC_MEM_DM_WIDTH               4
#define CRC_MODE_OFFSET                16
#define CRC_MODE_WIDTH                 2
#define CRC_RETRY_EN_OFFSET            16
#define CRC_RETRY_EN_WIDTH             1
#define CRC_RETRY_IN_PROGRESS_OFFSET   0
#define CRC_RETRY_IN_PROGRESS_WIDTH    1
#define CRC_RETRY_SHIFTER_WIDTH        48
#define CRC_RETRY_WR_FIFO_WIDTH        144
#define CRC_WR_BLK_SIZE_OFFSET         8
#define CRC_WR_BLK_SIZE_WIDTH          1
#define CRC_WR_EN_BIT                  0
#define CRC_WR_ERR_BIT                 43
#define CS_BANK_WIDTH                  5
#define CS_COMPARISON_FOR_REFRESH_DEPTH_OFFSET 8
#define CS_COMPARISON_FOR_REFRESH_DEPTH_WIDTH 5
#define CS_DIFF_WIDTH                  1
#define CS_MAP_ADDR                    330
#define CS_MAP_OFFSET                  16
#define CS_MAP_WIDTH                   2
#define CS_SAME_EN_ADDR                328
#define CS_SAME_EN_OFFSET              24
#define CS_SAME_EN_WIDTH               1
#define CS_WIDTH                       1
#define CTL_REG_ADDR_WIDTH             11
#define CTLR_DISABLE_ODT_ON_ZQ_WIDTH   1
#define CTRLUPD_AREF_HP_ENABLE_OFFSET  8
#define CTRLUPD_AREF_HP_ENABLE_WIDTH   1
#define CTRLUPD_REQ_OFFSET             24
#define CTRLUPD_REQ_PER_AREF_EN_ADDR   335
#define CTRLUPD_REQ_PER_AREF_EN_OFFSET 0
#define CTRLUPD_REQ_PER_AREF_EN_WIDTH  1
#define CTRLUPD_REQ_WIDTH              1
#define CURRENT_REG_COPY_ADDR          178
#define CURRENT_REG_COPY_OFFSET        16
#define CURRENT_REG_COPY_WIDTH         2
#define DATA_BYTE_WIDTH                4
#define DATA_WIDTH                     64
#define DDR1_CLASS                     0
#define DDR2_CLASS                     2
#define DDR3_CLASS                     3
#define DDR4_CLASS                     5
#define DDR5_CLASS                     6
#define DDR_MODE_WIDTH                 3
#define DECODED_BSTLEN_WIDTH           6
#define DEN_ID_WIDTH                   4
#define DEVICE0_BYTE0_CS0_OFFSET       16
#define DEVICE0_BYTE0_CS0_WIDTH        4
#define DEVICE0_BYTE0_CS1_OFFSET       24
#define DEVICE0_BYTE0_CS1_WIDTH        4
#define DEVICE1_BYTE0_CS0_OFFSET       24
#define DEVICE1_BYTE0_CS0_WIDTH        4
#define DEVICE1_BYTE0_CS1_OFFSET       0
#define DEVICE1_BYTE0_CS1_WIDTH        4
#define DEVICE2_BYTE0_CS0_OFFSET       0
#define DEVICE2_BYTE0_CS0_WIDTH        4
#define DEVICE2_BYTE0_CS1_OFFSET       8
#define DEVICE2_BYTE0_CS1_WIDTH        4
#define DEVICE3_BYTE0_CS0_OFFSET       8
#define DEVICE3_BYTE0_CS0_WIDTH        4
#define DEVICE3_BYTE0_CS1_OFFSET       16
#define DEVICE3_BYTE0_CS1_WIDTH        4
#define DEVICE_BIT_WIDTH               9
#define DEVICE_BYTE0_WIDTH             4
#define DFI_ADDRESS_WIDTH              20
#define DFI_CALVL_RESP_WIDTH           2
#define DFI_CALVL_SLICE_WIDTH          1
#define DFI_CMD_COUNT_WIDTH            4
#define DFI_CTLR_DATA_WIDTH            128
#define DFI_CTLR_WRDATA_MASK_WIDTH     16
#define DFI_DATA_BYTE_WIDTH            4
#define DFI_DATA_SLICE_WIDTH           4
#define DFI_DATA_WIDTH                 64
#define DFI_DBI_WIDTH                  8
#define DFI_DNV_WIDTH                  8
#define DFI_ERROR_BIT                  22
#define DFI_ERROR_INFO_OFFSET          8
#define DFI_ERROR_INFO_WIDTH           20
#define DFI_ERROR_OFFSET               0
#define DFI_ERROR_WIDTH                5
#define DFI_FULL_RDDATA_WIDTH          64
#define DFI_FUNCTION_WIDTH             8
#define DFI_INV_DATA_CS_OFFSET         8
#define DFI_INV_DATA_CS_WIDTH          1
#define DFI_MRR_WIDTH                  32
#define DFI_NET_DLY                    0
#define DFI_PHY_CALVL_MODE_OFFSET      16
#define DFI_PHY_CALVL_MODE_WIDTH       1
#define DFI_PHY_RDLVL_GATE_MODE_OFFSET 0
#define DFI_PHY_RDLVL_GATE_MODE_WIDTH  1
#define DFI_PHY_RDLVL_MODE_OFFSET      24
#define DFI_PHY_RDLVL_MODE_WIDTH       1
#define DFI_PHY_WRLVL_MODE_OFFSET      16
#define DFI_PHY_WRLVL_MODE_WIDTH       1
#define DFI_PHYMSTR_TYPE_WIDTH         2
#define DFI_RDLVL_DELAY_WIDTH          16
#define DFI_RDLVL_GATE_DELAY_WIDTH     16
#define DFI_RDLVL_RESP_WIDTH           8
#define DFI_RDLVL_SLICE_WIDTH          4
#define DFI_WRDATA_MASK_WIDTH          8
#define DFI_WRLVL_DELAY_WIDTH          16
#define DFI_WRLVL_RESP_WIDTH           4
#define DFI_WRLVL_SLICE_WIDTH          4
#define DFIBUS_BOOT_FREQ_ADDR          23
#define DFIBUS_BOOT_FREQ_OFFSET        0
#define DFIBUS_BOOT_FREQ_WIDTH         2
#define DFIBUS_FREQ_F0_OFFSET          8
#define DFIBUS_FREQ_F0_WIDTH           5
#define DFIBUS_FREQ_F1_OFFSET          16
#define DFIBUS_FREQ_F1_WIDTH           5
#define DFIBUS_FREQ_F2_OFFSET          24
#define DFIBUS_FREQ_F2_WIDTH           5
#define DFIBUS_FREQ_INIT_ADDR          22
#define DFIBUS_FREQ_INIT_OFFSET        24
#define DFIBUS_FREQ_INIT_WIDTH         2
#define DFIBUS_FREQ_WIDTH              5
#define DFS_ALWAYS_WRITE_FSP_OFFSET    8
#define DFS_ALWAYS_WRITE_FSP_WIDTH     1
#define DFS_CALVL_EN_OFFSET            24
#define DFS_CALVL_EN_WIDTH             1
#define DFS_CMD_WIDTH                  5
#define DFS_DLL_OFF_OFFSET             0
#define DFS_DLL_OFF_WIDTH              3
#define DFS_DONE_BIT                   38
#define DFS_ENABLE_OFFSET              24
#define DFS_ENABLE_WIDTH               1
#define DFS_FRC_STATUS_WIDTH           4
#define DFS_FSP_INSYNC_ACTIVE_OFFSET   16
#define DFS_FSP_INSYNC_ACTIVE_WIDTH    1
#define DFS_FSP_INSYNC_INACTIVE_OFFSET 24
#define DFS_FSP_INSYNC_INACTIVE_WIDTH  1
#define DFS_PHY_REG_WRITE_ADDR_OFFSET  0
#define DFS_PHY_REG_WRITE_ADDR_WIDTH   32
#define DFS_PHY_REG_WRITE_DATA_F0_OFFSET 0
#define DFS_PHY_REG_WRITE_DATA_F0_WIDTH 32
#define DFS_PHY_REG_WRITE_DATA_F1_OFFSET 0
#define DFS_PHY_REG_WRITE_DATA_F1_WIDTH 32
#define DFS_PHY_REG_WRITE_DATA_F2_OFFSET 0
#define DFS_PHY_REG_WRITE_DATA_F2_WIDTH 32
#define DFS_PHY_REG_WRITE_DATA_WIDTH   32
#define DFS_PHY_REG_WRITE_EN_OFFSET    24
#define DFS_PHY_REG_WRITE_EN_WIDTH     1
#define DFS_PHY_REG_WRITE_MASK_OFFSET  0
#define DFS_PHY_REG_WRITE_MASK_WIDTH   4
#define DFS_PHY_REG_WRITE_WAIT_OFFSET  8
#define DFS_PHY_REG_WRITE_WAIT_WIDTH   16
#define DFS_PROMOTE_THRESHOLD_F0_OFFSET 0
#define DFS_PROMOTE_THRESHOLD_F0_WIDTH 16
#define DFS_PROMOTE_THRESHOLD_F1_OFFSET 16
#define DFS_PROMOTE_THRESHOLD_F1_WIDTH 16
#define DFS_PROMOTE_THRESHOLD_F2_OFFSET 0
#define DFS_PROMOTE_THRESHOLD_F2_WIDTH 16
#define DFS_PROMOTE_THRESHOLD_WIDTH    16
#define DFS_RDLVL_EN_OFFSET            8
#define DFS_RDLVL_EN_WIDTH             1
#define DFS_RDLVL_GATE_EN_OFFSET       16
#define DFS_RDLVL_GATE_EN_WIDTH        1
#define DFS_REG_COPY_WIDTH             2
#define DFS_STATUS_BIT                 39
#define DFS_STATUS_OFFSET              8
#define DFS_STATUS_WIDTH               2
#define DFS_WRLVL_EN_OFFSET            0
#define DFS_WRLVL_EN_WIDTH             1
#define DFS_ZQ_EN_OFFSET               16
#define DFS_ZQ_EN_WIDTH                1
#define DISABLE_CRC_BEFORE_MPR_OFFSET  8
#define DISABLE_CRC_BEFORE_MPR_WIDTH   1
#define DISABLE_MEMORY_MASKED_WRITE_OFFSET 24
#define DISABLE_MEMORY_MASKED_WRITE_WIDTH 1
#define DISABLE_RD_INTERLEAVE_OFFSET   0
#define DISABLE_RD_INTERLEAVE_WIDTH    1
#define DISABLE_RW_GROUP_W_BNK_CONFLICT_ADDR 329
#define DISABLE_RW_GROUP_W_BNK_CONFLICT_OFFSET 8
#define DISABLE_RW_GROUP_W_BNK_CONFLICT_WIDTH 2
#define DISABLE_UPDATE_TVRCG_OFFSET    16
#define DISABLE_UPDATE_TVRCG_WIDTH     1
#define DLL_LOCK_STATE_CHANGE_BIT      35
#define DLL_RST_ADJ_DLY_OFFSET         16
#define DLL_RST_ADJ_DLY_WIDTH          8
#define DLL_RST_DELAY_OFFSET           0
#define DLL_RST_DELAY_WIDTH            16
#define DM_BYTE_WIDTH_D                8
#define DM_WIDTH                       8
#define DQ_MAP_0_OFFSET                0
#define DQ_MAP_0_WIDTH                 64
#define DQ_MAP_ODD_RANK_SWAP_0_OFFSET  0
#define DQ_MAP_ODD_RANK_SWAP_0_WIDTH   2
#define DQ_MAP_ODD_RANK_SWAP_WIDTH     2
#define DQ_MAP_WIDTH                   64
#define DQS_OSC_BASE_BIT               29
#define DQS_OSC_DONE_BIT               28
#define DQS_OSC_ENABLE_OFFSET          8
#define DQS_OSC_ENABLE_WIDTH           1
#define DQS_OSC_HIGH_THRESHOLD_OFFSET  0
#define DQS_OSC_HIGH_THRESHOLD_WIDTH   32
#define DQS_OSC_MPC_CMD_OFFSET         0
#define DQS_OSC_MPC_CMD_WIDTH          24
#define DQS_OSC_NORM_THRESHOLD_OFFSET  0
#define DQS_OSC_NORM_THRESHOLD_WIDTH   32
#define DQS_OSC_OOV_BIT                31
#define DQS_OSC_OVFL_BIT               30
#define DQS_OSC_PERIOD_OFFSET          16
#define DQS_OSC_PERIOD_WIDTH           15
#define DQS_OSC_PROMOTE_THRESHOLD_OFFSET 0
#define DQS_OSC_PROMOTE_THRESHOLD_WIDTH 32
#define DQS_OSC_REQUEST_OFFSET         16
#define DQS_OSC_REQUEST_WIDTH          1
#define DQS_OSC_TIMEOUT_BIT            32
#define DQS_OSC_TIMEOUT_OFFSET         0
#define DQS_OSC_TIMEOUT_WIDTH          32
#define DQS_OSC_TST_OFFSET             16
#define DQS_OSC_TST_WIDTH              1
#define DRAM_CLASS_OFFSET              8
#define DRAM_CLASS_WIDTH               4
#define DRAM_CLK_DISABLE_OFFSET        8
#define DRAM_CLK_DISABLE_WIDTH         2
#define DRAM_CMD_INHIBIT_BIT           34
#define ECC_DATA_WIDTH                 64
#define ECC_DISABLE_BIT                6
#define EER                            0
#define EFFECT_MEM_DATA_WIDTH          32
#define EFFECT_USER_DATA_WIDTH         128
#define EN_1T_TIMING_OFFSET            16
#define EN_1T_TIMING_WIDTH             1
#define EN_ODT_ASSERT_EXCEPT_RD_OFFSET 8
#define EN_ODT_ASSERT_EXCEPT_RD_WIDTH  1
#define ENABLE_QUICK_SREFRESH_OFFSET   16
#define ENABLE_QUICK_SREFRESH_WIDTH    1
#define END_BANK                       15
#define END_BANK_PARITY_ERROR_INJECT   16
#define END_COL                        2
#define END_CS                         36
#define END_ENTRY_EQUAL                0
#define END_ROW                        19
#define ENTRY_EQUAL                    0
#define ENTRY_EQUAL_WIDTH              2
#define ENTRY_FULL                     2
#define EXCLUSIVE_ACCESS_DEPTH         1
#define EXCLUSIVE_BIT                  3
#define FIFO_SRAM                      0
#define FIRST_CMD_IN_SEQ_P             254
#define FM_ARB_AREF_BIT                1
#define FM_ARB_BIST_BIT                17
#define FM_ARB_CALVL_BIT               7
#define FM_ARB_DFS_BIT                 15
#define FM_ARB_DQS_OSC_BIT             4
#define FM_ARB_HW_BIT                  0
#define FM_ARB_LPC_BIT                 16
#define FM_ARB_MPRR_BIT                14
#define FM_ARB_MRR_BIT                 13
#define FM_ARB_MRW_BIT                 12
#define FM_ARB_PHYMSTR_BIT             6
#define FM_ARB_PRE_BIT                 2
#define FM_ARB_RDLVL_BIT               9
#define FM_ARB_RDLVL_GATE_BIT          8
#define FM_ARB_REQ_WIDTH               18
#define FM_ARB_UPD_BIT                 5
#define FM_ARB_VREF_BIT                11
#define FM_ARB_WRLVL_BIT               10
#define FM_ARB_ZQ_BIT                  3
#define FM_DISCONNECT_OFFSET           8
#define FM_DISCONNECT_WIDTH            8
#define FM_HIGH_CQ_DISCONNECT_OFFSET   0
#define FM_HIGH_CQ_DISCONNECT_WIDTH    8
#define FM_NORM_CQ_DISCONNECT_OFFSET   24
#define FM_NORM_CQ_DISCONNECT_WIDTH    8
#define FREQ_CHANGE_REG_COPY_WIDTH     2
#define FREQ_CHANGE_TYPE_F0_OFFSET     0
#define FREQ_CHANGE_TYPE_F0_WIDTH      2
#define FREQ_CHANGE_TYPE_F1_OFFSET     8
#define FREQ_CHANGE_TYPE_F1_WIDTH      2
#define FREQ_CHANGE_TYPE_F2_OFFSET     16
#define FREQ_CHANGE_TYPE_F2_WIDTH      2
#define FREQ_CHANGE_TYPE_WIDTH         2
#define FSP0_FRC_OFFSET                24
#define FSP0_FRC_VALID_OFFSET          8
#define FSP0_FRC_VALID_WIDTH           1
#define FSP0_FRC_WIDTH                 2
#define FSP1_FRC_OFFSET                0
#define FSP1_FRC_VALID_OFFSET          16
#define FSP1_FRC_VALID_WIDTH           1
#define FSP1_FRC_WIDTH                 2
#define FSP_OP_CURRENT_OFFSET          24
#define FSP_OP_CURRENT_WIDTH           1
#define FSP_PHY_UPDATE_MRW_OFFSET      0
#define FSP_PHY_UPDATE_MRW_WIDTH       1
#define FSP_STATUS_OFFSET              16
#define FSP_STATUS_WIDTH               1
#define FSP_WR_CURRENT_OFFSET          0
#define FSP_WR_CURRENT_WIDTH           1
#define FULL_BEAT_WIDTH                5
#define FULL_RD_BEAT_WIDTH             4
#define FUNC_VALID_CYCLES_OFFSET       0
#define FUNC_VALID_CYCLES_WIDTH        4
#define GATHER_FIFO_RESYNC_OFFSET      8
#define GATHER_FIFO_RESYNC_WIDTH       1
#define HALF_DATA_WIDTH                32
#define HALF_DQS_WIDTH                 4
#define HW_PROMOTE_THRESHOLD_F0_OFFSET 0
#define HW_PROMOTE_THRESHOLD_F0_WIDTH  16
#define HW_PROMOTE_THRESHOLD_F1_OFFSET 16
#define HW_PROMOTE_THRESHOLD_F1_WIDTH  16
#define HW_PROMOTE_THRESHOLD_F2_OFFSET 0
#define HW_PROMOTE_THRESHOLD_F2_WIDTH  16
#define HW_PROMOTE_THRESHOLD_WIDTH     16
#define HWI_CLK_WIDTH                  2
#define HWI_CMD_WIDTH                  6
#define HWI_CMDTYPE_WIDTH              2
#define HWI_SR_LEVEL_WIDTH             3
#define HWI_SREF_WIDTH                 2
#define IN_ORDER_ACCEPT_ADDR           334
#define IN_ORDER_ACCEPT_OFFSET         0
#define IN_ORDER_ACCEPT_WIDTH          1
#define INHIBIT_CMD_WIDTH              23
#define INHIBIT_DRAM_CMD_OFFSET        8
#define INHIBIT_DRAM_CMD_WIDTH         2
#define INIT_DONE_BIT                  4
#define INT_ACK_ADDR                   341
#define INT_ACK_OFFSET                 0
#define INT_ACK_WIDTH                  45
#define INT_MASK_OFFSET                0
#define INT_MASK_WIDTH                 46
#define INT_STATUS_ADDR                339
#define INT_STATUS_OFFSET              0
#define INT_STATUS_WIDTH               46
#define LAST_CMD_IN_SEQ_P              253
#define LAST_RD_WIDTH                  1
#define LENGTH_P                       176
#define LONG_COUNT_MASK_OFFSET         8
#define LONG_COUNT_MASK_WIDTH          5
#define LOWPOWER_REFRESH_ENABLE_OFFSET 8
#define LOWPOWER_REFRESH_ENABLE_WIDTH  2
#define LP_AUTO_ENTRY_EN_OFFSET        16
#define LP_AUTO_ENTRY_EN_WIDTH         4
#define LP_AUTO_EXIT_EN_OFFSET         24
#define LP_AUTO_EXIT_EN_WIDTH          4
#define LP_AUTO_MEM_GATE_EN_OFFSET     0
#define LP_AUTO_MEM_GATE_EN_WIDTH      3
#define LP_AUTO_PD_IDLE_OFFSET         8
#define LP_AUTO_PD_IDLE_WIDTH          12
#define LP_AUTO_SR_LONG_IDLE_OFFSET    16
#define LP_AUTO_SR_LONG_IDLE_WIDTH     8
#define LP_AUTO_SR_LONG_MC_GATE_IDLE_OFFSET 24
#define LP_AUTO_SR_LONG_MC_GATE_IDLE_WIDTH 8
#define LP_AUTO_SR_SHORT_IDLE_OFFSET   0
#define LP_AUTO_SR_SHORT_IDLE_WIDTH    12
#define LP_CMD_ADDR                    159
#define LP_CMD_OFFSET                  0
#define LP_CMD_WIDTH                   7
#define LP_EXT_CMD_WIDTH               6
#define LP_EXT_RESP_WIDTH              2
#define LP_INST                        1
#define LP_STATE_OFFSET                8
#define LP_STATE_WIDTH                 7
#define LPC_CMD_DONE_BIT               5
#define LPC_PROMOTE_THRESHOLD_F0_OFFSET 16
#define LPC_PROMOTE_THRESHOLD_F0_WIDTH 16
#define LPC_PROMOTE_THRESHOLD_F1_OFFSET 0
#define LPC_PROMOTE_THRESHOLD_F1_WIDTH 16
#define LPC_PROMOTE_THRESHOLD_F2_OFFSET 16
#define LPC_PROMOTE_THRESHOLD_F2_WIDTH 16
#define LPC_PROMOTE_THRESHOLD_WIDTH    16
#define LPC_SR_CTRLUPD_EN_OFFSET       0
#define LPC_SR_CTRLUPD_EN_WIDTH        1
#define LPC_SR_EXIT_CMD_EN_OFFSET      24
#define LPC_SR_EXIT_CMD_EN_WIDTH       1
#define LPC_SR_PHYMSTR_EN_OFFSET       16
#define LPC_SR_PHYMSTR_EN_WIDTH        1
#define LPC_SR_PHYUPD_EN_OFFSET        8
#define LPC_SR_PHYUPD_EN_WIDTH         1
#define LPC_SR_ZQ_EN_OFFSET            0
#define LPC_SR_ZQ_EN_WIDTH             1
#define LPDDR3_ODT_OFF_MAX             350.0
#define LPDDR3_ODT_ON_MIN              175.0
#define LPI_CTRL_IDLE_WAKEUP_F0_OFFSET 8
#define LPI_CTRL_IDLE_WAKEUP_F0_WIDTH  4
#define LPI_CTRL_IDLE_WAKEUP_F1_OFFSET 16
#define LPI_CTRL_IDLE_WAKEUP_F1_WIDTH  4
#define LPI_CTRL_IDLE_WAKEUP_F2_OFFSET 24
#define LPI_CTRL_IDLE_WAKEUP_F2_WIDTH  4
#define LPI_CTRL_IDLE_WAKEUP_WIDTH     4
#define LPI_CTRL_REQ_EN_OFFSET         8
#define LPI_CTRL_REQ_EN_WIDTH          1
#define LPI_ERROR_BIT                  24
#define LPI_PD_WAKEUP_F0_OFFSET        8
#define LPI_PD_WAKEUP_F0_WIDTH         4
#define LPI_PD_WAKEUP_F1_OFFSET        16
#define LPI_PD_WAKEUP_F1_WIDTH         4
#define LPI_PD_WAKEUP_F2_OFFSET        24
#define LPI_PD_WAKEUP_F2_WIDTH         4
#define LPI_PD_WAKEUP_WIDTH            4
#define LPI_SR_LONG_MCCLK_GATE_WAKEUP_F0_OFFSET 0
#define LPI_SR_LONG_MCCLK_GATE_WAKEUP_F0_WIDTH 4
#define LPI_SR_LONG_MCCLK_GATE_WAKEUP_F1_OFFSET 8
#define LPI_SR_LONG_MCCLK_GATE_WAKEUP_F1_WIDTH 4
#define LPI_SR_LONG_MCCLK_GATE_WAKEUP_F2_OFFSET 16
#define LPI_SR_LONG_MCCLK_GATE_WAKEUP_F2_WIDTH 4
#define LPI_SR_LONG_MCCLK_GATE_WAKEUP_WIDTH 4
#define LPI_SR_LONG_WAKEUP_F0_OFFSET   24
#define LPI_SR_LONG_WAKEUP_F0_WIDTH    4
#define LPI_SR_LONG_WAKEUP_F1_OFFSET   0
#define LPI_SR_LONG_WAKEUP_F1_WIDTH    4
#define LPI_SR_LONG_WAKEUP_F2_OFFSET   8
#define LPI_SR_LONG_WAKEUP_F2_WIDTH    4
#define LPI_SR_LONG_WAKEUP_WIDTH       4
#define LPI_SR_SHORT_WAKEUP_F0_OFFSET  16
#define LPI_SR_SHORT_WAKEUP_F0_WIDTH   4
#define LPI_SR_SHORT_WAKEUP_F1_OFFSET  24
#define LPI_SR_SHORT_WAKEUP_F1_WIDTH   4
#define LPI_SR_SHORT_WAKEUP_F2_OFFSET  0
#define LPI_SR_SHORT_WAKEUP_F2_WIDTH   4
#define LPI_SR_SHORT_WAKEUP_WIDTH      4
#define LPI_SRPD_LONG_MCCLK_GATE_WAKEUP_F0_OFFSET 0
#define LPI_SRPD_LONG_MCCLK_GATE_WAKEUP_F0_WIDTH 4
#define LPI_SRPD_LONG_MCCLK_GATE_WAKEUP_F1_OFFSET 8
#define LPI_SRPD_LONG_MCCLK_GATE_WAKEUP_F1_WIDTH 4
#define LPI_SRPD_LONG_MCCLK_GATE_WAKEUP_F2_OFFSET 16
#define LPI_SRPD_LONG_MCCLK_GATE_WAKEUP_F2_WIDTH 4
#define LPI_SRPD_LONG_MCCLK_GATE_WAKEUP_WIDTH 4
#define LPI_SRPD_LONG_WAKEUP_F0_OFFSET 24
#define LPI_SRPD_LONG_WAKEUP_F0_WIDTH  4
#define LPI_SRPD_LONG_WAKEUP_F1_OFFSET 0
#define LPI_SRPD_LONG_WAKEUP_F1_WIDTH  4
#define LPI_SRPD_LONG_WAKEUP_F2_OFFSET 8
#define LPI_SRPD_LONG_WAKEUP_F2_WIDTH  4
#define LPI_SRPD_LONG_WAKEUP_WIDTH     4
#define LPI_SRPD_SHORT_WAKEUP_F0_OFFSET 16
#define LPI_SRPD_SHORT_WAKEUP_F0_WIDTH 4
#define LPI_SRPD_SHORT_WAKEUP_F1_OFFSET 24
#define LPI_SRPD_SHORT_WAKEUP_F1_WIDTH 4
#define LPI_SRPD_SHORT_WAKEUP_F2_OFFSET 0
#define LPI_SRPD_SHORT_WAKEUP_F2_WIDTH 4
#define LPI_SRPD_SHORT_WAKEUP_WIDTH    4
#define LPI_TIMER_WAKEUP_F0_OFFSET     8
#define LPI_TIMER_WAKEUP_F0_WIDTH      4
#define LPI_TIMER_WAKEUP_F1_OFFSET     16
#define LPI_TIMER_WAKEUP_F1_WIDTH      4
#define LPI_TIMER_WAKEUP_F2_OFFSET     24
#define LPI_TIMER_WAKEUP_F2_WIDTH      4
#define LPI_TIMER_WAKEUP_WIDTH         4
#define LPI_WAKEUP_EN_ADDR             166
#define LPI_WAKEUP_EN_OFFSET           0
#define LPI_WAKEUP_EN_WIDTH            6
#define LPI_WAKEUP_TIMEOUT_OFFSET      16
#define LPI_WAKEUP_TIMEOUT_WIDTH       12
#define LVL_DONE_BIT                   20
#define LVL_PATTERN_WIDTH              4
#define MASKED_WRITE_BIT               1
#define MASKED_WRITE_CMD               2
#define MASTER_LONG_COUNTER_WIDTH      10
#define MAX_ADDR_WIDTH                 37
#define MAX_BANK_PER_CMD               2
#define MAX_BANK_PER_CMD_WIDTH         1
#define MAX_BG                         8
#define MAX_BG_PER_CS                  4
#define MAX_BST_ADDR_WIDTH             7
#define MAX_BST_CYCLES                 16
#define MAX_BST_WIDTH                  6
#define MAX_BYTE_WORD                  2
#define MAX_CMD_QUE                    16
#define MAX_CMD_REG_WIDTH              284
#define MAX_CMD_WIDTH                  4
#define MAX_COL                        13
#define MAX_COL_REG_OFFSET             8
#define MAX_COL_REG_WIDTH              4
#define MAX_CS                         2
#define MAX_CS_REG_OFFSET              16
#define MAX_CS_REG_WIDTH               2
#define MAX_LEN_WIDTH                  12
#define MAX_LEN_WORD_WIDTH             10
#define MAX_MEM_BURSTS                 128
#define MAX_MEM_BURSTS_P               48
#define MAX_NUM_DEV_P_CS_WIDTH         3
#define MAX_RANK                       2
#define MAX_REG_ADDR                   6027
#define MAX_ROW_REG_OFFSET             0
#define MAX_ROW_REG_WIDTH              5
#define MAX_USER_ADDR_WIDTH            36
#define MAXIMUM_BYTE_REQUEST           4096
#define MDQS_MULT_WIDTH                2
#define MEM_ADDRESS_WIDTH              16
#define MEM_BANK_GROUP_WIDTH           2
#define MEM_BANK_WIDTH                 3
#define MEM_DATA_LOG2_WIDTH            5
#define MEM_DATA_WIDTH                 32
#define MEM_DM_WIDTH                   4
#define MEM_RST_VALID_BIT              0
#define MEM_RST_VALID_OFFSET           16
#define MEM_RST_VALID_WIDTH            1
#define MEMCD_RMODW_FIFO_DEPTH         32
#define MEMCD_RMODW_FIFO_DEPTH_OFFSET  0
#define MEMCD_RMODW_FIFO_DEPTH_WIDTH   16
#define MEMCD_RMODW_FIFO_PTR_WIDTH     5
#define MEMCD_RMODW_FIFO_PTR_WIDTH_OFFSET 16
#define MEMCD_RMODW_FIFO_PTR_WIDTH_WIDTH 8
#define MEMDATA_RATIO_0_ADDR           331
#define MEMDATA_RATIO_0_OFFSET         8
#define MEMDATA_RATIO_0_WIDTH          3
#define MEMDATA_RATIO_1_ADDR           332
#define MEMDATA_RATIO_1_OFFSET         16
#define MEMDATA_RATIO_1_WIDTH          3
#define MEMDATA_RATIO_WIDTH            3
#define MPM_P                          258
#define MPMWIDTH                       15
#define MPR_CNT_WIDTH                  3
#define MPR_NUM_WIDTH                  2
#define MPR_PAGE_WIDTH                 2
#define MPR_WIDTH                      8
#define MPRR_DATA_0_OFFSET             0
#define MPRR_DATA_0_WIDTH              32
#define MPRR_DATA_1_OFFSET             0
#define MPRR_DATA_1_WIDTH              32
#define MPRR_DATA_2_OFFSET             0
#define MPRR_DATA_2_WIDTH              32
#define MPRR_DATA_3_OFFSET             0
#define MPRR_DATA_3_WIDTH              32
#define MPRR_DATA_WIDTH                32
#define MPRR_DONE_BIT                  23
#define MPRR_RD_DATA_WIDTH             32
#define MPRR_SW_PROMOTE_THRESHOLD_F0_OFFSET 0
#define MPRR_SW_PROMOTE_THRESHOLD_F0_WIDTH 16
#define MPRR_SW_PROMOTE_THRESHOLD_F1_OFFSET 16
#define MPRR_SW_PROMOTE_THRESHOLD_F1_WIDTH 16
#define MPRR_SW_PROMOTE_THRESHOLD_F2_OFFSET 0
#define MPRR_SW_PROMOTE_THRESHOLD_F2_WIDTH 16
#define MPRR_SW_PROMOTE_THRESHOLD_WIDTH 16
#define MR0_DATA_F0_0_OFFSET           0
#define MR0_DATA_F0_0_WIDTH            17
#define MR0_DATA_F0_1_OFFSET           0
#define MR0_DATA_F0_1_WIDTH            17
#define MR0_DATA_F1_0_OFFSET           0
#define MR0_DATA_F1_0_WIDTH            17
#define MR0_DATA_F1_1_OFFSET           0
#define MR0_DATA_F1_1_WIDTH            17
#define MR0_DATA_F2_0_OFFSET           0
#define MR0_DATA_F2_0_WIDTH            17
#define MR0_DATA_F2_1_OFFSET           0
#define MR0_DATA_F2_1_WIDTH            17
#define MR0_DATA_WIDTH                 17
#define MR1_DATA_F0_0_OFFSET           0
#define MR1_DATA_F0_0_WIDTH            17
#define MR1_DATA_F0_1_OFFSET           0
#define MR1_DATA_F0_1_WIDTH            17
#define MR1_DATA_F1_0_OFFSET           0
#define MR1_DATA_F1_0_WIDTH            17
#define MR1_DATA_F1_1_OFFSET           0
#define MR1_DATA_F1_1_WIDTH            17
#define MR1_DATA_F2_0_OFFSET           0
#define MR1_DATA_F2_0_WIDTH            17
#define MR1_DATA_F2_1_OFFSET           0
#define MR1_DATA_F2_1_WIDTH            17
#define MR1_DATA_WIDTH                 17
#define MR2_DATA_0_WIDTH               17
#define MR2_DATA_F0_0_OFFSET           0
#define MR2_DATA_F0_0_WIDTH            17
#define MR2_DATA_F0_1_OFFSET           0
#define MR2_DATA_F0_1_WIDTH            17
#define MR2_DATA_F1_0_OFFSET           0
#define MR2_DATA_F1_0_WIDTH            17
#define MR2_DATA_F1_1_OFFSET           0
#define MR2_DATA_F1_1_WIDTH            17
#define MR2_DATA_F2_0_OFFSET           0
#define MR2_DATA_F2_0_WIDTH            17
#define MR2_DATA_F2_1_OFFSET           0
#define MR2_DATA_F2_1_WIDTH            17
#define MR2_DATA_WIDTH                 17
#define MR3_DATA_F0_0_OFFSET           0
#define MR3_DATA_F0_0_WIDTH            17
#define MR3_DATA_F0_1_OFFSET           0
#define MR3_DATA_F0_1_WIDTH            17
#define MR3_DATA_F1_0_OFFSET           0
#define MR3_DATA_F1_0_WIDTH            17
#define MR3_DATA_F1_1_OFFSET           0
#define MR3_DATA_F1_1_WIDTH            17
#define MR3_DATA_F2_0_OFFSET           0
#define MR3_DATA_F2_0_WIDTH            17
#define MR3_DATA_F2_1_OFFSET           0
#define MR3_DATA_F2_1_WIDTH            17
#define MR3_DATA_WIDTH                 17
#define MR4_DATA_F0_0_OFFSET           0
#define MR4_DATA_F0_0_WIDTH            17
#define MR4_DATA_F0_1_OFFSET           0
#define MR4_DATA_F0_1_WIDTH            17
#define MR4_DATA_F1_0_OFFSET           0
#define MR4_DATA_F1_0_WIDTH            17
#define MR4_DATA_F1_1_OFFSET           0
#define MR4_DATA_F1_1_WIDTH            17
#define MR4_DATA_F2_0_OFFSET           0
#define MR4_DATA_F2_0_WIDTH            17
#define MR4_DATA_F2_1_OFFSET           0
#define MR4_DATA_F2_1_WIDTH            17
#define MR4_DATA_WIDTH                 17
#define MR4_DLL_RST_OFFSET             0
#define MR4_DLL_RST_WIDTH              1
#define MR5_DATA_F0_0_OFFSET           0
#define MR5_DATA_F0_0_WIDTH            17
#define MR5_DATA_F0_1_OFFSET           0
#define MR5_DATA_F0_1_WIDTH            17
#define MR5_DATA_F1_0_OFFSET           0
#define MR5_DATA_F1_0_WIDTH            17
#define MR5_DATA_F1_1_OFFSET           0
#define MR5_DATA_F1_1_WIDTH            17
#define MR5_DATA_F2_0_OFFSET           0
#define MR5_DATA_F2_0_WIDTH            17
#define MR5_DATA_F2_1_OFFSET           0
#define MR5_DATA_F2_1_WIDTH            17
#define MR5_DATA_WIDTH                 17
#define MR6_DATA_F0_0_OFFSET           0
#define MR6_DATA_F0_0_WIDTH            17
#define MR6_DATA_F0_1_OFFSET           0
#define MR6_DATA_F0_1_WIDTH            17
#define MR6_DATA_F1_0_OFFSET           0
#define MR6_DATA_F1_0_WIDTH            17
#define MR6_DATA_F1_1_OFFSET           0
#define MR6_DATA_F1_1_WIDTH            17
#define MR6_DATA_F2_0_OFFSET           0
#define MR6_DATA_F2_0_WIDTH            17
#define MR6_DATA_F2_1_OFFSET           0
#define MR6_DATA_F2_1_WIDTH            17
#define MR6_DATA_WIDTH                 17
#define MR8_DATA_0_OFFSET              0
#define MR8_DATA_0_WIDTH               17
#define MR8_DATA_1_OFFSET              0
#define MR8_DATA_1_WIDTH               17
#define MR8_DATA_WIDTH                 17
#define MR11_DATA_F0_0_OFFSET          24
#define MR11_DATA_F0_0_WIDTH           8
#define MR11_DATA_F0_1_OFFSET          24
#define MR11_DATA_F0_1_WIDTH           8
#define MR11_DATA_F1_0_OFFSET          0
#define MR11_DATA_F1_0_WIDTH           8
#define MR11_DATA_F1_1_OFFSET          0
#define MR11_DATA_F1_1_WIDTH           8
#define MR11_DATA_F2_0_OFFSET          8
#define MR11_DATA_F2_0_WIDTH           8
#define MR11_DATA_F2_1_OFFSET          8
#define MR11_DATA_F2_1_WIDTH           8
#define MR11_DATA_WIDTH                8
#define MR12_DATA_F0_0_OFFSET          0
#define MR12_DATA_F0_0_WIDTH           17
#define MR12_DATA_F0_1_OFFSET          0
#define MR12_DATA_F0_1_WIDTH           17
#define MR12_DATA_F1_0_OFFSET          0
#define MR12_DATA_F1_0_WIDTH           17
#define MR12_DATA_F1_1_OFFSET          0
#define MR12_DATA_F1_1_WIDTH           17
#define MR12_DATA_F2_0_OFFSET          0
#define MR12_DATA_F2_0_WIDTH           17
#define MR12_DATA_F2_1_OFFSET          0
#define MR12_DATA_F2_1_WIDTH           17
#define MR12_DATA_WIDTH                17
#define MR13_DATA_0_ADDR               232
#define MR13_DATA_0_OFFSET             0
#define MR13_DATA_0_WIDTH              17
#define MR13_DATA_1_ADDR               267
#define MR13_DATA_1_OFFSET             0
#define MR13_DATA_1_WIDTH              17
#define MR13_DATA_WIDTH                17
#define MR14_DATA_F0_0_OFFSET          0
#define MR14_DATA_F0_0_WIDTH           17
#define MR14_DATA_F0_1_OFFSET          0
#define MR14_DATA_F0_1_WIDTH           17
#define MR14_DATA_F1_0_OFFSET          0
#define MR14_DATA_F1_0_WIDTH           17
#define MR14_DATA_F1_1_OFFSET          0
#define MR14_DATA_F1_1_WIDTH           17
#define MR14_DATA_F2_0_OFFSET          0
#define MR14_DATA_F2_0_WIDTH           17
#define MR14_DATA_F2_1_OFFSET          0
#define MR14_DATA_F2_1_WIDTH           17
#define MR14_DATA_WIDTH                17
#define MR16_DATA_0_OFFSET             24
#define MR16_DATA_0_WIDTH              8
#define MR16_DATA_1_OFFSET             24
#define MR16_DATA_1_WIDTH              8
#define MR16_DATA_WIDTH                8
#define MR17_DATA_0_OFFSET             0
#define MR17_DATA_0_WIDTH              8
#define MR17_DATA_1_OFFSET             0
#define MR17_DATA_1_WIDTH              8
#define MR17_DATA_WIDTH                8
#define MR20_DATA_0_OFFSET             8
#define MR20_DATA_0_WIDTH              8
#define MR20_DATA_1_OFFSET             8
#define MR20_DATA_1_WIDTH              8
#define MR20_DATA_WIDTH                8
#define MR22_DATA_F0_0_OFFSET          0
#define MR22_DATA_F0_0_WIDTH           17
#define MR22_DATA_F0_1_OFFSET          0
#define MR22_DATA_F0_1_WIDTH           17
#define MR22_DATA_F1_0_OFFSET          0
#define MR22_DATA_F1_0_WIDTH           17
#define MR22_DATA_F1_1_OFFSET          0
#define MR22_DATA_F1_1_WIDTH           17
#define MR22_DATA_F2_0_OFFSET          0
#define MR22_DATA_F2_0_WIDTH           17
#define MR22_DATA_F2_1_OFFSET          0
#define MR22_DATA_F2_1_WIDTH           17
#define MR22_DATA_WIDTH                17
#define MR23_DATA_OFFSET               0
#define MR23_DATA_WIDTH                17
#define MR_DATA_WIDTH                  17
#define MR_FSP_DATA_VALID_F0_OFFSET    24
#define MR_FSP_DATA_VALID_F0_WIDTH     1
#define MR_FSP_DATA_VALID_F1_OFFSET    0
#define MR_FSP_DATA_VALID_F1_WIDTH     1
#define MR_FSP_DATA_VALID_F2_OFFSET    8
#define MR_FSP_DATA_VALID_F2_WIDTH     1
#define MR_FSP_DATA_VALID_WIDTH        1
#define MRR_CMD_SOURCE_WIDTH           3
#define MRR_DATA_MASK_WIDTH            32
#define MRR_DATA_WIDTH                 8
#define MRR_ERROR_BIT                  14
#define MRR_ERROR_STATUS_OFFSET        0
#define MRR_ERROR_STATUS_WIDTH         2
#define MRR_LSB_REG_OFFSET             24
#define MRR_LSB_REG_WIDTH              8
#define MRR_MSB_REG_OFFSET             0
#define MRR_MSB_REG_WIDTH              8
#define MRR_PROMOTE_THRESHOLD_F0_OFFSET 0
#define MRR_PROMOTE_THRESHOLD_F0_WIDTH 16
#define MRR_PROMOTE_THRESHOLD_F1_OFFSET 16
#define MRR_PROMOTE_THRESHOLD_F1_WIDTH 16
#define MRR_PROMOTE_THRESHOLD_F2_OFFSET 0
#define MRR_PROMOTE_THRESHOLD_F2_WIDTH 16
#define MRR_PROMOTE_THRESHOLD_WIDTH    16
#define MRR_TEMPCHK_HIGH_THRESHOLD_F0_OFFSET 16
#define MRR_TEMPCHK_HIGH_THRESHOLD_F0_WIDTH 16
#define MRR_TEMPCHK_HIGH_THRESHOLD_F1_OFFSET 0
#define MRR_TEMPCHK_HIGH_THRESHOLD_F1_WIDTH 16
#define MRR_TEMPCHK_HIGH_THRESHOLD_F2_OFFSET 16
#define MRR_TEMPCHK_HIGH_THRESHOLD_F2_WIDTH 16
#define MRR_TEMPCHK_HIGH_THRESHOLD_WIDTH 16
#define MRR_TEMPCHK_NORM_THRESHOLD_F0_OFFSET 0
#define MRR_TEMPCHK_NORM_THRESHOLD_F0_WIDTH 16
#define MRR_TEMPCHK_NORM_THRESHOLD_F1_OFFSET 16
#define MRR_TEMPCHK_NORM_THRESHOLD_F1_WIDTH 16
#define MRR_TEMPCHK_NORM_THRESHOLD_F2_OFFSET 0
#define MRR_TEMPCHK_NORM_THRESHOLD_F2_WIDTH 16
#define MRR_TEMPCHK_NORM_THRESHOLD_WIDTH 16
#define MRR_TEMPCHK_TIMEOUT_F0_OFFSET  0
#define MRR_TEMPCHK_TIMEOUT_F0_WIDTH   16
#define MRR_TEMPCHK_TIMEOUT_F1_OFFSET  16
#define MRR_TEMPCHK_TIMEOUT_F1_WIDTH   16
#define MRR_TEMPCHK_TIMEOUT_F2_OFFSET  0
#define MRR_TEMPCHK_TIMEOUT_F2_WIDTH   16
#define MRR_TEMPCHK_TIMEOUT_WIDTH      16
#define MRS_WIDTH                      8
#define MRSINGLE_DATA_0_OFFSET         0
#define MRSINGLE_DATA_0_WIDTH          17
#define MRSINGLE_DATA_1_OFFSET         0
#define MRSINGLE_DATA_1_WIDTH          17
#define MRSINGLE_DATA_WIDTH            17
#define MRW_ARB_CALVL                  11
#define MRW_ARB_FREQ_CHANGE            4
#define MRW_ARB_FREQ_CHANGE_FSP        6
#define MRW_ARB_FSP_SET_OP             5
#define MRW_ARB_MPRR                   12
#define MRW_ARB_NUM_REQ                15
#define MRW_ARB_PWR_ON_DLLRST          2
#define MRW_ARB_PWR_ON_MR63            3
#define MRW_ARB_PWR_ON_SM              1
#define MRW_ARB_RDLVL                  9
#define MRW_ARB_SOFTWARE               0
#define MRW_ARB_VREF                   10
#define MRW_ARB_WRLVL                  8
#define MRW_ARB_ZQ                     7
#define MRW_CS_WIDTH                   8
#define MRW_CSNUM_END                  8
#define MRW_CSNUM_START                15
#define MRW_DFS_UPDATE_FRC_OFFSET      0
#define MRW_DFS_UPDATE_FRC_WIDTH       2
#define MRW_MODEBIT_ALL                16
#define MRW_MODEBIT_BASE               17
#define MRW_MODEBIT_FSP_UPDATE         26
#define MRW_MODEBIT_OW                 19
#define MRW_MODEBIT_PASR               18
#define MRW_MODEBIT_SINGLE             23
#define MRW_MODEBIT_START              25
#define MRW_MODEBIT_UPDATEALL          24
#define MRW_PROMOTE_THRESHOLD_F0_OFFSET 16
#define MRW_PROMOTE_THRESHOLD_F0_WIDTH 16
#define MRW_PROMOTE_THRESHOLD_F1_OFFSET 0
#define MRW_PROMOTE_THRESHOLD_F1_WIDTH 16
#define MRW_PROMOTE_THRESHOLD_F2_OFFSET 16
#define MRW_PROMOTE_THRESHOLD_F2_WIDTH 16
#define MRW_PROMOTE_THRESHOLD_WIDTH    16
#define MRW_REGNUM_END                 0
#define MRW_REGNUM_START               7
#define MRW_REGNUM_WIDTH               8
#define MRW_STATUS_OFFSET              0
#define MRW_STATUS_WIDTH               8
#define MRW_WIDE_CS_ENABLE_OFFSET      24
#define MRW_WIDE_CS_ENABLE_WIDTH       1
#define MULTI_BG_REQ_P                 3
#define MULTI_CHANNEL_ZQ_CAL_MASTER_OFFSET 8
#define MULTI_CHANNEL_ZQ_CAL_MASTER_WIDTH 1
#define NO_AUTO_MRR_INIT_OFFSET        24
#define NO_AUTO_MRR_INIT_WIDTH         1
#define NO_MEMORY_DM_OFFSET            0
#define NO_MEMORY_DM_WIDTH             1
#define NO_MRW_INIT_OFFSET             16
#define NO_MRW_INIT_WIDTH              1
#define NO_ZQ_INIT_OFFSET              16
#define NO_ZQ_INIT_WIDTH               1
#define NON_INHIBIT_CMD_WIDTH          7
#define NUM_AXI_PORTS                  4
#define NUM_BANK                       32
#define NUM_BANK_PER_ROTATION_GROUP    16
#define NUM_COMMAND_MUX_SOURCES        3
#define NUM_CRC_DEVICES                4
#define NUM_CTLR_PORTS                 4
#define NUM_DFS_REG_COPIES             3
#define NUM_DRAM_ERROR_PADS            1
#define NUM_FREQ_CHANGE_REG_COPIES     3
#define NUM_INT_SOURCES                45
#define NUM_PORTS                      4
#define NUM_PQ_PORTS                   4
#define NUM_PRIORITIES                 16
#define NUM_Q_ENTRIES_ACT_DISABLE_OFFSET 16
#define NUM_Q_ENTRIES_ACT_DISABLE_WIDTH 4
#define NUM_REG_DIMM_PARITY_PADS       1
#define NUM_TEST_AXI_PORTS             4
#define NUM_X4_DATA_DRAMS              4
#define NWR_F0_OFFSET                  0
#define NWR_F0_WIDTH                   8
#define NWR_F1_OFFSET                  8
#define NWR_F1_WIDTH                   8
#define NWR_F2_OFFSET                  16
#define NWR_F2_WIDTH                   8
#define NWR_WIDTH                      8
#define ODT_EN_F0_OFFSET               16
#define ODT_EN_F0_WIDTH                1
#define ODT_EN_F1_OFFSET               24
#define ODT_EN_F1_WIDTH                1
#define ODT_EN_F2_OFFSET               0
#define ODT_EN_F2_WIDTH                1
#define ODT_EN_WIDTH                   1
#define ODT_RD_MAP_CS0_OFFSET          8
#define ODT_RD_MAP_CS0_WIDTH           2
#define ODT_RD_MAP_CS1_OFFSET          24
#define ODT_RD_MAP_CS1_WIDTH           2
#define ODT_TICK_MINUS_ADJ_OFFSET      8
#define ODT_TICK_MINUS_ADJ_WIDTH       4
#define ODT_TICK_PLUS_ADJ_OFFSET       0
#define ODT_TICK_PLUS_ADJ_WIDTH        4
#define ODT_VALUE_OFFSET               24
#define ODT_VALUE_WIDTH                1
#define ODT_WR_MAP_CS0_OFFSET          16
#define ODT_WR_MAP_CS0_WIDTH           2
#define ODT_WR_MAP_CS1_OFFSET          0
#define ODT_WR_MAP_CS1_WIDTH           2
#define OPTIMAL_RMODW_EN_OFFSET        16
#define OPTIMAL_RMODW_EN_WIDTH         1
#define OPTIMAL_RMODW_WRITE_REDUC_EN_OFFSET 24
#define OPTIMAL_RMODW_WRITE_REDUC_EN_WIDTH 1
#define OSC_BASE_VALUE_0_CS0_OFFSET    0
#define OSC_BASE_VALUE_0_CS0_WIDTH     16
#define OSC_BASE_VALUE_0_CS1_OFFSET    0
#define OSC_BASE_VALUE_0_CS1_WIDTH     16
#define OSC_BASE_VALUE_1_CS0_OFFSET    16
#define OSC_BASE_VALUE_1_CS0_WIDTH     16
#define OSC_BASE_VALUE_1_CS1_OFFSET    16
#define OSC_BASE_VALUE_1_CS1_WIDTH     16
#define OSC_BASE_VALUE_2_CS0_OFFSET    0
#define OSC_BASE_VALUE_2_CS0_WIDTH     16
#define OSC_BASE_VALUE_2_CS1_OFFSET    0
#define OSC_BASE_VALUE_2_CS1_WIDTH     16
#define OSC_BASE_VALUE_3_CS0_OFFSET    16
#define OSC_BASE_VALUE_3_CS0_WIDTH     16
#define OSC_BASE_VALUE_3_CS1_OFFSET    16
#define OSC_BASE_VALUE_3_CS1_WIDTH     16
#define OSC_BASE_VALUE_WIDTH           16
#define OSC_TIME_WIDTH                 16
#define OSC_VARIANCE_LIMIT_OFFSET      0
#define OSC_VARIANCE_LIMIT_WIDTH       16
#define OUT_OF_RANGE_ADDR_OFFSET       0
#define OUT_OF_RANGE_ADDR_WIDTH        36
#define OUT_OF_RANGE_BIT               1
#define OUT_OF_RANGE_LENGTH_OFFSET     8
#define OUT_OF_RANGE_LENGTH_WIDTH      12
#define OUT_OF_RANGE_OVFLOW_BIT        2
#define OUT_OF_RANGE_SOURCE_ID_OFFSET  0
#define OUT_OF_RANGE_SOURCE_ID_WIDTH   18
#define OUT_OF_RANGE_TYPE_OFFSET       24
#define OUT_OF_RANGE_TYPE_WIDTH        7
#define PAGE_WIDTH                     22
#define PARITY_ERROR_INJECT_BIT        24
#define PBR_BANK_SELECT_DELAY_OFFSET   16
#define PBR_BANK_SELECT_DELAY_WIDTH    4
#define PBR_CONT_REQ_EN                1
#define PBR_CONT_REQ_EN_OFFSET         24
#define PBR_CONT_REQ_EN_WIDTH          1
#define PBR_EN_OFFSET                  0
#define PBR_EN_WIDTH                   1
#define PBR_MAX_BANK_WAIT_OFFSET       0
#define PBR_MAX_BANK_WAIT_WIDTH        16
#define PBR_NUMERIC_ORDER_OFFSET       8
#define PBR_NUMERIC_ORDER_WIDTH        1
#define PBR_REQ_WIDTH                  6
#define PERIPHERAL_MRR_DATA_ADDR       186
#define PERIPHERAL_MRR_DATA_OFFSET     0
#define PERIPHERAL_MRR_DATA_WIDTH      40
#define PERIPHERAL_MRR_DONE_BIT        25
#define PHY_INDEP_INIT_MODE_OFFSET     16
#define PHY_INDEP_INIT_MODE_WIDTH      1
#define PHY_INDEP_TRAIN_MODE_OFFSET    0
#define PHY_INDEP_TRAIN_MODE_WIDTH     1
#define PHY_MAX_REG_ADDR_WIDTH         12
#define PHYMSTR_DFI4_PROMOTE_THRESHOLD_F0_OFFSET 0
#define PHYMSTR_DFI4_PROMOTE_THRESHOLD_F0_WIDTH 16
#define PHYMSTR_DFI4_PROMOTE_THRESHOLD_F1_OFFSET 0
#define PHYMSTR_DFI4_PROMOTE_THRESHOLD_F1_WIDTH 16
#define PHYMSTR_DFI4_PROMOTE_THRESHOLD_F2_OFFSET 0
#define PHYMSTR_DFI4_PROMOTE_THRESHOLD_F2_WIDTH 16
#define PHYMSTR_DFI4_PROMOTE_THRESHOLD_WIDTH 16
#define PHYMSTR_DFI_VERSION_4P0V1_OFFSET 8
#define PHYMSTR_DFI_VERSION_4P0V1_WIDTH 1
#define PHYMSTR_ERROR_BIT              15
#define PHYMSTR_ERROR_STATUS_OFFSET    0
#define PHYMSTR_ERROR_STATUS_WIDTH     2
#define PHYMSTR_LPC_SR_LEVEL_WIDTH     1
#define PHYMSTR_NO_AREF_OFFSET         24
#define PHYMSTR_NO_AREF_WIDTH          1
#define PHYMSTR_TRAIN_AFTER_INIT_COMPLETE_OFFSET 16
#define PHYMSTR_TRAIN_AFTER_INIT_COMPLETE_WIDTH 1
#define PHYUPD_APPEND_EN_OFFSET        0
#define PHYUPD_APPEND_EN_WIDTH         1
#define PLACEMENT_EN_OFFSET            24
#define PLACEMENT_EN_WIDTH             1
#define PORT0_BYTE_WIDTH               4
#define PORT0_DATA_WIDTH               128
#define PORT0_MASK_WIDTH               16
#define PORT1_BYTE_WIDTH               4
#define PORT1_DATA_WIDTH               128
#define PORT1_MASK_WIDTH               16
#define PORT2_BYTE_WIDTH               4
#define PORT2_DATA_WIDTH               128
#define PORT2_MASK_WIDTH               16
#define PORT3_BYTE_WIDTH               4
#define PORT3_DATA_WIDTH               128
#define PORT3_MASK_WIDTH               16
#define PORT_BEAT_SIZE_WIDTH           3
#define PORT_CMD_ERR_BIT               3
#define PORT_CMD_ERROR_ADDR_OFFSET     0
#define PORT_CMD_ERROR_ADDR_WIDTH      36
#define PORT_CMD_ERROR_ID_OFFSET       8
#define PORT_CMD_ERROR_ID_WIDTH        18
#define PORT_CMD_ERROR_TYPE_OFFSET     0
#define PORT_CMD_ERROR_TYPE_WIDTH      2
#define PORT_ERROR_ID_WIDTH            18
#define PORT_SPLIT_BLOCK_SIZE_WIDTH    7
#define PORT_WIDTH                     2
#define PORTWIDTH                      2
#define PPR_BANK_ADDRESS_OFFSET        0
#define PPR_BANK_ADDRESS_WIDTH         4
#define PPR_COMMAND_MRW_OFFSET         0
#define PPR_COMMAND_MRW_WIDTH          8
#define PPR_COMMAND_WIDTH              3
#define PPR_CONTROL_OFFSET             16
#define PPR_CONTROL_WIDTH              1
#define PPR_CS_ADDRESS_OFFSET          8
#define PPR_CS_ADDRESS_WIDTH           1
#define PPR_DATA_OFFSET                0
#define PPR_DATA_WIDTH                 128
#define PPR_ROW_ADDRESS_OFFSET         8
#define PPR_ROW_ADDRESS_WIDTH          17
#define PPR_STATUS_BIT                 42
#define PPR_STATUS_OFFSET              0
#define PPR_STATUS_WIDTH               2
#define PQ_SOURCEWIDTH                 2
#define PRE_2TICK_COUNT_OFFSET         0
#define PRE_2TICK_COUNT_WIDTH          3
#define PRE_4TICK_COUNT_OFFSET         24
#define PRE_4TICK_COUNT_WIDTH          3
#define PREAMBLE_SUPPORT_F0_OFFSET     16
#define PREAMBLE_SUPPORT_F0_WIDTH      2
#define PREAMBLE_SUPPORT_F1_OFFSET     24
#define PREAMBLE_SUPPORT_F1_WIDTH      2
#define PREAMBLE_SUPPORT_F2_OFFSET     0
#define PREAMBLE_SUPPORT_F2_WIDTH      2
#define PREAMBLE_SUPPORT_WIDTH         2
#define PRIORITY_EN_OFFSET             0
#define PRIORITY_EN_WIDTH              1
#define PRIORITY_P                     227
#define PRIORITY_SLIDE_P               210
#define PRIORITYWIDTH                  4
#define PRIORITYWIDTHBIT               17
#define PWRDN_SHIFT_DELAY_OFFSET       8
#define PWRDN_SHIFT_DELAY_WIDTH        9
#define PWRUP_SREFRESH_EXIT_OFFSET     0
#define PWRUP_SREFRESH_EXIT_WIDTH      1
#define Q_FULLNESS_OFFSET              24
#define Q_FULLNESS_WIDTH               4
#define Q_TOTAL_WORDS_P                239
#define R2R_DIFFCS_DLY_F0_OFFSET       24
#define R2R_DIFFCS_DLY_F0_WIDTH        5
#define R2R_DIFFCS_DLY_F1_OFFSET       24
#define R2R_DIFFCS_DLY_F1_WIDTH        5
#define R2R_DIFFCS_DLY_F2_OFFSET       24
#define R2R_DIFFCS_DLY_F2_WIDTH        5
#define R2R_DIFFCS_DLY_WIDTH           5
#define R2R_SAMECS_DLY_OFFSET          24
#define R2R_SAMECS_DLY_WIDTH           5
#define R2W_DIFFCS_DLY_F0_OFFSET       0
#define R2W_DIFFCS_DLY_F0_WIDTH        5
#define R2W_DIFFCS_DLY_F1_OFFSET       0
#define R2W_DIFFCS_DLY_F1_WIDTH        5
#define R2W_DIFFCS_DLY_F2_OFFSET       0
#define R2W_DIFFCS_DLY_F2_WIDTH        5
#define R2W_DIFFCS_DLY_WIDTH           5
#define R2W_SAMECS_DLY_F0_OFFSET       0
#define R2W_SAMECS_DLY_F0_WIDTH        5
#define R2W_SAMECS_DLY_F1_OFFSET       8
#define R2W_SAMECS_DLY_F1_WIDTH        5
#define R2W_SAMECS_DLY_F2_OFFSET       16
#define R2W_SAMECS_DLY_F2_WIDTH        5
#define R2W_SAMECS_DLY_WIDTH           5
#define RAS_PARITY_ERROR_INJECT_BIT    22
#define RD_DBI_EN_OFFSET               24
#define RD_DBI_EN_WIDTH                1
#define RD_PREAMBLE_TRAINING_EN_OFFSET 8
#define RD_PREAMBLE_TRAINING_EN_WIDTH  1
#define RD_TO_ODTH_F0_OFFSET           8
#define RD_TO_ODTH_F0_WIDTH            6
#define RD_TO_ODTH_F1_OFFSET           16
#define RD_TO_ODTH_F1_WIDTH            6
#define RD_TO_ODTH_F2_OFFSET           24
#define RD_TO_ODTH_F2_WIDTH            6
#define RD_TO_ODTH_WIDTH               6
#define RDCSLAT_MAX                    255
#define RDDATA_EN_MAX                  256
#define RDDATA_GATHER_FIFO_PTR_WIDTH   4
#define RDDATA_INFO_FIFO_PTR_WIDTH     6
#define RDDATA_INFO_FIFO_WIDTH         74
#define RDLAT_ADJ_F0_OFFSET            0
#define RDLAT_ADJ_F0_WIDTH             8
#define RDLAT_ADJ_F1_OFFSET            0
#define RDLAT_ADJ_F1_WIDTH             8
#define RDLAT_ADJ_F2_OFFSET            0
#define RDLAT_ADJ_F2_WIDTH             8
#define RDLAT_ADJ_WIDTH                8
#define RDLAT_WIDTH                    9
#define RDLVL_AREF_EN_OFFSET           8
#define RDLVL_AREF_EN_WIDTH            1
#define RDLVL_CS_MAP_OFFSET            16
#define RDLVL_CS_MAP_WIDTH             2
#define RDLVL_CS_OFFSET                16
#define RDLVL_CS_WIDTH                 1
#define RDLVL_DFI_PROMOTE_THRESHOLD_F0_OFFSET 0
#define RDLVL_DFI_PROMOTE_THRESHOLD_F0_WIDTH 16
#define RDLVL_DFI_PROMOTE_THRESHOLD_F1_OFFSET 0
#define RDLVL_DFI_PROMOTE_THRESHOLD_F1_WIDTH 16
#define RDLVL_DFI_PROMOTE_THRESHOLD_F2_OFFSET 0
#define RDLVL_DFI_PROMOTE_THRESHOLD_F2_WIDTH 16
#define RDLVL_DFI_PROMOTE_THRESHOLD_WIDTH 16
#define RDLVL_EN_OFFSET                8
#define RDLVL_EN_WIDTH                 1
#define RDLVL_ERROR_BIT                9
#define RDLVL_ERROR_STATUS_OFFSET      0
#define RDLVL_ERROR_STATUS_WIDTH       3
#define RDLVL_FORMAT_0_OFFSET          0
#define RDLVL_FORMAT_0_WIDTH           2
#define RDLVL_FORMAT_1_OFFSET          0
#define RDLVL_FORMAT_1_WIDTH           2
#define RDLVL_FORMAT_2_OFFSET          0
#define RDLVL_FORMAT_2_WIDTH           2
#define RDLVL_FORMAT_3_OFFSET          0
#define RDLVL_FORMAT_3_WIDTH           2
#define RDLVL_FORMAT_4_OFFSET          0
#define RDLVL_FORMAT_4_WIDTH           2
#define RDLVL_FORMAT_5_OFFSET          0
#define RDLVL_FORMAT_5_WIDTH           2
#define RDLVL_FORMAT_6_OFFSET          0
#define RDLVL_FORMAT_6_WIDTH           2
#define RDLVL_FORMAT_7_OFFSET          0
#define RDLVL_FORMAT_7_WIDTH           2
#define RDLVL_FORMAT_WIDTH             2
#define RDLVL_GATE_AREF_EN_OFFSET      16
#define RDLVL_GATE_AREF_EN_WIDTH       1
#define RDLVL_GATE_CS_MAP_OFFSET       24
#define RDLVL_GATE_CS_MAP_WIDTH        2
#define RDLVL_GATE_DFI_PROMOTE_THRESHOLD_F0_OFFSET 16
#define RDLVL_GATE_DFI_PROMOTE_THRESHOLD_F0_WIDTH 16
#define RDLVL_GATE_DFI_PROMOTE_THRESHOLD_F1_OFFSET 16
#define RDLVL_GATE_DFI_PROMOTE_THRESHOLD_F1_WIDTH 16
#define RDLVL_GATE_DFI_PROMOTE_THRESHOLD_F2_OFFSET 16
#define RDLVL_GATE_DFI_PROMOTE_THRESHOLD_F2_WIDTH 16
#define RDLVL_GATE_DFI_PROMOTE_THRESHOLD_WIDTH 16
#define RDLVL_GATE_EN_OFFSET           16
#define RDLVL_GATE_EN_WIDTH            1
#define RDLVL_GATE_ERROR_BIT           10
#define RDLVL_GATE_ERROR_STATUS_OFFSET 8
#define RDLVL_GATE_ERROR_STATUS_WIDTH  3
#define RDLVL_GATE_HIGH_THRESHOLD_F0_OFFSET 0
#define RDLVL_GATE_HIGH_THRESHOLD_F0_WIDTH 16
#define RDLVL_GATE_HIGH_THRESHOLD_F1_OFFSET 0
#define RDLVL_GATE_HIGH_THRESHOLD_F1_WIDTH 16
#define RDLVL_GATE_HIGH_THRESHOLD_F2_OFFSET 0
#define RDLVL_GATE_HIGH_THRESHOLD_F2_WIDTH 16
#define RDLVL_GATE_HIGH_THRESHOLD_WIDTH 16
#define RDLVL_GATE_NORM_THRESHOLD_F0_OFFSET 16
#define RDLVL_GATE_NORM_THRESHOLD_F0_WIDTH 16
#define RDLVL_GATE_NORM_THRESHOLD_F1_OFFSET 16
#define RDLVL_GATE_NORM_THRESHOLD_F1_WIDTH 16
#define RDLVL_GATE_NORM_THRESHOLD_F2_OFFSET 16
#define RDLVL_GATE_NORM_THRESHOLD_F2_WIDTH 16
#define RDLVL_GATE_NORM_THRESHOLD_WIDTH 16
#define RDLVL_GATE_ON_SREF_EXIT_OFFSET 0
#define RDLVL_GATE_ON_SREF_EXIT_WIDTH  1
#define RDLVL_GATE_PERIODIC_OFFSET     24
#define RDLVL_GATE_PERIODIC_WIDTH      1
#define RDLVL_GATE_REQ_BIT             18
#define RDLVL_GATE_REQ_OFFSET          8
#define RDLVL_GATE_REQ_WIDTH           1
#define RDLVL_GATE_ROTATE_OFFSET       8
#define RDLVL_GATE_ROTATE_WIDTH        1
#define RDLVL_GATE_SEQ_EN_OFFSET       16
#define RDLVL_GATE_SEQ_EN_WIDTH        4
#define RDLVL_GATE_SW_PROMOTE_THRESHOLD_F0_OFFSET 0
#define RDLVL_GATE_SW_PROMOTE_THRESHOLD_F0_WIDTH 16
#define RDLVL_GATE_SW_PROMOTE_THRESHOLD_F1_OFFSET 0
#define RDLVL_GATE_SW_PROMOTE_THRESHOLD_F1_WIDTH 16
#define RDLVL_GATE_SW_PROMOTE_THRESHOLD_F2_OFFSET 0
#define RDLVL_GATE_SW_PROMOTE_THRESHOLD_F2_WIDTH 16
#define RDLVL_GATE_SW_PROMOTE_THRESHOLD_WIDTH 16
#define RDLVL_GATE_TIMEOUT_F0_OFFSET   16
#define RDLVL_GATE_TIMEOUT_F0_WIDTH    16
#define RDLVL_GATE_TIMEOUT_F1_OFFSET   16
#define RDLVL_GATE_TIMEOUT_F1_WIDTH    16
#define RDLVL_GATE_TIMEOUT_F2_OFFSET   16
#define RDLVL_GATE_TIMEOUT_F2_WIDTH    16
#define RDLVL_GATE_TIMEOUT_WIDTH       16
#define RDLVL_HIGH_THRESHOLD_F0_OFFSET 16
#define RDLVL_HIGH_THRESHOLD_F0_WIDTH  16
#define RDLVL_HIGH_THRESHOLD_F1_OFFSET 16
#define RDLVL_HIGH_THRESHOLD_F1_WIDTH  16
#define RDLVL_HIGH_THRESHOLD_F2_OFFSET 16
#define RDLVL_HIGH_THRESHOLD_F2_WIDTH  16
#define RDLVL_HIGH_THRESHOLD_WIDTH     16
#define RDLVL_NORM_THRESHOLD_F0_OFFSET 0
#define RDLVL_NORM_THRESHOLD_F0_WIDTH  16
#define RDLVL_NORM_THRESHOLD_F1_OFFSET 0
#define RDLVL_NORM_THRESHOLD_F1_WIDTH  16
#define RDLVL_NORM_THRESHOLD_F2_OFFSET 0
#define RDLVL_NORM_THRESHOLD_F2_WIDTH  16
#define RDLVL_NORM_THRESHOLD_WIDTH     16
#define RDLVL_ON_SREF_EXIT_OFFSET      16
#define RDLVL_ON_SREF_EXIT_WIDTH       1
#define RDLVL_PAT_0_OFFSET             0
#define RDLVL_PAT_0_WIDTH              64
#define RDLVL_PAT_1_OFFSET             0
#define RDLVL_PAT_1_WIDTH              64
#define RDLVL_PAT_2_OFFSET             0
#define RDLVL_PAT_2_WIDTH              64
#define RDLVL_PAT_3_OFFSET             0
#define RDLVL_PAT_3_WIDTH              64
#define RDLVL_PAT_4_OFFSET             0
#define RDLVL_PAT_4_WIDTH              64
#define RDLVL_PAT_5_OFFSET             0
#define RDLVL_PAT_5_WIDTH              64
#define RDLVL_PAT_6_OFFSET             0
#define RDLVL_PAT_6_WIDTH              64
#define RDLVL_PAT_7_OFFSET             0
#define RDLVL_PAT_7_WIDTH              64
#define RDLVL_PAT_WIDTH                64
#define RDLVL_PERIODIC_OFFSET          8
#define RDLVL_PERIODIC_WIDTH           1
#define RDLVL_REQ_BIT                  17
#define RDLVL_REQ_OFFSET               0
#define RDLVL_REQ_WIDTH                1
#define RDLVL_RESP_MASK_OFFSET         0
#define RDLVL_RESP_MASK_WIDTH          8
#define RDLVL_ROTATE_OFFSET            0
#define RDLVL_ROTATE_WIDTH             1
#define RDLVL_SEQ_EN_OFFSET            8
#define RDLVL_SEQ_EN_WIDTH             4
#define RDLVL_SW_PROMOTE_THRESHOLD_F0_OFFSET 16
#define RDLVL_SW_PROMOTE_THRESHOLD_F0_WIDTH 16
#define RDLVL_SW_PROMOTE_THRESHOLD_F1_OFFSET 16
#define RDLVL_SW_PROMOTE_THRESHOLD_F1_WIDTH 16
#define RDLVL_SW_PROMOTE_THRESHOLD_F2_OFFSET 16
#define RDLVL_SW_PROMOTE_THRESHOLD_F2_WIDTH 16
#define RDLVL_SW_PROMOTE_THRESHOLD_WIDTH 16
#define RDLVL_TIMEOUT_F0_OFFSET        0
#define RDLVL_TIMEOUT_F0_WIDTH         16
#define RDLVL_TIMEOUT_F1_OFFSET        0
#define RDLVL_TIMEOUT_F1_WIDTH         16
#define RDLVL_TIMEOUT_F2_OFFSET        0
#define RDLVL_TIMEOUT_F2_WIDTH         16
#define RDLVL_TIMEOUT_WIDTH            16
#define READ_DATA_FIFO_DEPTH_OFFSET    24
#define READ_DATA_FIFO_DEPTH_WIDTH     8
#define READ_DATA_FIFO_PTR_WIDTH_OFFSET 0
#define READ_DATA_FIFO_PTR_WIDTH_WIDTH 8
#define READ_FIFO_DEPTH                16
#define READ_FIFO_STATUS_WIDTH         8
#define READ_MODEREG_ADDR              185
#define READ_MODEREG_OFFSET            8
#define READ_MODEREG_WIDTH             17
#define READ_MPR_OFFSET                8
#define READ_MPR_WIDTH                 4
#define READ_RESP_WIDTH                2
#define READFIFO_DATA_WIDTH            128
#define REDUC_ADDR                     331
#define REDUC_OFFSET                   0
#define REDUC_WIDTH                    1
#define REG_ADDR_WIDTH                 13
#define REG_BYTE_ADDR_WIDTH            PAR_REG_ADDR_WIDTH + REG_BYTE_WIDTH
#define REG_BYTE_WIDTH                 2
#define REG_DATA_WIDTH                 32
#define REG_DIMM_ENABLE_ADDR           74
#define REG_DIMM_ENABLE_OFFSET         0
#define REG_DIMM_ENABLE_WIDTH          1
#define REG_MASK_WIDTH                 4
#define RFIFO_SRAM_DATA_WIDTH          153
#define RL3_SUPPORT_EN_OFFSET          16
#define RL3_SUPPORT_EN_WIDTH           2
#define RL_TICK_MINUS_ADJ_OFFSET       8
#define RL_TICK_MINUS_ADJ_WIDTH        4
#define RL_TICK_PLUS_ADJ_OFFSET        0
#define RL_TICK_PLUS_ADJ_WIDTH         4
#define RMODW_LEN_WIDTH                8
#define RMODW_NUM_BEATS                16
#define RMODW_WIDTH                    4
#define ROW_DIFF_ADDR                  325
#define ROW_DIFF_OFFSET                8
#define ROW_DIFF_WIDTH                 3
#define ROW_WIDTH                      17
#define RW2MRW_DLY_F0_OFFSET           0
#define RW2MRW_DLY_F0_WIDTH            5
#define RW2MRW_DLY_F1_OFFSET           8
#define RW2MRW_DLY_F1_WIDTH            5
#define RW2MRW_DLY_F2_OFFSET           16
#define RW2MRW_DLY_F2_WIDTH            5
#define RW2MRW_DLY_WIDTH               5
#define RW_BIT                         0
#define RW_SAME_EN_OFFSET              8
#define RW_SAME_EN_WIDTH               1
#define RW_SAME_PAGE_EN_OFFSET         16
#define RW_SAME_PAGE_EN_WIDTH          1
#define SDRAM_END_BANK                 15
#define SDRAM_END_COL                  2
#define SDRAM_END_CS                   36
#define SDRAM_END_ROW                  19
#define SDRAM_ROW_WIDTH                17
#define SDRAM_START_BANK               18
#define SDRAM_START_COL                14
#define SDRAM_START_CS                 36
#define SDRAM_START_ROW                35
#define SELECT_DEPTH                   4
#define SELECT_DEPTH_PLUS_1            5
#define SIM_MEMCD_RMODW_FIFO_DEPTH     32
#define SIM_MEMCD_RMODW_FIFO_PTR_WIDTH 5
#define SOURCE_ID_P                    192
#define SOURCESIZE                     4
#define SOURCEWIDTH                    18
#define SPLIT_STATE_WIDTH              4
#define SR_FIFO_WRDATA_LAT_E1_WIDTH    42
#define SR_FIFO_WRDATA_LAT_E2_WIDTH    1
#define SR_FIFO_WREN_LAT_WIDTH         3
#define SREFRESH_EXIT_NO_REFRESH_OFFSET 8
#define SREFRESH_EXIT_NO_REFRESH_WIDTH 1
#define START_ACT_ROW                  35
#define START_ADDR                     0
#define START_BANK                     18
#define START_COL                      14
#define START_CS                       36
#define START_ENTRY_EQUAL              1
#define START_OFFSET                   0
#define START_ROW                      35
#define START_WIDTH                    1
#define STAT_P                         0
#define STAT_WIDTH                     3
#define STRATEGY_2TICK_COUNT_OFFSET    16
#define STRATEGY_2TICK_COUNT_WIDTH     3
#define STRATEGY_4TICK_COUNT_OFFSET    8
#define STRATEGY_4TICK_COUNT_WIDTH     3
#define SW_LEVELING_MODE_OFFSET        24
#define SW_LEVELING_MODE_WIDTH         3
#define SWAP_EN_OFFSET                 24
#define SWAP_EN_WIDTH                  1
#define SWI_CLK_MODE_WIDTH             2
#define SWI_CMD_WIDTH                  3
#define SWI_DFS_CLK_WIDTH              2
#define SWI_DFS_CMDTYPE_WIDTH          2
#define SWI_DFS_SREF_WIDTH             2
#define SWI_LPC_CLK_WIDTH              2
#define SWI_LPC_CMDTYPE_WIDTH          2
#define SWI_LPC_SREF_WIDTH             2
#define SWI_SR_LEVEL_WIDTH             2
#define SWLVL_OP_DONE_OFFSET           24
#define SWLVL_OP_DONE_WIDTH            1
#define SWLVL_RESP_0_OFFSET            0
#define SWLVL_RESP_0_WIDTH             1
#define SWLVL_RESP_1_OFFSET            8
#define SWLVL_RESP_1_WIDTH             1
#define SWLVL_RESP_2_OFFSET            16
#define SWLVL_RESP_2_WIDTH             1
#define SWLVL_RESP_3_OFFSET            24
#define SWLVL_RESP_3_WIDTH             1
#define SWLVL_RESP_WIDTH               1
#define SYNC_FIFO_DEPTH                4
#define SYNC_FIFO_PTR_WIDTH            2
#define TBST_INT_INTERVAL_OFFSET       0
#define TBST_INT_INTERVAL_WIDTH        3
#define TCACKEH_OFFSET                 16
#define TCACKEH_WIDTH                  5
#define TCACKEL_OFFSET                 8
#define TCACKEL_WIDTH                  5
#define TCAENT_OFFSET                  16
#define TCAENT_WIDTH                   10
#define TCAEXT_OFFSET                  8
#define TCAEXT_WIDTH                   5
#define TCAL_MAX                       2
#define TCAMRD_OFFSET                  0
#define TCAMRD_WIDTH                   6
#define TCCD_L_F0_OFFSET               16
#define TCCD_L_F0_WIDTH                5
#define TCCD_L_F1_OFFSET               16
#define TCCD_L_F1_WIDTH                5
#define TCCD_L_F2_OFFSET               16
#define TCCD_L_F2_WIDTH                5
#define TCCD_L_WIDTH                   5
#define TCCD_OFFSET                    8
#define TCCD_WIDTH                     5
#define TCCDMW_OFFSET                  16
#define TCCDMW_WIDTH                   6
#define TCKCKEL_F0_OFFSET              16
#define TCKCKEL_F0_WIDTH               5
#define TCKCKEL_F1_OFFSET              16
#define TCKCKEL_F1_WIDTH               5
#define TCKCKEL_F2_OFFSET              16
#define TCKCKEL_F2_WIDTH               5
#define TCKCKEL_WIDTH                  5
#define TCKE_F0_OFFSET                 24
#define TCKE_F0_WIDTH                  5
#define TCKE_F1_OFFSET                 24
#define TCKE_F1_WIDTH                  5
#define TCKE_F2_OFFSET                 24
#define TCKE_F2_WIDTH                  5
#define TCKE_WIDTH                     5
#define TCKEHCMD_F0_OFFSET             8
#define TCKEHCMD_F0_WIDTH              5
#define TCKEHCMD_F1_OFFSET             8
#define TCKEHCMD_F1_WIDTH              5
#define TCKEHCMD_F2_OFFSET             8
#define TCKEHCMD_F2_WIDTH              5
#define TCKEHCMD_WIDTH                 5
#define TCKEHCS_F0_OFFSET              24
#define TCKEHCS_F0_WIDTH               5
#define TCKEHCS_F1_OFFSET              8
#define TCKEHCS_F1_WIDTH               5
#define TCKEHCS_F2_OFFSET              24
#define TCKEHCS_F2_WIDTH               5
#define TCKEHCS_WIDTH                  5
#define TCKELCMD_F0_OFFSET             0
#define TCKELCMD_F0_WIDTH              5
#define TCKELCMD_F1_OFFSET             0
#define TCKELCMD_F1_WIDTH              5
#define TCKELCMD_F2_OFFSET             0
#define TCKELCMD_F2_WIDTH              5
#define TCKELCMD_WIDTH                 5
#define TCKELCS_F0_OFFSET              16
#define TCKELCS_F0_WIDTH               5
#define TCKELCS_F1_OFFSET              0
#define TCKELCS_F1_WIDTH               5
#define TCKELCS_F2_OFFSET              16
#define TCKELCS_F2_WIDTH               5
#define TCKELCS_WIDTH                  5
#define TCKELPD_F0_OFFSET              8
#define TCKELPD_F0_WIDTH               5
#define TCKELPD_F1_OFFSET              8
#define TCKELPD_F1_WIDTH               5
#define TCKELPD_F2_OFFSET              8
#define TCKELPD_F2_WIDTH               5
#define TCKELPD_WIDTH                  5
#define TCKESR_F0_OFFSET               0
#define TCKESR_F0_WIDTH                8
#define TCKESR_F1_OFFSET               0
#define TCKESR_F1_WIDTH                8
#define TCKESR_F2_OFFSET               0
#define TCKESR_F2_WIDTH                8
#define TCKESR_WIDTH                   8
#define TCKFSPE_F0_OFFSET              0
#define TCKFSPE_F0_WIDTH               5
#define TCKFSPE_F1_OFFSET              16
#define TCKFSPE_F1_WIDTH               5
#define TCKFSPE_F2_OFFSET              0
#define TCKFSPE_F2_WIDTH               5
#define TCKFSPE_WIDTH                  5
#define TCKFSPX_F0_OFFSET              8
#define TCKFSPX_F0_WIDTH               5
#define TCKFSPX_F1_OFFSET              24
#define TCKFSPX_F1_WIDTH               5
#define TCKFSPX_F2_OFFSET              8
#define TCKFSPX_F2_WIDTH               5
#define TCKFSPX_WIDTH                  5
#define TCMDCKE_F0_OFFSET              24
#define TCMDCKE_F0_WIDTH               5
#define TCMDCKE_F1_OFFSET              24
#define TCMDCKE_F1_WIDTH               5
#define TCMDCKE_F2_OFFSET              24
#define TCMDCKE_F2_WIDTH               5
#define TCMDCKE_WIDTH                  5
#define TCSCKE_F0_OFFSET               8
#define TCSCKE_F0_WIDTH                5
#define TCSCKE_F1_OFFSET               24
#define TCSCKE_F1_WIDTH                5
#define TCSCKE_F2_OFFSET               8
#define TCSCKE_F2_WIDTH                5
#define TCSCKE_WIDTH                   5
#define TCSCKEH_F0_OFFSET              16
#define TCSCKEH_F0_WIDTH               5
#define TCSCKEH_F1_OFFSET              16
#define TCSCKEH_F1_WIDTH               5
#define TCSCKEH_F2_OFFSET              16
#define TCSCKEH_F2_WIDTH               5
#define TCSCKEH_WIDTH                  5
#define TDAL_F0_OFFSET                 8
#define TDAL_F0_WIDTH                  8
#define TDAL_F1_OFFSET                 16
#define TDAL_F1_WIDTH                  8
#define TDAL_F2_OFFSET                 24
#define TDAL_F2_WIDTH                  8
#define TDAL_WIDTH                     8
#define TDFI_CALVL_CAPTURE_F0_OFFSET   16
#define TDFI_CALVL_CAPTURE_F0_WIDTH    10
#define TDFI_CALVL_CAPTURE_F1_OFFSET   16
#define TDFI_CALVL_CAPTURE_F1_WIDTH    10
#define TDFI_CALVL_CAPTURE_F2_OFFSET   16
#define TDFI_CALVL_CAPTURE_F2_WIDTH    10
#define TDFI_CALVL_CAPTURE_WIDTH       10
#define TDFI_CALVL_CC_F0_OFFSET        0
#define TDFI_CALVL_CC_F0_WIDTH         10
#define TDFI_CALVL_CC_F1_OFFSET        0
#define TDFI_CALVL_CC_F1_WIDTH         10
#define TDFI_CALVL_CC_F2_OFFSET        0
#define TDFI_CALVL_CC_F2_WIDTH         10
#define TDFI_CALVL_CC_WIDTH            10
#define TDFI_CALVL_EN_OFFSET           16
#define TDFI_CALVL_EN_WIDTH            8
#define TDFI_CALVL_MAX_OFFSET          0
#define TDFI_CALVL_MAX_WIDTH           32
#define TDFI_CALVL_RESP_OFFSET         0
#define TDFI_CALVL_RESP_WIDTH          32
#define TDFI_CTRL_DELAY_F0_OFFSET      16
#define TDFI_CTRL_DELAY_F0_WIDTH       4
#define TDFI_CTRL_DELAY_F1_OFFSET      24
#define TDFI_CTRL_DELAY_F1_WIDTH       4
#define TDFI_CTRL_DELAY_F2_OFFSET      0
#define TDFI_CTRL_DELAY_F2_WIDTH       4
#define TDFI_CTRL_DELAY_WIDTH          4
#define TDFI_CTRLUPD_INTERVAL_F0_ADDR  474
#define TDFI_CTRLUPD_INTERVAL_F0_OFFSET 0
#define TDFI_CTRLUPD_INTERVAL_F0_WIDTH 32
#define TDFI_CTRLUPD_INTERVAL_F1_ADDR  482
#define TDFI_CTRLUPD_INTERVAL_F1_OFFSET 0
#define TDFI_CTRLUPD_INTERVAL_F1_WIDTH 32
#define TDFI_CTRLUPD_INTERVAL_F2_ADDR  490
#define TDFI_CTRLUPD_INTERVAL_F2_OFFSET 0
#define TDFI_CTRLUPD_INTERVAL_F2_WIDTH 32
#define TDFI_CTRLUPD_INTERVAL_WIDTH    32
#define TDFI_CTRLUPD_MAX_F0_OFFSET     0
#define TDFI_CTRLUPD_MAX_F0_WIDTH      21
#define TDFI_CTRLUPD_MAX_F1_OFFSET     0
#define TDFI_CTRLUPD_MAX_F1_WIDTH      21
#define TDFI_CTRLUPD_MAX_F2_OFFSET     0
#define TDFI_CTRLUPD_MAX_F2_WIDTH      21
#define TDFI_CTRLUPD_MAX_WIDTH         21
#define TDFI_CTRLUPD_MIN_OFFSET        16
#define TDFI_CTRLUPD_MIN_WIDTH         8
#define TDFI_DRAM_CLK_DISABLE_OFFSET   8
#define TDFI_DRAM_CLK_DISABLE_WIDTH    4
#define TDFI_DRAM_CLK_ENABLE_OFFSET    16
#define TDFI_DRAM_CLK_ENABLE_WIDTH     4
#define TDFI_INIT_COMPLETE_F0_OFFSET   0
#define TDFI_INIT_COMPLETE_F0_WIDTH    16
#define TDFI_INIT_COMPLETE_F1_OFFSET   0
#define TDFI_INIT_COMPLETE_F1_WIDTH    16
#define TDFI_INIT_COMPLETE_F2_OFFSET   0
#define TDFI_INIT_COMPLETE_F2_WIDTH    16
#define TDFI_INIT_COMPLETE_TIMEOUT_BIT 37
#define TDFI_INIT_COMPLETE_WIDTH       16
#define TDFI_INIT_START_F0_OFFSET      8
#define TDFI_INIT_START_F0_WIDTH       10
#define TDFI_INIT_START_F1_OFFSET      16
#define TDFI_INIT_START_F1_WIDTH       10
#define TDFI_INIT_START_F2_OFFSET      16
#define TDFI_INIT_START_F2_WIDTH       10
#define TDFI_INIT_START_WIDTH          10
#define TDFI_LP_RESP_OFFSET            0
#define TDFI_LP_RESP_WIDTH             3
#define TDFI_PARIN_LAT_MAX             8
#define TDFI_PARIN_LAT_OFFSET          0
#define TDFI_PARIN_LAT_WIDTH           3
#define TDFI_PHY_CRC_MAX_LAT_F0_OFFSET 16
#define TDFI_PHY_CRC_MAX_LAT_F0_WIDTH  6
#define TDFI_PHY_CRC_MAX_LAT_F1_OFFSET 24
#define TDFI_PHY_CRC_MAX_LAT_F1_WIDTH  6
#define TDFI_PHY_CRC_MAX_LAT_F2_OFFSET 0
#define TDFI_PHY_CRC_MAX_LAT_F2_WIDTH  6
#define TDFI_PHY_CRC_MAX_LAT_WIDTH     6
#define TDFI_PHY_RDLAT_F0_OFFSET       8
#define TDFI_PHY_RDLAT_F0_WIDTH        8
#define TDFI_PHY_RDLAT_F1_OFFSET       16
#define TDFI_PHY_RDLAT_F1_WIDTH        8
#define TDFI_PHY_RDLAT_F2_OFFSET       24
#define TDFI_PHY_RDLAT_F2_WIDTH        8
#define TDFI_PHY_RDLAT_WIDTH           8
#define TDFI_PHY_WRDATA_F0_OFFSET      24
#define TDFI_PHY_WRDATA_F0_WIDTH       3
#define TDFI_PHY_WRDATA_F1_OFFSET      0
#define TDFI_PHY_WRDATA_F1_WIDTH       3
#define TDFI_PHY_WRDATA_F2_OFFSET      8
#define TDFI_PHY_WRDATA_F2_WIDTH       3
#define TDFI_PHY_WRDATA_PIPE_DEPTH     4
#define TDFI_PHY_WRDATA_WIDTH          3
#define TDFI_PHY_WRLAT_OFFSET          24
#define TDFI_PHY_WRLAT_WIDTH           8
#define TDFI_PHYMSTR_MAX_F0_OFFSET     0
#define TDFI_PHYMSTR_MAX_F0_WIDTH      32
#define TDFI_PHYMSTR_MAX_F1_OFFSET     0
#define TDFI_PHYMSTR_MAX_F1_WIDTH      32
#define TDFI_PHYMSTR_MAX_F2_OFFSET     0
#define TDFI_PHYMSTR_MAX_F2_WIDTH      32
#define TDFI_PHYMSTR_MAX_TYPE0_F0_OFFSET 0
#define TDFI_PHYMSTR_MAX_TYPE0_F0_WIDTH 32
#define TDFI_PHYMSTR_MAX_TYPE0_F1_OFFSET 0
#define TDFI_PHYMSTR_MAX_TYPE0_F1_WIDTH 32
#define TDFI_PHYMSTR_MAX_TYPE0_F2_OFFSET 0
#define TDFI_PHYMSTR_MAX_TYPE0_F2_WIDTH 32
#define TDFI_PHYMSTR_MAX_TYPE0_WIDTH   32
#define TDFI_PHYMSTR_MAX_TYPE1_F0_OFFSET 0
#define TDFI_PHYMSTR_MAX_TYPE1_F0_WIDTH 32
#define TDFI_PHYMSTR_MAX_TYPE1_F1_OFFSET 0
#define TDFI_PHYMSTR_MAX_TYPE1_F1_WIDTH 32
#define TDFI_PHYMSTR_MAX_TYPE1_F2_OFFSET 0
#define TDFI_PHYMSTR_MAX_TYPE1_F2_WIDTH 32
#define TDFI_PHYMSTR_MAX_TYPE1_WIDTH   32
#define TDFI_PHYMSTR_MAX_TYPE2_F0_OFFSET 0
#define TDFI_PHYMSTR_MAX_TYPE2_F0_WIDTH 32
#define TDFI_PHYMSTR_MAX_TYPE2_F1_OFFSET 0
#define TDFI_PHYMSTR_MAX_TYPE2_F1_WIDTH 32
#define TDFI_PHYMSTR_MAX_TYPE2_F2_OFFSET 0
#define TDFI_PHYMSTR_MAX_TYPE2_F2_WIDTH 32
#define TDFI_PHYMSTR_MAX_TYPE2_WIDTH   32
#define TDFI_PHYMSTR_MAX_TYPE3_F0_OFFSET 0
#define TDFI_PHYMSTR_MAX_TYPE3_F0_WIDTH 32
#define TDFI_PHYMSTR_MAX_TYPE3_F1_OFFSET 0
#define TDFI_PHYMSTR_MAX_TYPE3_F1_WIDTH 32
#define TDFI_PHYMSTR_MAX_TYPE3_F2_OFFSET 0
#define TDFI_PHYMSTR_MAX_TYPE3_F2_WIDTH 32
#define TDFI_PHYMSTR_MAX_TYPE3_WIDTH   32
#define TDFI_PHYMSTR_MAX_WIDTH         32
#define TDFI_PHYMSTR_RESP_F0_OFFSET    0
#define TDFI_PHYMSTR_RESP_F0_WIDTH     20
#define TDFI_PHYMSTR_RESP_F1_OFFSET    0
#define TDFI_PHYMSTR_RESP_F1_WIDTH     20
#define TDFI_PHYMSTR_RESP_F2_OFFSET    0
#define TDFI_PHYMSTR_RESP_F2_WIDTH     20
#define TDFI_PHYMSTR_RESP_WIDTH        20
#define TDFI_PHYUPD_RESP_F0_OFFSET     0
#define TDFI_PHYUPD_RESP_F0_WIDTH      23
#define TDFI_PHYUPD_RESP_F1_OFFSET     0
#define TDFI_PHYUPD_RESP_F1_WIDTH      23
#define TDFI_PHYUPD_RESP_F2_OFFSET     0
#define TDFI_PHYUPD_RESP_F2_WIDTH      23
#define TDFI_PHYUPD_RESP_WIDTH         23
#define TDFI_PHYUPD_TYPE0_F0_ADDR      469
#define TDFI_PHYUPD_TYPE0_F0_OFFSET    0
#define TDFI_PHYUPD_TYPE0_F0_WIDTH     32
#define TDFI_PHYUPD_TYPE0_F1_ADDR      477
#define TDFI_PHYUPD_TYPE0_F1_OFFSET    0
#define TDFI_PHYUPD_TYPE0_F1_WIDTH     32
#define TDFI_PHYUPD_TYPE0_F2_ADDR      485
#define TDFI_PHYUPD_TYPE0_F2_OFFSET    0
#define TDFI_PHYUPD_TYPE0_F2_WIDTH     32
#define TDFI_PHYUPD_TYPE0_WIDTH        32
#define TDFI_PHYUPD_TYPE1_F0_ADDR      470
#define TDFI_PHYUPD_TYPE1_F0_OFFSET    0
#define TDFI_PHYUPD_TYPE1_F0_WIDTH     32
#define TDFI_PHYUPD_TYPE1_F1_ADDR      478
#define TDFI_PHYUPD_TYPE1_F1_OFFSET    0
#define TDFI_PHYUPD_TYPE1_F1_WIDTH     32
#define TDFI_PHYUPD_TYPE1_F2_ADDR      486
#define TDFI_PHYUPD_TYPE1_F2_OFFSET    0
#define TDFI_PHYUPD_TYPE1_F2_WIDTH     32
#define TDFI_PHYUPD_TYPE1_WIDTH        32
#define TDFI_PHYUPD_TYPE2_F0_ADDR      471
#define TDFI_PHYUPD_TYPE2_F0_OFFSET    0
#define TDFI_PHYUPD_TYPE2_F0_WIDTH     32
#define TDFI_PHYUPD_TYPE2_F1_ADDR      479
#define TDFI_PHYUPD_TYPE2_F1_OFFSET    0
#define TDFI_PHYUPD_TYPE2_F1_WIDTH     32
#define TDFI_PHYUPD_TYPE2_F2_ADDR      487
#define TDFI_PHYUPD_TYPE2_F2_OFFSET    0
#define TDFI_PHYUPD_TYPE2_F2_WIDTH     32
#define TDFI_PHYUPD_TYPE2_WIDTH        32
#define TDFI_PHYUPD_TYPE3_F0_ADDR      472
#define TDFI_PHYUPD_TYPE3_F0_OFFSET    0
#define TDFI_PHYUPD_TYPE3_F0_WIDTH     32
#define TDFI_PHYUPD_TYPE3_F1_ADDR      480
#define TDFI_PHYUPD_TYPE3_F1_OFFSET    0
#define TDFI_PHYUPD_TYPE3_F1_WIDTH     32
#define TDFI_PHYUPD_TYPE3_F2_ADDR      488
#define TDFI_PHYUPD_TYPE3_F2_OFFSET    0
#define TDFI_PHYUPD_TYPE3_F2_WIDTH     32
#define TDFI_PHYUPD_TYPE3_WIDTH        32
#define TDFI_RDCSLAT_F0_OFFSET         16
#define TDFI_RDCSLAT_F0_WIDTH          8
#define TDFI_RDCSLAT_F1_OFFSET         0
#define TDFI_RDCSLAT_F1_WIDTH          8
#define TDFI_RDCSLAT_F2_OFFSET         16
#define TDFI_RDCSLAT_F2_WIDTH          8
#define TDFI_RDCSLAT_WIDTH             8
#define TDFI_RDDATA_EN_OFFSET          0
#define TDFI_RDDATA_EN_WIDTH           8
#define TDFI_RDLVL_EN_OFFSET           0
#define TDFI_RDLVL_EN_WIDTH            8
#define TDFI_RDLVL_MAX_OFFSET          0
#define TDFI_RDLVL_MAX_WIDTH           32
#define TDFI_RDLVL_RESP_OFFSET         0
#define TDFI_RDLVL_RESP_WIDTH          32
#define TDFI_RDLVL_RR_OFFSET           8
#define TDFI_RDLVL_RR_WIDTH            10
#define TDFI_WRCSLAT_F0_OFFSET         24
#define TDFI_WRCSLAT_F0_WIDTH          8
#define TDFI_WRCSLAT_F1_OFFSET         8
#define TDFI_WRCSLAT_F1_WIDTH          8
#define TDFI_WRCSLAT_F2_OFFSET         24
#define TDFI_WRCSLAT_F2_WIDTH          8
#define TDFI_WRCSLAT_WIDTH             8
#define TDFI_WRDATA_DELAY_OFFSET       8
#define TDFI_WRDATA_DELAY_WIDTH        8
#define TDFI_WRLVL_EN_OFFSET           24
#define TDFI_WRLVL_EN_WIDTH            8
#define TDFI_WRLVL_MAX_OFFSET          0
#define TDFI_WRLVL_MAX_WIDTH           32
#define TDFI_WRLVL_RESP_OFFSET         0
#define TDFI_WRLVL_RESP_WIDTH          32
#define TDFI_WRLVL_WW_OFFSET           0
#define TDFI_WRLVL_WW_WIDTH            10
#define TDLL_F0_OFFSET                 0
#define TDLL_F0_WIDTH                  16
#define TDLL_F1_OFFSET                 16
#define TDLL_F1_WIDTH                  16
#define TDLL_F2_OFFSET                 0
#define TDLL_F2_WIDTH                  16
#define TDLL_WIDTH                     16
#define TDQSCK_MAX_F0_OFFSET           8
#define TDQSCK_MAX_F0_WIDTH            4
#define TDQSCK_MAX_F1_OFFSET           24
#define TDQSCK_MAX_F1_WIDTH            4
#define TDQSCK_MAX_F2_OFFSET           8
#define TDQSCK_MAX_F2_WIDTH            4
#define TDQSCK_MAX_WIDTH               4
#define TDQSCK_MIN_F0_OFFSET           16
#define TDQSCK_MIN_F0_WIDTH            3
#define TDQSCK_MIN_F1_OFFSET           0
#define TDQSCK_MIN_F1_WIDTH            3
#define TDQSCK_MIN_F2_OFFSET           16
#define TDQSCK_MIN_F2_WIDTH            3
#define TDQSCK_MIN_WIDTH               3
#define TEMP_ALERT_BIT                 27
#define TEMP_CHG_ALERT_BIT             26
#define TESCKE_F0_OFFSET               0
#define TESCKE_F0_WIDTH                3
#define TESCKE_F1_OFFSET               0
#define TESCKE_F1_WIDTH                3
#define TESCKE_F2_OFFSET               0
#define TESCKE_F2_WIDTH                3
#define TESCKE_WIDTH                   3
#define TFAW_F0_OFFSET                 0
#define TFAW_F0_WIDTH                  9
#define TFAW_F1_OFFSET                 0
#define TFAW_F1_WIDTH                  9
#define TFAW_F2_OFFSET                 0
#define TFAW_F2_WIDTH                  9
#define TFAW_WIDTH                     9
#define TFC_F0_OFFSET                  16
#define TFC_F0_WIDTH                   10
#define TFC_F1_OFFSET                  0
#define TFC_F1_WIDTH                   10
#define TFC_F2_OFFSET                  16
#define TFC_F2_WIDTH                   10
#define TFC_WIDTH                      10
#define TICK_COUNT_WIDTH               3
#define TINIT3_F0_OFFSET               0
#define TINIT3_F0_WIDTH                24
#define TINIT3_F1_OFFSET               0
#define TINIT3_F1_WIDTH                24
#define TINIT3_F2_OFFSET               0
#define TINIT3_F2_WIDTH                24
#define TINIT3_WIDTH                   24
#define TINIT4_F0_OFFSET               0
#define TINIT4_F0_WIDTH                24
#define TINIT4_F1_OFFSET               0
#define TINIT4_F1_WIDTH                24
#define TINIT4_F2_OFFSET               0
#define TINIT4_F2_WIDTH                24
#define TINIT4_WIDTH                   24
#define TINIT5_F0_OFFSET               0
#define TINIT5_F0_WIDTH                24
#define TINIT5_F1_OFFSET               0
#define TINIT5_F1_WIDTH                24
#define TINIT5_F2_OFFSET               0
#define TINIT5_F2_WIDTH                24
#define TINIT5_WIDTH                   24
#define TINIT_F0_OFFSET                0
#define TINIT_F0_WIDTH                 24
#define TINIT_F1_OFFSET                0
#define TINIT_F1_WIDTH                 24
#define TINIT_F2_OFFSET                0
#define TINIT_F2_WIDTH                 24
#define TINIT_WIDTH                    24
#define TMOD_F0_OFFSET                 16
#define TMOD_F0_WIDTH                  8
#define TMOD_F1_OFFSET                 8
#define TMOD_F1_WIDTH                  8
#define TMOD_F2_OFFSET                 8
#define TMOD_F2_WIDTH                  8
#define TMOD_OPT_THRESHOLD_OFFSET      24
#define TMOD_OPT_THRESHOLD_WIDTH       3
#define TMOD_PAR_F0_OFFSET             0
#define TMOD_PAR_F0_WIDTH              8
#define TMOD_PAR_F1_OFFSET             0
#define TMOD_PAR_F1_WIDTH              8
#define TMOD_PAR_F2_OFFSET             0
#define TMOD_PAR_F2_WIDTH              8
#define TMOD_PAR_MAX_PL_F0_OFFSET      16
#define TMOD_PAR_MAX_PL_F0_WIDTH       8
#define TMOD_PAR_MAX_PL_F1_OFFSET      16
#define TMOD_PAR_MAX_PL_F1_WIDTH       8
#define TMOD_PAR_MAX_PL_F2_OFFSET      16
#define TMOD_PAR_MAX_PL_F2_WIDTH       8
#define TMOD_PAR_WIDTH                 8
#define TMOD_WIDTH                     8
#define TMP_2X4_TICK_MINUS_ADJ_OFFSET  8
#define TMP_2X4_TICK_MINUS_ADJ_WIDTH   4
#define TMP_2X4_TICK_PLUS_ADJ_OFFSET   0
#define TMP_2X4_TICK_PLUS_ADJ_WIDTH    4
#define TMP_4X2_TICK_MINUS_ADJ_OFFSET  8
#define TMP_4X2_TICK_MINUS_ADJ_WIDTH   4
#define TMP_4X2_TICK_PLUS_ADJ_OFFSET   0
#define TMP_4X2_TICK_PLUS_ADJ_WIDTH    4
#define TMP_NXN_TICK_MINUS_ADJ_OFFSET  24
#define TMP_NXN_TICK_MINUS_ADJ_WIDTH   4
#define TMP_NXN_TICK_PLUS_ADJ_OFFSET   16
#define TMP_NXN_TICK_PLUS_ADJ_WIDTH    4
#define TMPRR_OFFSET                   0
#define TMPRR_WIDTH                    4
#define TMRD_F0_OFFSET                 8
#define TMRD_F0_WIDTH                  8
#define TMRD_F1_OFFSET                 0
#define TMRD_F1_WIDTH                  8
#define TMRD_F2_OFFSET                 0
#define TMRD_F2_WIDTH                  8
#define TMRD_OPT_THRESHOLD_OFFSET      0
#define TMRD_OPT_THRESHOLD_WIDTH       3
#define TMRD_PAR_F0_OFFSET             8
#define TMRD_PAR_F0_WIDTH              8
#define TMRD_PAR_F1_OFFSET             8
#define TMRD_PAR_F1_WIDTH              8
#define TMRD_PAR_F2_OFFSET             8
#define TMRD_PAR_F2_WIDTH              8
#define TMRD_PAR_MAX_PL_F0_OFFSET      24
#define TMRD_PAR_MAX_PL_F0_WIDTH       8
#define TMRD_PAR_MAX_PL_F1_OFFSET      24
#define TMRD_PAR_MAX_PL_F1_WIDTH       8
#define TMRD_PAR_MAX_PL_F2_OFFSET      24
#define TMRD_PAR_MAX_PL_F2_WIDTH       8
#define TMRD_PAR_MAX_PL_WIDTH          8
#define TMRD_PAR_WIDTH                 8
#define TMRD_PDA_OFFSET                24
#define TMRD_PDA_WIDTH                 8
#define TMRD_WIDTH                     8
#define TMRR_OFFSET                    0
#define TMRR_WIDTH                     4
#define TMRRI_F0_OFFSET                16
#define TMRRI_F0_WIDTH                 8
#define TMRRI_F1_OFFSET                24
#define TMRRI_F1_WIDTH                 8
#define TMRRI_F2_OFFSET                0
#define TMRRI_F2_WIDTH                 8
#define TMRRI_WIDTH                    8
#define TMRWCKEL_F0_OFFSET             0
#define TMRWCKEL_F0_WIDTH              5
#define TMRWCKEL_F1_OFFSET             16
#define TMRWCKEL_F1_WIDTH              5
#define TMRWCKEL_F2_OFFSET             0
#define TMRWCKEL_F2_WIDTH              5
#define TMRWCKEL_WIDTH                 5
#define TMRZ_F0_OFFSET                 24
#define TMRZ_F0_WIDTH                  5
#define TMRZ_F1_OFFSET                 0
#define TMRZ_F1_WIDTH                  5
#define TMRZ_F2_OFFSET                 8
#define TMRZ_F2_WIDTH                  5
#define TMRZ_WIDTH                     5
#define TODTH_RD_F0_OFFSET             24
#define TODTH_RD_F0_WIDTH              4
#define TODTH_RD_F1_OFFSET             16
#define TODTH_RD_F1_WIDTH              4
#define TODTH_RD_F2_OFFSET             8
#define TODTH_RD_F2_WIDTH              4
#define TODTH_RD_WIDTH                 4
#define TODTH_WR_F0_OFFSET             16
#define TODTH_WR_F0_WIDTH              4
#define TODTH_WR_F1_OFFSET             8
#define TODTH_WR_F1_WIDTH              4
#define TODTH_WR_F2_OFFSET             0
#define TODTH_WR_F2_WIDTH              4
#define TODTH_WR_WIDTH                 4
#define TODTL_2CMD_F0_OFFSET           8
#define TODTL_2CMD_F0_WIDTH            8
#define TODTL_2CMD_F1_OFFSET           0
#define TODTL_2CMD_F1_WIDTH            8
#define TODTL_2CMD_F2_OFFSET           24
#define TODTL_2CMD_F2_WIDTH            8
#define TODTL_2CMD_WIDTH               8
#define TOSCO_F0_OFFSET                8
#define TOSCO_F0_WIDTH                 8
#define TOSCO_F1_OFFSET                16
#define TOSCO_F1_WIDTH                 8
#define TOSCO_F2_OFFSET                24
#define TOSCO_F2_WIDTH                 8
#define TOSCO_WIDTH                    8
#define TOTAL_BANK                     16
#define TOTAL_MEMBST_WORDS_P           273
#define TPDEX_F0_OFFSET                16
#define TPDEX_F0_WIDTH                 16
#define TPDEX_F1_OFFSET                0
#define TPDEX_F1_WIDTH                 16
#define TPDEX_F2_OFFSET                16
#define TPDEX_F2_WIDTH                 16
#define TPDEX_WIDTH                    16
#define TPPD_OFFSET                    16
#define TPPD_WIDTH                     3
#define TRANS_COUNT_DLY_WIDTH          10
#define TRANS_ID_WIDTH                 16
#define TRAS_LOCKOUT_OFFSET            0
#define TRAS_LOCKOUT_WIDTH             1
#define TRAS_MAX_F0_OFFSET             0
#define TRAS_MAX_F0_WIDTH              20
#define TRAS_MAX_F1_OFFSET             0
#define TRAS_MAX_F1_WIDTH              20
#define TRAS_MAX_F2_OFFSET             0
#define TRAS_MAX_F2_WIDTH              20
#define TRAS_MAX_WIDTH                 20
#define TRAS_MIN_F0_OFFSET             24
#define TRAS_MIN_F0_WIDTH              8
#define TRAS_MIN_F1_OFFSET             24
#define TRAS_MIN_F1_WIDTH              8
#define TRAS_MIN_F2_OFFSET             24
#define TRAS_MIN_F2_WIDTH              8
#define TRAS_MIN_WIDTH                 8
#define TRAS_OPT_THRESHOLD_OFFSET      16
#define TRAS_OPT_THRESHOLD_WIDTH       3
#define TRAS_TICK_MINUS_ADJ_OFFSET     24
#define TRAS_TICK_MINUS_ADJ_WIDTH      4
#define TRAS_TICK_PLUS_ADJ_OFFSET      16
#define TRAS_TICK_PLUS_ADJ_WIDTH       4
#define TRC_F0_OFFSET                  8
#define TRC_F0_WIDTH                   9
#define TRC_F1_OFFSET                  8
#define TRC_F1_WIDTH                   9
#define TRC_F2_OFFSET                  8
#define TRC_F2_WIDTH                   9
#define TRC_WIDTH                      9
#define TRCD_F0_OFFSET                 16
#define TRCD_F0_WIDTH                  8
#define TRCD_F1_OFFSET                 0
#define TRCD_F1_WIDTH                  8
#define TRCD_F2_OFFSET                 16
#define TRCD_F2_WIDTH                  8
#define TRCD_WIDTH                     8
#define TREF_ENABLE_ADDR               77
#define TREF_ENABLE_OFFSET             24
#define TREF_ENABLE_WIDTH              1
#define TREF_F0_OFFSET                 0
#define TREF_F0_WIDTH                  20
#define TREF_F1_OFFSET                 0
#define TREF_F1_WIDTH                  20
#define TREF_F2_OFFSET                 0
#define TREF_F2_WIDTH                  20
#define TREF_INTERVAL_OFFSET           0
#define TREF_INTERVAL_WIDTH            20
#define TREF_WIDTH                     20
#define TREFI_PB_F0_OFFSET             0
#define TREFI_PB_F0_WIDTH              20
#define TREFI_PB_F1_OFFSET             0
#define TREFI_PB_F1_WIDTH              20
#define TREFI_PB_F2_OFFSET             0
#define TREFI_PB_F2_WIDTH              20
#define TREFI_PB_WIDTH                 20
#define TRFC_F0_OFFSET                 16
#define TRFC_F0_WIDTH                  10
#define TRFC_F1_OFFSET                 0
#define TRFC_F1_WIDTH                  10
#define TRFC_F2_OFFSET                 0
#define TRFC_F2_WIDTH                  10
#define TRFC_OPT_THRESHOLD_OFFSET      0
#define TRFC_OPT_THRESHOLD_WIDTH       3
#define TRFC_PB_F0_OFFSET              16
#define TRFC_PB_F0_WIDTH               10
#define TRFC_PB_F1_OFFSET              0
#define TRFC_PB_F1_WIDTH               10
#define TRFC_PB_F2_OFFSET              0
#define TRFC_PB_F2_WIDTH               10
#define TRFC_PB_WIDTH                  10
#define TRFC_TICK_MINUS_ADJ_OFFSET     24
#define TRFC_TICK_MINUS_ADJ_WIDTH      4
#define TRFC_TICK_PLUS_ADJ_OFFSET      16
#define TRFC_TICK_PLUS_ADJ_WIDTH       4
#define TRFC_WIDTH                     10
#define TRP_AB_F0_OFFSET               8
#define TRP_AB_F0_WIDTH                8
#define TRP_AB_F1_OFFSET               16
#define TRP_AB_F1_WIDTH                8
#define TRP_AB_F2_OFFSET               24
#define TRP_AB_F2_WIDTH                8
#define TRP_AB_WIDTH                   8
#define TRP_F0_OFFSET                  16
#define TRP_F0_WIDTH                   8
#define TRP_F1_OFFSET                  16
#define TRP_F1_WIDTH                   8
#define TRP_F2_OFFSET                  16
#define TRP_F2_WIDTH                   8
#define TRP_OPT_THRESHOLD_OFFSET       8
#define TRP_OPT_THRESHOLD_WIDTH        3
#define TRP_TICK_MINUS_ADJ_OFFSET      8
#define TRP_TICK_MINUS_ADJ_WIDTH       4
#define TRP_TICK_PLUS_ADJ_OFFSET       0
#define TRP_TICK_PLUS_ADJ_WIDTH        4
#define TRP_WIDTH                      8
#define TRRD_F0_OFFSET                 24
#define TRRD_F0_WIDTH                  8
#define TRRD_F1_OFFSET                 24
#define TRRD_F1_WIDTH                  8
#define TRRD_F2_OFFSET                 24
#define TRRD_F2_WIDTH                  8
#define TRRD_L_F0_OFFSET               0
#define TRRD_L_F0_WIDTH                8
#define TRRD_L_F1_OFFSET               0
#define TRRD_L_F1_WIDTH                8
#define TRRD_L_F2_OFFSET               0
#define TRRD_L_F2_WIDTH                8
#define TRRD_L_WIDTH                   8
#define TRRD_WIDTH                     8
#define TRST_PWRON_OFFSET              0
#define TRST_PWRON_WIDTH               32
#define TRTP_AP_F0_OFFSET              0
#define TRTP_AP_F0_WIDTH               8
#define TRTP_AP_F1_OFFSET              24
#define TRTP_AP_F1_WIDTH               8
#define TRTP_AP_F2_OFFSET              24
#define TRTP_AP_F2_WIDTH               8
#define TRTP_F0_OFFSET                 24
#define TRTP_F0_WIDTH                  8
#define TRTP_F1_OFFSET                 16
#define TRTP_F1_WIDTH                  8
#define TRTP_F2_OFFSET                 16
#define TRTP_F2_WIDTH                  8
#define TRTP_MAX_LAT_WIDTH             9
#define TRTP_WIDTH                     8
#define TSCALE                         100
#define TSR_F0_OFFSET                  24
#define TSR_F0_WIDTH                   8
#define TSR_F1_OFFSET                  24
#define TSR_F1_WIDTH                   8
#define TSR_F2_OFFSET                  24
#define TSR_F2_WIDTH                   8
#define TSR_WIDTH                      8
#define TSREF2PHYMSTR_OFFSET           8
#define TSREF2PHYMSTR_WIDTH            6
#define TVRCG_DISABLE_F0_OFFSET        0
#define TVRCG_DISABLE_F0_WIDTH         10
#define TVRCG_DISABLE_F1_OFFSET        16
#define TVRCG_DISABLE_F1_WIDTH         10
#define TVRCG_DISABLE_F2_OFFSET        0
#define TVRCG_DISABLE_F2_WIDTH         10
#define TVRCG_DISABLE_WIDTH            10
#define TVRCG_ENABLE_F0_OFFSET         16
#define TVRCG_ENABLE_F0_WIDTH          10
#define TVRCG_ENABLE_F1_OFFSET         0
#define TVRCG_ENABLE_F1_WIDTH          10
#define TVRCG_ENABLE_F2_OFFSET         16
#define TVRCG_ENABLE_F2_WIDTH          10
#define TVRCG_ENABLE_WIDTH             10
#define TVRCG_WIDTH                    10
#define TVREF_LONG_F0_OFFSET           16
#define TVREF_LONG_F0_WIDTH            16
#define TVREF_LONG_F1_OFFSET           0
#define TVREF_LONG_F1_WIDTH            16
#define TVREF_LONG_F2_OFFSET           16
#define TVREF_LONG_F2_WIDTH            16
#define TVREF_LONG_WIDTH               16
#define TVREF_OFFSET                   8
#define TVREF_WIDTH                    16
#define TWO_PASS_GATE_OFFSET           24
#define TWO_PASS_GATE_WIDTH            1
#define TWR_AP_MAX_LAT_WIDTH           9
#define TWR_F0_OFFSET                  24
#define TWR_F0_WIDTH                   8
#define TWR_F1_OFFSET                  8
#define TWR_F1_WIDTH                   8
#define TWR_F2_OFFSET                  24
#define TWR_F2_WIDTH                   8
#define TWR_MAX_LAT_WIDTH              9
#define TWR_MPR_F0_OFFSET              8
#define TWR_MPR_F0_WIDTH               8
#define TWR_MPR_F1_OFFSET              8
#define TWR_MPR_F1_WIDTH               8
#define TWR_MPR_F2_OFFSET              8
#define TWR_MPR_F2_WIDTH               8
#define TWR_MPR_WIDTH                  8
#define TWR_TICK_MINUS_ADJ_OFFSET      24
#define TWR_TICK_MINUS_ADJ_WIDTH       4
#define TWR_TICK_PLUS_ADJ_OFFSET       16
#define TWR_TICK_PLUS_ADJ_WIDTH        4
#define TWR_WIDTH                      8
#define TWTR_F0_OFFSET                 0
#define TWTR_F0_WIDTH                  6
#define TWTR_F1_OFFSET                 0
#define TWTR_F1_WIDTH                  6
#define TWTR_F2_OFFSET                 0
#define TWTR_F2_WIDTH                  6
#define TWTR_L_F0_OFFSET               8
#define TWTR_L_F0_WIDTH                6
#define TWTR_L_F1_OFFSET               8
#define TWTR_L_F1_WIDTH                6
#define TWTR_L_F2_OFFSET               8
#define TWTR_L_F2_WIDTH                6
#define TWTR_L_WIDTH                   6
#define TWTR_WIDTH                     6
#define TXPDLL_F0_OFFSET               0
#define TXPDLL_F0_WIDTH                16
#define TXPDLL_F1_OFFSET               16
#define TXPDLL_F1_WIDTH                16
#define TXPDLL_F2_OFFSET               0
#define TXPDLL_F2_WIDTH                16
#define TXPDLL_WIDTH                   16
#define TXSNR_F0_OFFSET                16
#define TXSNR_F0_WIDTH                 16
#define TXSNR_F1_OFFSET                16
#define TXSNR_F1_WIDTH                 16
#define TXSNR_F2_OFFSET                16
#define TXSNR_F2_WIDTH                 16
#define TXSNR_WIDTH                    16
#define TXSR_F0_OFFSET                 0
#define TXSR_F0_WIDTH                  16
#define TXSR_F1_OFFSET                 0
#define TXSR_F1_WIDTH                  16
#define TXSR_F2_OFFSET                 0
#define TXSR_F2_WIDTH                  16
#define TXSR_WIDTH                     16
#define TYPE_WIDTH                     7
#define TZQCAL_F0_OFFSET               16
#define TZQCAL_F0_WIDTH                12
#define TZQCAL_F1_OFFSET               0
#define TZQCAL_F1_WIDTH                12
#define TZQCAL_F2_OFFSET               16
#define TZQCAL_F2_WIDTH                12
#define TZQCAL_WIDTH                   12
#define TZQCKE_F0_OFFSET               8
#define TZQCKE_F0_WIDTH                4
#define TZQCKE_F1_OFFSET               24
#define TZQCKE_F1_WIDTH                4
#define TZQCKE_F2_OFFSET               8
#define TZQCKE_F2_WIDTH                4
#define TZQCKE_WIDTH                   4
#define TZQLAT_F0_OFFSET               0
#define TZQLAT_F0_WIDTH                7
#define TZQLAT_F1_OFFSET               16
#define TZQLAT_F1_WIDTH                7
#define TZQLAT_F2_OFFSET               0
#define TZQLAT_F2_WIDTH                7
#define TZQLAT_WIDTH                   7
#define UPD_CTRLUPD_HIGH_THRESHOLD_F0_ADDR 117
#define UPD_CTRLUPD_HIGH_THRESHOLD_F0_OFFSET 0
#define UPD_CTRLUPD_HIGH_THRESHOLD_F0_WIDTH 16
#define UPD_CTRLUPD_HIGH_THRESHOLD_F1_ADDR 119
#define UPD_CTRLUPD_HIGH_THRESHOLD_F1_OFFSET 16
#define UPD_CTRLUPD_HIGH_THRESHOLD_F1_WIDTH 16
#define UPD_CTRLUPD_HIGH_THRESHOLD_F2_ADDR 122
#define UPD_CTRLUPD_HIGH_THRESHOLD_F2_OFFSET 0
#define UPD_CTRLUPD_HIGH_THRESHOLD_F2_WIDTH 16
#define UPD_CTRLUPD_HIGH_THRESHOLD_WIDTH 16
#define UPD_CTRLUPD_NORM_THRESHOLD_F0_ADDR 116
#define UPD_CTRLUPD_NORM_THRESHOLD_F0_OFFSET 16
#define UPD_CTRLUPD_NORM_THRESHOLD_F0_WIDTH 16
#define UPD_CTRLUPD_NORM_THRESHOLD_F1_ADDR 119
#define UPD_CTRLUPD_NORM_THRESHOLD_F1_OFFSET 0
#define UPD_CTRLUPD_NORM_THRESHOLD_F1_WIDTH 16
#define UPD_CTRLUPD_NORM_THRESHOLD_F2_ADDR 121
#define UPD_CTRLUPD_NORM_THRESHOLD_F2_OFFSET 16
#define UPD_CTRLUPD_NORM_THRESHOLD_F2_WIDTH 16
#define UPD_CTRLUPD_NORM_THRESHOLD_WIDTH 16
#define UPD_CTRLUPD_SW_PROMOTE_THRESHOLD_F0_OFFSET 0
#define UPD_CTRLUPD_SW_PROMOTE_THRESHOLD_F0_WIDTH 16
#define UPD_CTRLUPD_SW_PROMOTE_THRESHOLD_F1_OFFSET 16
#define UPD_CTRLUPD_SW_PROMOTE_THRESHOLD_F1_WIDTH 16
#define UPD_CTRLUPD_SW_PROMOTE_THRESHOLD_F2_OFFSET 0
#define UPD_CTRLUPD_SW_PROMOTE_THRESHOLD_F2_WIDTH 16
#define UPD_CTRLUPD_SW_PROMOTE_THRESHOLD_WIDTH 16
#define UPD_CTRLUPD_TIMEOUT_F0_OFFSET  16
#define UPD_CTRLUPD_TIMEOUT_F0_WIDTH   16
#define UPD_CTRLUPD_TIMEOUT_F1_OFFSET  0
#define UPD_CTRLUPD_TIMEOUT_F1_WIDTH   16
#define UPD_CTRLUPD_TIMEOUT_F2_OFFSET  16
#define UPD_CTRLUPD_TIMEOUT_F2_WIDTH   16
#define UPD_CTRLUPD_TIMEOUT_WIDTH      16
#define UPD_PHYUPD_DFI_PROMOTE_THRESHOLD_F0_OFFSET 16
#define UPD_PHYUPD_DFI_PROMOTE_THRESHOLD_F0_WIDTH 16
#define UPD_PHYUPD_DFI_PROMOTE_THRESHOLD_F1_OFFSET 0
#define UPD_PHYUPD_DFI_PROMOTE_THRESHOLD_F1_WIDTH 16
#define UPD_PHYUPD_DFI_PROMOTE_THRESHOLD_F2_OFFSET 16
#define UPD_PHYUPD_DFI_PROMOTE_THRESHOLD_F2_WIDTH 16
#define UPD_PHYUPD_DFI_PROMOTE_THRESHOLD_WIDTH 16
#define UPDATE_ERROR_BIT               13
#define UPDATE_ERROR_STATUS_OFFSET     0
#define UPDATE_ERROR_STATUS_WIDTH      8
#define USER_BYTE_WIDTH                4
#define USER_BYTE_WIDTH_M1             3
#define USER_DATA_WIDTH                128
#define USER_MASK_WIDTH                16
#define USER_RESYNC_DLL_BIT            36
#define USER_WORDS_PER_BURST_ERROR_BIT 8
#define VERSION_OFFSET                 16
#define VERSION_WIDTH                  16
#define VREF_CS_OFFSET                 16
#define VREF_CS_WIDTH                  1
#define VREF_DFS_EN_OFFSET             8
#define VREF_DFS_EN_WIDTH              1
#define VREF_EN_OFFSET                 24
#define VREF_EN_WIDTH                  1
#define VREF_PDA_EN_OFFSET             0
#define VREF_PDA_EN_WIDTH              1
#define VREF_SW_PROMOTE_THRESHOLD_F0_OFFSET 16
#define VREF_SW_PROMOTE_THRESHOLD_F0_WIDTH 16
#define VREF_SW_PROMOTE_THRESHOLD_F1_OFFSET 0
#define VREF_SW_PROMOTE_THRESHOLD_F1_WIDTH 16
#define VREF_SW_PROMOTE_THRESHOLD_F2_OFFSET 16
#define VREF_SW_PROMOTE_THRESHOLD_F2_WIDTH 16
#define VREF_SW_PROMOTE_THRESHOLD_WIDTH 16
#define VREF_VAL_DEV0_0_F0_OFFSET      16
#define VREF_VAL_DEV0_0_F0_WIDTH       7
#define VREF_VAL_DEV0_0_F1_OFFSET      16
#define VREF_VAL_DEV0_0_F1_WIDTH       7
#define VREF_VAL_DEV0_0_F2_OFFSET      16
#define VREF_VAL_DEV0_0_F2_WIDTH       7
#define VREF_VAL_DEV0_1_F0_OFFSET      24
#define VREF_VAL_DEV0_1_F0_WIDTH       7
#define VREF_VAL_DEV0_1_F1_OFFSET      24
#define VREF_VAL_DEV0_1_F1_WIDTH       7
#define VREF_VAL_DEV0_1_F2_OFFSET      24
#define VREF_VAL_DEV0_1_F2_WIDTH       7
#define VREF_VAL_DEV1_0_F0_OFFSET      0
#define VREF_VAL_DEV1_0_F0_WIDTH       7
#define VREF_VAL_DEV1_0_F1_OFFSET      0
#define VREF_VAL_DEV1_0_F1_WIDTH       7
#define VREF_VAL_DEV1_0_F2_OFFSET      0
#define VREF_VAL_DEV1_0_F2_WIDTH       7
#define VREF_VAL_DEV1_1_F0_OFFSET      8
#define VREF_VAL_DEV1_1_F0_WIDTH       7
#define VREF_VAL_DEV1_1_F1_OFFSET      8
#define VREF_VAL_DEV1_1_F1_WIDTH       7
#define VREF_VAL_DEV1_1_F2_OFFSET      8
#define VREF_VAL_DEV1_1_F2_WIDTH       7
#define VREF_VAL_DEV2_0_F0_OFFSET      16
#define VREF_VAL_DEV2_0_F0_WIDTH       7
#define VREF_VAL_DEV2_0_F1_OFFSET      16
#define VREF_VAL_DEV2_0_F1_WIDTH       7
#define VREF_VAL_DEV2_0_F2_OFFSET      16
#define VREF_VAL_DEV2_0_F2_WIDTH       7
#define VREF_VAL_DEV2_1_F0_OFFSET      24
#define VREF_VAL_DEV2_1_F0_WIDTH       7
#define VREF_VAL_DEV2_1_F1_OFFSET      24
#define VREF_VAL_DEV2_1_F1_WIDTH       7
#define VREF_VAL_DEV2_1_F2_OFFSET      24
#define VREF_VAL_DEV2_1_F2_WIDTH       7
#define VREF_VAL_DEV3_0_F0_OFFSET      0
#define VREF_VAL_DEV3_0_F0_WIDTH       7
#define VREF_VAL_DEV3_0_F1_OFFSET      0
#define VREF_VAL_DEV3_0_F1_WIDTH       7
#define VREF_VAL_DEV3_0_F2_OFFSET      0
#define VREF_VAL_DEV3_0_F2_WIDTH       7
#define VREF_VAL_DEV3_1_F0_OFFSET      8
#define VREF_VAL_DEV3_1_F0_WIDTH       7
#define VREF_VAL_DEV3_1_F1_OFFSET      8
#define VREF_VAL_DEV3_1_F1_WIDTH       7
#define VREF_VAL_DEV3_1_F2_OFFSET      8
#define VREF_VAL_DEV3_1_F2_WIDTH       7
#define VREF_VAL_WIDTH                 7
#define W2R_DIFFCS_DLY_F0_OFFSET       8
#define W2R_DIFFCS_DLY_F0_WIDTH        5
#define W2R_DIFFCS_DLY_F1_OFFSET       8
#define W2R_DIFFCS_DLY_F1_WIDTH        5
#define W2R_DIFFCS_DLY_F2_OFFSET       8
#define W2R_DIFFCS_DLY_F2_WIDTH        5
#define W2R_DIFFCS_DLY_WIDTH           5
#define W2R_SAMECS_DLY_OFFSET          24
#define W2R_SAMECS_DLY_WIDTH           5
#define W2R_SPLIT_EN_OFFSET            0
#define W2R_SPLIT_EN_WIDTH             1
#define W2W_DIFFCS_DLY_F0_OFFSET       16
#define W2W_DIFFCS_DLY_F0_WIDTH        5
#define W2W_DIFFCS_DLY_F1_OFFSET       16
#define W2W_DIFFCS_DLY_F1_WIDTH        5
#define W2W_DIFFCS_DLY_F2_OFFSET       16
#define W2W_DIFFCS_DLY_F2_WIDTH        5
#define W2W_DIFFCS_DLY_WIDTH           5
#define W2W_SAMECS_DLY_OFFSET          0
#define W2W_SAMECS_DLY_WIDTH           5
#define WE_PARITY_ERROR_INJECT_BIT     20
#define WFLUSH_BIT                     4
#define WL_TICK_MINUS_ADJ_OFFSET       24
#define WL_TICK_MINUS_ADJ_WIDTH        4
#define WL_TICK_PLUS_ADJ_OFFSET        16
#define WL_TICK_PLUS_ADJ_WIDTH         4
#define WLDQSEN_OFFSET                 24
#define WLDQSEN_WIDTH                  6
#define WLMRD_OFFSET                   0
#define WLMRD_WIDTH                    6
#define WR_DBI_EN_OFFSET               16
#define WR_DBI_EN_WIDTH                1
#define WR_ORDER_REQ_OFFSET            8
#define WR_ORDER_REQ_WIDTH             2
#define WR_TAG_WIDTH                   6
#define WR_TO_ODTH_F0_OFFSET           16
#define WR_TO_ODTH_F0_WIDTH            6
#define WR_TO_ODTH_F1_OFFSET           24
#define WR_TO_ODTH_F1_WIDTH            6
#define WR_TO_ODTH_F2_OFFSET           0
#define WR_TO_ODTH_F2_WIDTH            6
#define WR_TO_ODTH_WIDTH               6
#define WRAP_BIT                       2
#define WRAP_SPLIT_ERROR_BIT           7
#define WRCMD_LAT_MAX                  64
#define WRCMD_LAT_WIDTH                6
#define WRCSLAT_MAX                    255
#define WRITE_DATA_FIFO_DEPTH_OFFSET   8
#define WRITE_DATA_FIFO_DEPTH_WIDTH    8
#define WRITE_DATA_FIFO_PTR_WIDTH_OFFSET 16
#define WRITE_DATA_FIFO_PTR_WIDTH_WIDTH 8
#define WRITE_FIFO_STATUS_WIDTH        8
#define WRITE_MODEREG_DONE_BIT         33
#define WRITE_MODEREG_OFFSET           0
#define WRITE_MODEREG_WIDTH            27
#define WRITEINTERP_OFFSET             8
#define WRITEINTERP_WIDTH              1
#define WRLAT_ADJ_F0_OFFSET            8
#define WRLAT_ADJ_F0_WIDTH             8
#define WRLAT_ADJ_F1_OFFSET            8
#define WRLAT_ADJ_F1_WIDTH             8
#define WRLAT_ADJ_F2_OFFSET            8
#define WRLAT_ADJ_F2_WIDTH             8
#define WRLAT_ADJ_MAX                  256
#define WRLAT_ADJ_WIDTH                8
#define WRLAT_F0_OFFSET                8
#define WRLAT_F0_WIDTH                 7
#define WRLAT_F1_OFFSET                8
#define WRLAT_F1_WIDTH                 7
#define WRLAT_F2_OFFSET                8
#define WRLAT_F2_WIDTH                 7
#define WRLAT_WIDTH                    7
#define WRLVL_AREF_EN_OFFSET           16
#define WRLVL_AREF_EN_WIDTH            1
#define WRLVL_CS_MAP_OFFSET            0
#define WRLVL_CS_MAP_WIDTH             2
#define WRLVL_CS_OFFSET                16
#define WRLVL_CS_WIDTH                 1
#define WRLVL_DFI_PROMOTE_THRESHOLD_F0_OFFSET 16
#define WRLVL_DFI_PROMOTE_THRESHOLD_F0_WIDTH 16
#define WRLVL_DFI_PROMOTE_THRESHOLD_F1_OFFSET 0
#define WRLVL_DFI_PROMOTE_THRESHOLD_F1_WIDTH 16
#define WRLVL_DFI_PROMOTE_THRESHOLD_F2_OFFSET 16
#define WRLVL_DFI_PROMOTE_THRESHOLD_F2_WIDTH 16
#define WRLVL_DFI_PROMOTE_THRESHOLD_WIDTH 16
#define WRLVL_EN_OFFSET                8
#define WRLVL_EN_WIDTH                 1
#define WRLVL_ERROR_BIT                11
#define WRLVL_ERROR_STATUS_OFFSET      8
#define WRLVL_ERROR_STATUS_WIDTH       3
#define WRLVL_HIGH_THRESHOLD_F0_OFFSET 0
#define WRLVL_HIGH_THRESHOLD_F0_WIDTH  16
#define WRLVL_HIGH_THRESHOLD_F1_OFFSET 16
#define WRLVL_HIGH_THRESHOLD_F1_WIDTH  16
#define WRLVL_HIGH_THRESHOLD_F2_OFFSET 0
#define WRLVL_HIGH_THRESHOLD_F2_WIDTH  16
#define WRLVL_HIGH_THRESHOLD_WIDTH     16
#define WRLVL_NORM_THRESHOLD_F0_OFFSET 16
#define WRLVL_NORM_THRESHOLD_F0_WIDTH  16
#define WRLVL_NORM_THRESHOLD_F1_OFFSET 0
#define WRLVL_NORM_THRESHOLD_F1_WIDTH  16
#define WRLVL_NORM_THRESHOLD_F2_OFFSET 16
#define WRLVL_NORM_THRESHOLD_F2_WIDTH  16
#define WRLVL_NORM_THRESHOLD_WIDTH     16
#define WRLVL_ON_SREF_EXIT_OFFSET      0
#define WRLVL_ON_SREF_EXIT_WIDTH       1
#define WRLVL_PERIODIC_OFFSET          24
#define WRLVL_PERIODIC_WIDTH           1
#define WRLVL_REQ_BIT                  16
#define WRLVL_REQ_OFFSET               8
#define WRLVL_REQ_WIDTH                1
#define WRLVL_RESP_MASK_OFFSET         8
#define WRLVL_RESP_MASK_WIDTH          4
#define WRLVL_ROTATE_OFFSET            24
#define WRLVL_ROTATE_WIDTH             1
#define WRLVL_SW_PROMOTE_THRESHOLD_F0_OFFSET 0
#define WRLVL_SW_PROMOTE_THRESHOLD_F0_WIDTH 16
#define WRLVL_SW_PROMOTE_THRESHOLD_F1_OFFSET 16
#define WRLVL_SW_PROMOTE_THRESHOLD_F1_WIDTH 16
#define WRLVL_SW_PROMOTE_THRESHOLD_F2_OFFSET 0
#define WRLVL_SW_PROMOTE_THRESHOLD_F2_WIDTH 16
#define WRLVL_SW_PROMOTE_THRESHOLD_WIDTH 16
#define WRLVL_TIMEOUT_F0_OFFSET        16
#define WRLVL_TIMEOUT_F0_WIDTH         16
#define WRLVL_TIMEOUT_F1_OFFSET        0
#define WRLVL_TIMEOUT_F1_WIDTH         16
#define WRLVL_TIMEOUT_F2_OFFSET        16
#define WRLVL_TIMEOUT_F2_WIDTH         16
#define WRLVL_TIMEOUT_WIDTH            16
#define WRRESP_REQ_P                   256
#define XLATE_RATIO_WIDTH              1
#define ZQ_ARB_REQ_WIDTH               4
#define ZQ_CAL_LATCH_MAP_0_OFFSET      8
#define ZQ_CAL_LATCH_MAP_0_WIDTH       2
#define ZQ_CAL_LATCH_MAP_1_OFFSET      24
#define ZQ_CAL_LATCH_MAP_1_WIDTH       2
#define ZQ_CAL_START_MAP_0_OFFSET      0
#define ZQ_CAL_START_MAP_0_WIDTH       2
#define ZQ_CAL_START_MAP_1_OFFSET      16
#define ZQ_CAL_START_MAP_1_WIDTH       2
#define ZQ_CALINIT_CS_CL_STATUS_OFFSET 0
#define ZQ_CALINIT_CS_CL_STATUS_WIDTH  2
#define ZQ_CALLATCH_HIGH_THRESHOLD_F0_OFFSET 0
#define ZQ_CALLATCH_HIGH_THRESHOLD_F0_WIDTH 16
#define ZQ_CALLATCH_HIGH_THRESHOLD_F1_OFFSET 16
#define ZQ_CALLATCH_HIGH_THRESHOLD_F1_WIDTH 16
#define ZQ_CALLATCH_HIGH_THRESHOLD_F2_OFFSET 0
#define ZQ_CALLATCH_HIGH_THRESHOLD_F2_WIDTH 16
#define ZQ_CALLATCH_HIGH_THRESHOLD_WIDTH 16
#define ZQ_CALLATCH_STATUS_OFFSET      24
#define ZQ_CALLATCH_STATUS_WIDTH       2
#define ZQ_CALLATCH_TIMEOUT_F0_OFFSET  0
#define ZQ_CALLATCH_TIMEOUT_F0_WIDTH   16
#define ZQ_CALLATCH_TIMEOUT_F1_OFFSET  16
#define ZQ_CALLATCH_TIMEOUT_F1_WIDTH   16
#define ZQ_CALLATCH_TIMEOUT_F2_OFFSET  0
#define ZQ_CALLATCH_TIMEOUT_F2_WIDTH   16
#define ZQ_CALLATCH_TIMEOUT_WIDTH      16
#define ZQ_CALSTART_HIGH_THRESHOLD_F0_OFFSET 16
#define ZQ_CALSTART_HIGH_THRESHOLD_F0_WIDTH 16
#define ZQ_CALSTART_HIGH_THRESHOLD_F1_OFFSET 0
#define ZQ_CALSTART_HIGH_THRESHOLD_F1_WIDTH 16
#define ZQ_CALSTART_HIGH_THRESHOLD_F2_OFFSET 16
#define ZQ_CALSTART_HIGH_THRESHOLD_F2_WIDTH 16
#define ZQ_CALSTART_HIGH_THRESHOLD_WIDTH 16
#define ZQ_CALSTART_NORM_THRESHOLD_F0_OFFSET 0
#define ZQ_CALSTART_NORM_THRESHOLD_F0_WIDTH 16
#define ZQ_CALSTART_NORM_THRESHOLD_F1_OFFSET 16
#define ZQ_CALSTART_NORM_THRESHOLD_F1_WIDTH 16
#define ZQ_CALSTART_NORM_THRESHOLD_F2_OFFSET 0
#define ZQ_CALSTART_NORM_THRESHOLD_F2_WIDTH 16
#define ZQ_CALSTART_NORM_THRESHOLD_WIDTH 16
#define ZQ_CALSTART_STATUS_OFFSET      16
#define ZQ_CALSTART_STATUS_WIDTH       2
#define ZQ_CALSTART_TIMEOUT_F0_OFFSET  16
#define ZQ_CALSTART_TIMEOUT_F0_WIDTH   16
#define ZQ_CALSTART_TIMEOUT_F1_OFFSET  0
#define ZQ_CALSTART_TIMEOUT_F1_WIDTH   16
#define ZQ_CALSTART_TIMEOUT_F2_OFFSET  16
#define ZQ_CALSTART_TIMEOUT_F2_WIDTH   16
#define ZQ_CALSTART_TIMEOUT_WIDTH      16
#define ZQ_CMD_REQ_WIDTH               4
#define ZQ_CS_HIGH_THRESHOLD_F0_OFFSET 0
#define ZQ_CS_HIGH_THRESHOLD_F0_WIDTH  16
#define ZQ_CS_HIGH_THRESHOLD_F1_OFFSET 16
#define ZQ_CS_HIGH_THRESHOLD_F1_WIDTH  16
#define ZQ_CS_HIGH_THRESHOLD_F2_OFFSET 0
#define ZQ_CS_HIGH_THRESHOLD_F2_WIDTH  16
#define ZQ_CS_HIGH_THRESHOLD_WIDTH     16
#define ZQ_CS_NORM_THRESHOLD_F0_OFFSET 16
#define ZQ_CS_NORM_THRESHOLD_F0_WIDTH  16
#define ZQ_CS_NORM_THRESHOLD_F1_OFFSET 0
#define ZQ_CS_NORM_THRESHOLD_F1_WIDTH  16
#define ZQ_CS_NORM_THRESHOLD_F2_OFFSET 16
#define ZQ_CS_NORM_THRESHOLD_F2_WIDTH  16
#define ZQ_CS_NORM_THRESHOLD_WIDTH     16
#define ZQ_CS_TIMEOUT_F0_OFFSET        16
#define ZQ_CS_TIMEOUT_F0_WIDTH         16
#define ZQ_CS_TIMEOUT_F1_OFFSET        0
#define ZQ_CS_TIMEOUT_F1_WIDTH         16
#define ZQ_CS_TIMEOUT_F2_OFFSET        16
#define ZQ_CS_TIMEOUT_F2_WIDTH         16
#define ZQ_CS_TIMEOUT_WIDTH            16
#define ZQ_PROMOTE_THRESHOLD_F0_OFFSET 0
#define ZQ_PROMOTE_THRESHOLD_F0_WIDTH  16
#define ZQ_PROMOTE_THRESHOLD_F1_OFFSET 16
#define ZQ_PROMOTE_THRESHOLD_F1_WIDTH  16
#define ZQ_PROMOTE_THRESHOLD_F2_OFFSET 0
#define ZQ_PROMOTE_THRESHOLD_F2_WIDTH  16
#define ZQ_PROMOTE_THRESHOLD_WIDTH     16
#define ZQ_REQ_OFFSET                  16
#define ZQ_REQ_PENDING_OFFSET          24
#define ZQ_REQ_PENDING_WIDTH           1
#define ZQ_REQ_WIDTH                   4
#define ZQ_STATUS_BIT                  41
#define ZQ_STATUS_WIDTH                2
#define ZQ_SW_REQ_START_LATCH_MAP_OFFSET 8
#define ZQ_SW_REQ_START_LATCH_MAP_WIDTH 2
#define ZQCL_F0_OFFSET                 16
#define ZQCL_F0_WIDTH                  12
#define ZQCL_F1_OFFSET                 0
#define ZQCL_F1_WIDTH                  12
#define ZQCL_F2_OFFSET                 16
#define ZQCL_F2_WIDTH                  12
#define ZQCL_WIDTH                     12
#define ZQCS_F0_OFFSET                 0
#define ZQCS_F0_WIDTH                  12
#define ZQCS_F1_OFFSET                 16
#define ZQCS_F1_WIDTH                  12
#define ZQCS_F2_OFFSET                 0
#define ZQCS_F2_WIDTH                  12
#define ZQCS_OPT_THRESHOLD_OFFSET      16
#define ZQCS_OPT_THRESHOLD_WIDTH       3
#define ZQCS_ROTATE_OFFSET             24
#define ZQCS_ROTATE_WIDTH              1
#define ZQINIT_F0_OFFSET               0
#define ZQINIT_F0_WIDTH                12
#define ZQINIT_F1_OFFSET               8
#define ZQINIT_F1_WIDTH                12
#define ZQINIT_F2_OFFSET               0
#define ZQINIT_F2_WIDTH                12
#define ZQINIT_WIDTH                   12
#define ZQRESET_F0_OFFSET              0
#define ZQRESET_F0_WIDTH               12
#define ZQRESET_F1_OFFSET              16
#define ZQRESET_F1_WIDTH               12
#define ZQRESET_F2_OFFSET              0
#define ZQRESET_F2_WIDTH               12
#define ZQRESET_WIDTH                  12

#define DENALI_CTL_00_ADDR (4*0)
#define DENALI_CTL_01_ADDR (4*1)
#define DENALI_CTL_02_ADDR (4*2)
#define DENALI_CTL_03_ADDR (4*3)
#define DENALI_CTL_04_ADDR (4*4)
#define DENALI_CTL_05_ADDR (4*5)
#define DENALI_CTL_06_ADDR (4*6)
#define DENALI_CTL_07_ADDR (4*7)
#define DENALI_CTL_08_ADDR (4*8)
#define DENALI_CTL_09_ADDR (4*9)
#define DENALI_CTL_10_ADDR (4*10)
#define DENALI_CTL_11_ADDR (4*11)
#define DENALI_CTL_12_ADDR (4*12)
#define DENALI_CTL_13_ADDR (4*13)
#define DENALI_CTL_14_ADDR (4*14)
#define DENALI_CTL_15_ADDR (4*15)
#define DENALI_CTL_16_ADDR (4*16)
#define DENALI_CTL_17_ADDR (4*17)
#define DENALI_CTL_18_ADDR (4*18)
#define DENALI_CTL_19_ADDR (4*19)
#define DENALI_CTL_20_ADDR (4*20)
#define DENALI_CTL_21_ADDR (4*21)
#define DENALI_CTL_22_ADDR (4*22)
#define DENALI_CTL_23_ADDR (4*23)
#define DENALI_CTL_24_ADDR (4*24)
#define DENALI_CTL_25_ADDR (4*25)
#define DENALI_CTL_26_ADDR (4*26)
#define DENALI_CTL_27_ADDR (4*27)
#define DENALI_CTL_28_ADDR (4*28)
#define DENALI_CTL_29_ADDR (4*29)
#define DENALI_CTL_30_ADDR (4*30)
#define DENALI_CTL_31_ADDR (4*31)
#define DENALI_CTL_32_ADDR (4*32)
#define DENALI_CTL_33_ADDR (4*33)
#define DENALI_CTL_34_ADDR (4*34)
#define DENALI_CTL_35_ADDR (4*35)
#define DENALI_CTL_36_ADDR (4*36)
#define DENALI_CTL_37_ADDR (4*37)
#define DENALI_CTL_38_ADDR (4*38)
#define DENALI_CTL_39_ADDR (4*39)
#define DENALI_CTL_40_ADDR (4*40)
#define DENALI_CTL_41_ADDR (4*41)
#define DENALI_CTL_42_ADDR (4*42)
#define DENALI_CTL_43_ADDR (4*43)
#define DENALI_CTL_44_ADDR (4*44)
#define DENALI_CTL_45_ADDR (4*45)
#define DENALI_CTL_46_ADDR (4*46)
#define DENALI_CTL_47_ADDR (4*47)
#define DENALI_CTL_48_ADDR (4*48)
#define DENALI_CTL_49_ADDR (4*49)
#define DENALI_CTL_50_ADDR (4*50)
#define DENALI_CTL_51_ADDR (4*51)
#define DENALI_CTL_52_ADDR (4*52)
#define DENALI_CTL_53_ADDR (4*53)
#define DENALI_CTL_54_ADDR (4*54)
#define DENALI_CTL_55_ADDR (4*55)
#define DENALI_CTL_56_ADDR (4*56)
#define DENALI_CTL_57_ADDR (4*57)
#define DENALI_CTL_58_ADDR (4*58)
#define DENALI_CTL_59_ADDR (4*59)
#define DENALI_CTL_60_ADDR (4*60)
#define DENALI_CTL_61_ADDR (4*61)
#define DENALI_CTL_62_ADDR (4*62)
#define DENALI_CTL_63_ADDR (4*63)
#define DENALI_CTL_64_ADDR (4*64)
#define DENALI_CTL_65_ADDR (4*65)
#define DENALI_CTL_66_ADDR (4*66)
#define DENALI_CTL_67_ADDR (4*67)
#define DENALI_CTL_68_ADDR (4*68)
#define DENALI_CTL_69_ADDR (4*69)
#define DENALI_CTL_70_ADDR (4*70)
#define DENALI_CTL_71_ADDR (4*71)
#define DENALI_CTL_72_ADDR (4*72)
#define DENALI_CTL_73_ADDR (4*73)
#define DENALI_CTL_74_ADDR (4*74)
#define DENALI_CTL_75_ADDR (4*75)
#define DENALI_CTL_76_ADDR (4*76)
#define DENALI_CTL_77_ADDR (4*77)
#define DENALI_CTL_78_ADDR (4*78)
#define DENALI_CTL_79_ADDR (4*79)
#define DENALI_CTL_80_ADDR (4*80)
#define DENALI_CTL_81_ADDR (4*81)
#define DENALI_CTL_82_ADDR (4*82)
#define DENALI_CTL_83_ADDR (4*83)
#define DENALI_CTL_84_ADDR (4*84)
#define DENALI_CTL_85_ADDR (4*85)
#define DENALI_CTL_86_ADDR (4*86)
#define DENALI_CTL_87_ADDR (4*87)
#define DENALI_CTL_88_ADDR (4*88)
#define DENALI_CTL_89_ADDR (4*89)
#define DENALI_CTL_90_ADDR (4*90)
#define DENALI_CTL_91_ADDR (4*91)
#define DENALI_CTL_92_ADDR (4*92)
#define DENALI_CTL_93_ADDR (4*93)
#define DENALI_CTL_94_ADDR (4*94)
#define DENALI_CTL_95_ADDR (4*95)
#define DENALI_CTL_96_ADDR (4*96)
#define DENALI_CTL_97_ADDR (4*97)
#define DENALI_CTL_98_ADDR (4*98)
#define DENALI_CTL_99_ADDR (4*99)
#define DENALI_CTL_100_ADDR (4*100)
#define DENALI_CTL_101_ADDR (4*101)
#define DENALI_CTL_102_ADDR (4*102)
#define DENALI_CTL_103_ADDR (4*103)
#define DENALI_CTL_104_ADDR (4*104)
#define DENALI_CTL_105_ADDR (4*105)
#define DENALI_CTL_106_ADDR (4*106)
#define DENALI_CTL_107_ADDR (4*107)
#define DENALI_CTL_108_ADDR (4*108)
#define DENALI_CTL_109_ADDR (4*109)
#define DENALI_CTL_110_ADDR (4*110)
#define DENALI_CTL_111_ADDR (4*111)
#define DENALI_CTL_112_ADDR (4*112)
#define DENALI_CTL_113_ADDR (4*113)
#define DENALI_CTL_114_ADDR (4*114)
#define DENALI_CTL_115_ADDR (4*115)
#define DENALI_CTL_116_ADDR (4*116)
#define DENALI_CTL_117_ADDR (4*117)
#define DENALI_CTL_118_ADDR (4*118)
#define DENALI_CTL_119_ADDR (4*119)
#define DENALI_CTL_120_ADDR (4*120)
#define DENALI_CTL_121_ADDR (4*121)
#define DENALI_CTL_122_ADDR (4*122)
#define DENALI_CTL_123_ADDR (4*123)
#define DENALI_CTL_124_ADDR (4*124)
#define DENALI_CTL_125_ADDR (4*125)
#define DENALI_CTL_126_ADDR (4*126)
#define DENALI_CTL_127_ADDR (4*127)
#define DENALI_CTL_128_ADDR (4*128)
#define DENALI_CTL_129_ADDR (4*129)
#define DENALI_CTL_130_ADDR (4*130)
#define DENALI_CTL_131_ADDR (4*131)
#define DENALI_CTL_132_ADDR (4*132)
#define DENALI_CTL_133_ADDR (4*133)
#define DENALI_CTL_134_ADDR (4*134)
#define DENALI_CTL_135_ADDR (4*135)
#define DENALI_CTL_136_ADDR (4*136)
#define DENALI_CTL_137_ADDR (4*137)
#define DENALI_CTL_138_ADDR (4*138)
#define DENALI_CTL_139_ADDR (4*139)
#define DENALI_CTL_140_ADDR (4*140)
#define DENALI_CTL_141_ADDR (4*141)
#define DENALI_CTL_142_ADDR (4*142)
#define DENALI_CTL_143_ADDR (4*143)
#define DENALI_CTL_144_ADDR (4*144)
#define DENALI_CTL_145_ADDR (4*145)
#define DENALI_CTL_146_ADDR (4*146)
#define DENALI_CTL_147_ADDR (4*147)
#define DENALI_CTL_148_ADDR (4*148)
#define DENALI_CTL_149_ADDR (4*149)
#define DENALI_CTL_150_ADDR (4*150)
#define DENALI_CTL_151_ADDR (4*151)
#define DENALI_CTL_152_ADDR (4*152)
#define DENALI_CTL_153_ADDR (4*153)
#define DENALI_CTL_154_ADDR (4*154)
#define DENALI_CTL_155_ADDR (4*155)
#define DENALI_CTL_156_ADDR (4*156)
#define DENALI_CTL_157_ADDR (4*157)
#define DENALI_CTL_158_ADDR (4*158)
#define DENALI_CTL_159_ADDR (4*159)
#define DENALI_CTL_160_ADDR (4*160)
#define DENALI_CTL_161_ADDR (4*161)
#define DENALI_CTL_162_ADDR (4*162)
#define DENALI_CTL_163_ADDR (4*163)
#define DENALI_CTL_164_ADDR (4*164)
#define DENALI_CTL_165_ADDR (4*165)
#define DENALI_CTL_166_ADDR (4*166)
#define DENALI_CTL_167_ADDR (4*167)
#define DENALI_CTL_168_ADDR (4*168)
#define DENALI_CTL_169_ADDR (4*169)
#define DENALI_CTL_170_ADDR (4*170)
#define DENALI_CTL_171_ADDR (4*171)
#define DENALI_CTL_172_ADDR (4*172)
#define DENALI_CTL_173_ADDR (4*173)
#define DENALI_CTL_174_ADDR (4*174)
#define DENALI_CTL_175_ADDR (4*175)
#define DENALI_CTL_176_ADDR (4*176)
#define DENALI_CTL_177_ADDR (4*177)
#define DENALI_CTL_178_ADDR (4*178)
#define DENALI_CTL_179_ADDR (4*179)
#define DENALI_CTL_180_ADDR (4*180)
#define DENALI_CTL_181_ADDR (4*181)
#define DENALI_CTL_182_ADDR (4*182)
#define DENALI_CTL_183_ADDR (4*183)
#define DENALI_CTL_184_ADDR (4*184)
#define DENALI_CTL_185_ADDR (4*185)
#define DENALI_CTL_186_ADDR (4*186)
#define DENALI_CTL_187_ADDR (4*187)
#define DENALI_CTL_188_ADDR (4*188)
#define DENALI_CTL_189_ADDR (4*189)
#define DENALI_CTL_190_ADDR (4*190)
#define DENALI_CTL_191_ADDR (4*191)
#define DENALI_CTL_192_ADDR (4*192)
#define DENALI_CTL_193_ADDR (4*193)
#define DENALI_CTL_194_ADDR (4*194)
#define DENALI_CTL_195_ADDR (4*195)
#define DENALI_CTL_196_ADDR (4*196)
#define DENALI_CTL_197_ADDR (4*197)
#define DENALI_CTL_198_ADDR (4*198)
#define DENALI_CTL_199_ADDR (4*199)
#define DENALI_CTL_200_ADDR (4*200)
#define DENALI_CTL_201_ADDR (4*201)
#define DENALI_CTL_202_ADDR (4*202)
#define DENALI_CTL_203_ADDR (4*203)
#define DENALI_CTL_204_ADDR (4*204)
#define DENALI_CTL_205_ADDR (4*205)
#define DENALI_CTL_206_ADDR (4*206)
#define DENALI_CTL_207_ADDR (4*207)
#define DENALI_CTL_208_ADDR (4*208)
#define DENALI_CTL_209_ADDR (4*209)
#define DENALI_CTL_210_ADDR (4*210)
#define DENALI_CTL_211_ADDR (4*211)
#define DENALI_CTL_212_ADDR (4*212)
#define DENALI_CTL_213_ADDR (4*213)
#define DENALI_CTL_214_ADDR (4*214)
#define DENALI_CTL_215_ADDR (4*215)
#define DENALI_CTL_216_ADDR (4*216)
#define DENALI_CTL_217_ADDR (4*217)
#define DENALI_CTL_218_ADDR (4*218)
#define DENALI_CTL_219_ADDR (4*219)
#define DENALI_CTL_220_ADDR (4*220)
#define DENALI_CTL_221_ADDR (4*221)
#define DENALI_CTL_222_ADDR (4*222)
#define DENALI_CTL_223_ADDR (4*223)
#define DENALI_CTL_224_ADDR (4*224)
#define DENALI_CTL_225_ADDR (4*225)
#define DENALI_CTL_226_ADDR (4*226)
#define DENALI_CTL_227_ADDR (4*227)
#define DENALI_CTL_228_ADDR (4*228)
#define DENALI_CTL_229_ADDR (4*229)
#define DENALI_CTL_230_ADDR (4*230)
#define DENALI_CTL_231_ADDR (4*231)
#define DENALI_CTL_232_ADDR (4*232)
#define DENALI_CTL_233_ADDR (4*233)
#define DENALI_CTL_234_ADDR (4*234)
#define DENALI_CTL_235_ADDR (4*235)
#define DENALI_CTL_236_ADDR (4*236)
#define DENALI_CTL_237_ADDR (4*237)
#define DENALI_CTL_238_ADDR (4*238)
#define DENALI_CTL_239_ADDR (4*239)
#define DENALI_CTL_240_ADDR (4*240)
#define DENALI_CTL_241_ADDR (4*241)
#define DENALI_CTL_242_ADDR (4*242)
#define DENALI_CTL_243_ADDR (4*243)
#define DENALI_CTL_244_ADDR (4*244)
#define DENALI_CTL_245_ADDR (4*245)
#define DENALI_CTL_246_ADDR (4*246)
#define DENALI_CTL_247_ADDR (4*247)
#define DENALI_CTL_248_ADDR (4*248)
#define DENALI_CTL_249_ADDR (4*249)
#define DENALI_CTL_250_ADDR (4*250)
#define DENALI_CTL_251_ADDR (4*251)
#define DENALI_CTL_252_ADDR (4*252)
#define DENALI_CTL_253_ADDR (4*253)
#define DENALI_CTL_254_ADDR (4*254)
#define DENALI_CTL_255_ADDR (4*255)
#define DENALI_CTL_256_ADDR (4*256)
#define DENALI_CTL_257_ADDR (4*257)
#define DENALI_CTL_258_ADDR (4*258)
#define DENALI_CTL_259_ADDR (4*259)
#define DENALI_CTL_260_ADDR (4*260)
#define DENALI_CTL_261_ADDR (4*261)
#define DENALI_CTL_262_ADDR (4*262)
#define DENALI_CTL_263_ADDR (4*263)
#define DENALI_CTL_264_ADDR (4*264)
#define DENALI_CTL_265_ADDR (4*265)
#define DENALI_CTL_266_ADDR (4*266)
#define DENALI_CTL_267_ADDR (4*267)
#define DENALI_CTL_268_ADDR (4*268)
#define DENALI_CTL_269_ADDR (4*269)
#define DENALI_CTL_270_ADDR (4*270)
#define DENALI_CTL_271_ADDR (4*271)
#define DENALI_CTL_272_ADDR (4*272)
#define DENALI_CTL_273_ADDR (4*273)
#define DENALI_CTL_274_ADDR (4*274)
#define DENALI_CTL_275_ADDR (4*275)
#define DENALI_CTL_276_ADDR (4*276)
#define DENALI_CTL_277_ADDR (4*277)
#define DENALI_CTL_278_ADDR (4*278)
#define DENALI_CTL_279_ADDR (4*279)
#define DENALI_CTL_280_ADDR (4*280)
#define DENALI_CTL_281_ADDR (4*281)
#define DENALI_CTL_282_ADDR (4*282)
#define DENALI_CTL_283_ADDR (4*283)
#define DENALI_CTL_284_ADDR (4*284)
#define DENALI_CTL_285_ADDR (4*285)
#define DENALI_CTL_286_ADDR (4*286)
#define DENALI_CTL_287_ADDR (4*287)
#define DENALI_CTL_288_ADDR (4*288)
#define DENALI_CTL_289_ADDR (4*289)
#define DENALI_CTL_290_ADDR (4*290)
#define DENALI_CTL_291_ADDR (4*291)
#define DENALI_CTL_292_ADDR (4*292)
#define DENALI_CTL_293_ADDR (4*293)
#define DENALI_CTL_294_ADDR (4*294)
#define DENALI_CTL_295_ADDR (4*295)
#define DENALI_CTL_296_ADDR (4*296)
#define DENALI_CTL_297_ADDR (4*297)
#define DENALI_CTL_298_ADDR (4*298)
#define DENALI_CTL_299_ADDR (4*299)
#define DENALI_CTL_300_ADDR (4*300)
#define DENALI_CTL_301_ADDR (4*301)
#define DENALI_CTL_302_ADDR (4*302)
#define DENALI_CTL_303_ADDR (4*303)
#define DENALI_CTL_304_ADDR (4*304)
#define DENALI_CTL_305_ADDR (4*305)
#define DENALI_CTL_306_ADDR (4*306)
#define DENALI_CTL_307_ADDR (4*307)
#define DENALI_CTL_308_ADDR (4*308)
#define DENALI_CTL_309_ADDR (4*309)
#define DENALI_CTL_310_ADDR (4*310)
#define DENALI_CTL_311_ADDR (4*311)
#define DENALI_CTL_312_ADDR (4*312)
#define DENALI_CTL_313_ADDR (4*313)
#define DENALI_CTL_314_ADDR (4*314)
#define DENALI_CTL_315_ADDR (4*315)
#define DENALI_CTL_316_ADDR (4*316)
#define DENALI_CTL_317_ADDR (4*317)
#define DENALI_CTL_318_ADDR (4*318)
#define DENALI_CTL_319_ADDR (4*319)
#define DENALI_CTL_320_ADDR (4*320)
#define DENALI_CTL_321_ADDR (4*321)
#define DENALI_CTL_322_ADDR (4*322)
#define DENALI_CTL_323_ADDR (4*323)
#define DENALI_CTL_324_ADDR (4*324)
#define DENALI_CTL_325_ADDR (4*325)
#define DENALI_CTL_326_ADDR (4*326)
#define DENALI_CTL_327_ADDR (4*327)
#define DENALI_CTL_328_ADDR (4*328)
#define DENALI_CTL_329_ADDR (4*329)
#define DENALI_CTL_330_ADDR (4*330)
#define DENALI_CTL_331_ADDR (4*331)
#define DENALI_CTL_332_ADDR (4*332)
#define DENALI_CTL_333_ADDR (4*333)
#define DENALI_CTL_334_ADDR (4*334)
#define DENALI_CTL_335_ADDR (4*335)
#define DENALI_CTL_336_ADDR (4*336)
#define DENALI_CTL_337_ADDR (4*337)
#define DENALI_CTL_338_ADDR (4*338)
#define DENALI_CTL_339_ADDR (4*339)
#define DENALI_CTL_340_ADDR (4*340)
#define DENALI_CTL_341_ADDR (4*341)
#define DENALI_CTL_342_ADDR (4*342)
#define DENALI_CTL_343_ADDR (4*343)
#define DENALI_CTL_344_ADDR (4*344)
#define DENALI_CTL_345_ADDR (4*345)
#define DENALI_CTL_346_ADDR (4*346)
#define DENALI_CTL_347_ADDR (4*347)
#define DENALI_CTL_348_ADDR (4*348)
#define DENALI_CTL_349_ADDR (4*349)
#define DENALI_CTL_350_ADDR (4*350)
#define DENALI_CTL_351_ADDR (4*351)
#define DENALI_CTL_352_ADDR (4*352)
#define DENALI_CTL_353_ADDR (4*353)
#define DENALI_CTL_354_ADDR (4*354)
#define DENALI_CTL_355_ADDR (4*355)
#define DENALI_CTL_356_ADDR (4*356)
#define DENALI_CTL_357_ADDR (4*357)
#define DENALI_CTL_358_ADDR (4*358)
#define DENALI_CTL_359_ADDR (4*359)
#define DENALI_CTL_360_ADDR (4*360)
#define DENALI_CTL_361_ADDR (4*361)
#define DENALI_CTL_362_ADDR (4*362)
#define DENALI_CTL_363_ADDR (4*363)
#define DENALI_CTL_364_ADDR (4*364)
#define DENALI_CTL_365_ADDR (4*365)
#define DENALI_CTL_366_ADDR (4*366)
#define DENALI_CTL_367_ADDR (4*367)
#define DENALI_CTL_368_ADDR (4*368)
#define DENALI_CTL_369_ADDR (4*369)
#define DENALI_CTL_370_ADDR (4*370)
#define DENALI_CTL_371_ADDR (4*371)
#define DENALI_CTL_372_ADDR (4*372)
#define DENALI_CTL_373_ADDR (4*373)
#define DENALI_CTL_374_ADDR (4*374)
#define DENALI_CTL_375_ADDR (4*375)
#define DENALI_CTL_376_ADDR (4*376)
#define DENALI_CTL_377_ADDR (4*377)
#define DENALI_CTL_378_ADDR (4*378)
#define DENALI_CTL_379_ADDR (4*379)
#define DENALI_CTL_380_ADDR (4*380)
#define DENALI_CTL_381_ADDR (4*381)
#define DENALI_CTL_382_ADDR (4*382)
#define DENALI_CTL_383_ADDR (4*383)
#define DENALI_CTL_384_ADDR (4*384)
#define DENALI_CTL_385_ADDR (4*385)
#define DENALI_CTL_386_ADDR (4*386)
#define DENALI_CTL_387_ADDR (4*387)
#define DENALI_CTL_388_ADDR (4*388)
#define DENALI_CTL_389_ADDR (4*389)
#define DENALI_CTL_390_ADDR (4*390)
#define DENALI_CTL_391_ADDR (4*391)
#define DENALI_CTL_392_ADDR (4*392)
#define DENALI_CTL_393_ADDR (4*393)
#define DENALI_CTL_394_ADDR (4*394)
#define DENALI_CTL_395_ADDR (4*395)
#define DENALI_CTL_396_ADDR (4*396)
#define DENALI_CTL_397_ADDR (4*397)
#define DENALI_CTL_398_ADDR (4*398)
#define DENALI_CTL_399_ADDR (4*399)
#define DENALI_CTL_400_ADDR (4*400)
#define DENALI_CTL_401_ADDR (4*401)
#define DENALI_CTL_402_ADDR (4*402)
#define DENALI_CTL_403_ADDR (4*403)
#define DENALI_CTL_404_ADDR (4*404)
#define DENALI_CTL_405_ADDR (4*405)
#define DENALI_CTL_406_ADDR (4*406)
#define DENALI_CTL_407_ADDR (4*407)
#define DENALI_CTL_408_ADDR (4*408)
#define DENALI_CTL_409_ADDR (4*409)
#define DENALI_CTL_410_ADDR (4*410)
#define DENALI_CTL_411_ADDR (4*411)
#define DENALI_CTL_412_ADDR (4*412)
#define DENALI_CTL_413_ADDR (4*413)
#define DENALI_CTL_414_ADDR (4*414)
#define DENALI_CTL_415_ADDR (4*415)
#define DENALI_CTL_416_ADDR (4*416)
#define DENALI_CTL_417_ADDR (4*417)
#define DENALI_CTL_418_ADDR (4*418)
#define DENALI_CTL_419_ADDR (4*419)
#define DENALI_CTL_420_ADDR (4*420)
#define DENALI_CTL_421_ADDR (4*421)
#define DENALI_CTL_422_ADDR (4*422)
#define DENALI_CTL_423_ADDR (4*423)
#define DENALI_CTL_424_ADDR (4*424)
#define DENALI_CTL_425_ADDR (4*425)
#define DENALI_CTL_426_ADDR (4*426)
#define DENALI_CTL_427_ADDR (4*427)
#define DENALI_CTL_428_ADDR (4*428)
#define DENALI_CTL_429_ADDR (4*429)
#define DENALI_CTL_430_ADDR (4*430)
#define DENALI_CTL_431_ADDR (4*431)
#define DENALI_CTL_432_ADDR (4*432)
#define DENALI_CTL_433_ADDR (4*433)
#define DENALI_CTL_434_ADDR (4*434)
#define DENALI_CTL_435_ADDR (4*435)
#define DENALI_CTL_436_ADDR (4*436)
#define DENALI_CTL_437_ADDR (4*437)
#define DENALI_CTL_438_ADDR (4*438)
#define DENALI_CTL_439_ADDR (4*439)
#define DENALI_CTL_440_ADDR (4*440)
#define DENALI_CTL_441_ADDR (4*441)
#define DENALI_CTL_442_ADDR (4*442)
#define DENALI_CTL_443_ADDR (4*443)
#define DENALI_CTL_444_ADDR (4*444)
#define DENALI_CTL_445_ADDR (4*445)
#define DENALI_CTL_446_ADDR (4*446)
#define DENALI_CTL_447_ADDR (4*447)
#define DENALI_CTL_448_ADDR (4*448)
#define DENALI_CTL_449_ADDR (4*449)
#define DENALI_CTL_450_ADDR (4*450)
#define DENALI_CTL_451_ADDR (4*451)
#define DENALI_CTL_452_ADDR (4*452)
#define DENALI_CTL_453_ADDR (4*453)
#define DENALI_CTL_454_ADDR (4*454)
#define DENALI_CTL_455_ADDR (4*455)
#define DENALI_CTL_456_ADDR (4*456)
#define DENALI_CTL_457_ADDR (4*457)
#define DENALI_CTL_458_ADDR (4*458)
#define DENALI_CTL_459_ADDR (4*459)
#define DENALI_CTL_460_ADDR (4*460)
#define DENALI_CTL_461_ADDR (4*461)
#define DENALI_CTL_462_ADDR (4*462)
#define DENALI_CTL_463_ADDR (4*463)
#define DENALI_CTL_464_ADDR (4*464)
#define DENALI_CTL_465_ADDR (4*465)
#define DENALI_CTL_466_ADDR (4*466)
#define DENALI_CTL_467_ADDR (4*467)
#define DENALI_CTL_468_ADDR (4*468)
#define DENALI_CTL_469_ADDR (4*469)
#define DENALI_CTL_470_ADDR (4*470)
#define DENALI_CTL_471_ADDR (4*471)
#define DENALI_CTL_472_ADDR (4*472)
#define DENALI_CTL_473_ADDR (4*473)
#define DENALI_CTL_474_ADDR (4*474)
#define DENALI_CTL_475_ADDR (4*475)
#define DENALI_CTL_476_ADDR (4*476)
#define DENALI_CTL_477_ADDR (4*477)
#define DENALI_CTL_478_ADDR (4*478)
#define DENALI_CTL_479_ADDR (4*479)
#define DENALI_CTL_480_ADDR (4*480)
#define DENALI_CTL_481_ADDR (4*481)
#define DENALI_CTL_482_ADDR (4*482)
#define DENALI_CTL_483_ADDR (4*483)
#define DENALI_CTL_484_ADDR (4*484)
#define DENALI_CTL_485_ADDR (4*485)
#define DENALI_CTL_486_ADDR (4*486)
#define DENALI_CTL_487_ADDR (4*487)
#define DENALI_CTL_488_ADDR (4*488)
#define DENALI_CTL_489_ADDR (4*489)
#define DENALI_CTL_490_ADDR (4*490)
#define DENALI_CTL_491_ADDR (4*491)
#define DENALI_CTL_492_ADDR (4*492)
#define DENALI_CTL_493_ADDR (4*493)
#define DENALI_CTL_494_ADDR (4*494)
#define DENALI_CTL_495_ADDR (4*495)
#define DENALI_CTL_496_ADDR (4*496)
#define DENALI_CTL_497_ADDR (4*497)
#define DENALI_CTL_498_ADDR (4*498)
#define DENALI_CTL_499_ADDR (4*499)
#define DENALI_CTL_500_ADDR (4*500)
#define DENALI_CTL_501_ADDR (4*501)
#define DENALI_CTL_502_ADDR (4*502)
#define DENALI_CTL_503_ADDR (4*503)
#define DENALI_CTL_504_ADDR (4*504)
#define DENALI_CTL_505_ADDR (4*504)
#define DENALI_CTL_506_ADDR (4*506)
#define DENALI_CTL_507_ADDR (4*507)
#define DENALI_CTL_508_ADDR (4*508)
#define DENALI_CTL_509_ADDR (4*509)
#define DENALI_CTL_510_ADDR (4*510)
#define DENALI_CTL_511_ADDR (4*511)
#define DENALI_CTL_512_ADDR (4*512)
#define DENALI_CTL_513_ADDR (4*513)
#define DENALI_CTL_514_ADDR (4*514)
#define DENALI_CTL_515_ADDR (4*515)
#define DENALI_CTL_516_ADDR (4*516)
#define DENALI_CTL_517_ADDR (4*517)

#define DENALI_CTL_00_DATA 0b00000000000000000000101100000000
#define DENALI_CTL_01_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_02_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_03_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_04_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_05_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_06_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_07_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_08_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_09_DATA 0b00000000000000000000000000000100
#define DENALI_CTL_10_DATA 0b00000000000000000000000000000001
#define DENALI_CTL_11_DATA 0b00000000000000000000000000000101
#define DENALI_CTL_12_DATA 0b00000000000000000000000000000010
#define DENALI_CTL_13_DATA 0b00000000000000000000000001010000
#define DENALI_CTL_14_DATA 0b00000000000000000000000000000001
#define DENALI_CTL_15_DATA 0b00000000000000000000000000000101
#define DENALI_CTL_16_DATA 0b00000000000000000000000000101000
#define DENALI_CTL_17_DATA 0b00000000000000000000000011010110
#define DENALI_CTL_18_DATA 0b00000000000000000000000000000011
#define DENALI_CTL_19_DATA 0b00000000000000000000000000000101
#define DENALI_CTL_20_DATA 0b00000000000000000000000001101011
#define DENALI_CTL_21_DATA 0b00000001000000000000000000000000
#define DENALI_CTL_22_DATA 0b00000010000000000001000000000001
#define DENALI_CTL_23_DATA 0b00000010000000010000000000000000
#define DENALI_CTL_24_DATA 0b00000000000000000000000100000010
#define DENALI_CTL_25_DATA 0b00000000000000000000000000000100
#define DENALI_CTL_26_DATA 0b00000000000000000000000000001010
#define DENALI_CTL_27_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_28_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_29_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_30_DATA 0b00000010000000000000000000000000
#define DENALI_CTL_31_DATA 0b00101011000100000000000100000010
#define DENALI_CTL_32_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_33_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_34_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_35_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_36_DATA 0b00000000000000000000000000010000
#define DENALI_CTL_37_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_38_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_39_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_40_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_41_DATA 0b00000000000000000000010000001100
#define DENALI_CTL_42_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_43_DATA 0b00000000000000000000011000010100
#define DENALI_CTL_44_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_45_DATA 0b00000000000000000000101000101000
#define DENALI_CTL_46_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_47_DATA 0b00000100000000000000100000000100
#define DENALI_CTL_48_DATA 0b00000011000000000000011100000000
#define DENALI_CTL_49_DATA 0b00000000000001000000000100001001
#define DENALI_CTL_50_DATA 0b00000100000000000000000000000001
#define DENALI_CTL_51_DATA 0b00010001000000000001100100000000
#define DENALI_CTL_52_DATA 0b00000000000010000000000100001001
#define DENALI_CTL_53_DATA 0b00001011000000000000000000010000
#define DENALI_CTL_54_DATA 0b00101101000000000100000100000000
#define DENALI_CTL_55_DATA 0b00000000000101000000110100001100
#define DENALI_CTL_56_DATA 0b00001000001000000000000000101011
#define DENALI_CTL_57_DATA 0b00000000000010100000101000001000
#define DENALI_CTL_58_DATA 0b00000100000000000000001010111110
#define DENALI_CTL_59_DATA 0b00001000000010000000101000000100
#define DENALI_CTL_60_DATA 0b00000000000000000000101000001010
#define DENALI_CTL_61_DATA 0b00000100000000000011011011011000
#define DENALI_CTL_62_DATA 0b00001000000010000000101000000100
#define DENALI_CTL_63_DATA 0b00000000000000000000111100001011
#define DENALI_CTL_64_DATA 0b00001000000000001001001000101100
#define DENALI_CTL_65_DATA 0b00000011000001000000111100001000
#define DENALI_CTL_66_DATA 0b00000100000001000000000000000010
#define DENALI_CTL_67_DATA 0b00010101000101000000100100001000
#define DENALI_CTL_68_DATA 0b00001111000000000000001100001000
#define DENALI_CTL_69_DATA 0b00000000000001010000101000000000
#define DENALI_CTL_70_DATA 0b00000001000010100000101000010100
#define DENALI_CTL_71_DATA 0b00000001000000000000001000000001
#define DENALI_CTL_72_DATA 0b00101001000100010000100000000001
#define DENALI_CTL_73_DATA 0b00010111000010010000010000000100
#define DENALI_CTL_74_DATA 0b00000001000000010000000000000000
#define DENALI_CTL_75_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_76_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_77_DATA 0b00000001000000000000000000000000
#define DENALI_CTL_78_DATA 0b00000000000001100000010000000011
#define DENALI_CTL_79_DATA 0b00000000000000000000000001000110
#define DENALI_CTL_80_DATA 0b00000000000000000000000001110000
#define DENALI_CTL_81_DATA 0b00000000000000000000011000010000
#define DENALI_CTL_82_DATA 0b00000000000000000000000100101011
#define DENALI_CTL_83_DATA 0b00000000000000000001000000110101
#define DENALI_CTL_84_DATA 0b00000000000000000000000000000101
#define DENALI_CTL_85_DATA 0b00000000000000110000000000000000
#define DENALI_CTL_86_DATA 0b00000000000000000000000000000001
#define DENALI_CTL_87_DATA 0b00000000000000000000000000111000
#define DENALI_CTL_88_DATA 0b00000000000000000000000010111011
#define DENALI_CTL_89_DATA 0b00000000000000000000000010010110
#define DENALI_CTL_90_DATA 0b00000000000000000000001000000000
#define DENALI_CTL_91_DATA 0b00000001000000110000000001000000
#define DENALI_CTL_92_DATA 0b00000000000001010000000000010010
#define DENALI_CTL_93_DATA 0b00000000000010000000000000000101
#define DENALI_CTL_94_DATA 0b00000000000010100000000000001010
#define DENALI_CTL_95_DATA 0b00001011000001110000000000011010
#define DENALI_CTL_96_DATA 0b00000101000001010000000100010111
#define DENALI_CTL_97_DATA 0b00000001000000010000001100001010
#define DENALI_CTL_98_DATA 0b00000011000010100000010100000101
#define DENALI_CTL_99_DATA 0b00001000000001100000001000000001
#define DENALI_CTL_100_DATA 0b00000000000000010000010000001111
#define DENALI_CTL_101_DATA 0b00000000000001100000000000000110
#define DENALI_CTL_102_DATA 0b00000000011100110000000001110011
#define DENALI_CTL_103_DATA 0b00000001001100110000000100110011
#define DENALI_CTL_104_DATA 0b00000011000001010000010100000101
#define DENALI_CTL_105_DATA 0b00000011000000010000001100000011
#define DENALI_CTL_106_DATA 0b00000110000001010000010100000101
#define DENALI_CTL_107_DATA 0b00000011000000010000001100000011
#define DENALI_CTL_108_DATA 0b00010000000001100000100000000110
#define DENALI_CTL_109_DATA 0b00000100000000100000100000000100
#define DENALI_CTL_110_DATA 0b00000011000000010000000000000000
#define DENALI_CTL_111_DATA 0b00000000000000010000000000000000
#define DENALI_CTL_112_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_113_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_114_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_115_DATA 0b01000000000000100000000100000000
#define DENALI_CTL_116_DATA 0b00000000000000111000000000010000
#define DENALI_CTL_117_DATA 0b00000000000001010000000000000100
#define DENALI_CTL_118_DATA 0b00000000000000000000000000000100
#define DENALI_CTL_119_DATA 0b00000000000001000000000000000011
#define DENALI_CTL_120_DATA 0b00000000000001000000000000000101
#define DENALI_CTL_121_DATA 0b00000000000000110000000000000000
#define DENALI_CTL_122_DATA 0b00000000000001010000000000000100
#define DENALI_CTL_123_DATA 0b00000000000000000000000000000100
#define DENALI_CTL_124_DATA 0b00000000000000000001000110000000
#define DENALI_CTL_125_DATA 0b00000000000000000001000110000000
#define DENALI_CTL_126_DATA 0b00000000000000000001000110000000
#define DENALI_CTL_127_DATA 0b00000000000000000001000110000000
#define DENALI_CTL_128_DATA 0b00000000000000000001000110000000
#define DENALI_CTL_129_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_130_DATA 0b00000000000000000000000111101010
#define DENALI_CTL_131_DATA 0b00000000000000011000010000000000
#define DENALI_CTL_132_DATA 0b00000000000000011000010000000000
#define DENALI_CTL_133_DATA 0b00000000000000011000010000000000
#define DENALI_CTL_134_DATA 0b00000000000000011000010000000000
#define DENALI_CTL_135_DATA 0b00000000000000011000010000000000
#define DENALI_CTL_136_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_137_DATA 0b00000000000000000010101001110000
#define DENALI_CTL_138_DATA 0b00000000000001000000110101000000
#define DENALI_CTL_139_DATA 0b00000000000001000000110101000000
#define DENALI_CTL_140_DATA 0b00000000000001000000110101000000
#define DENALI_CTL_141_DATA 0b00000000000001000000110101000000
#define DENALI_CTL_142_DATA 0b00000000000001000000110101000000
#define DENALI_CTL_143_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_144_DATA 0b00000000000000000111000101110011
#define DENALI_CTL_145_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_146_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_147_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_148_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_149_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_150_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_151_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_152_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_153_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_154_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_155_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_156_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_157_DATA 0b00000011000001010000000000000000
#define DENALI_CTL_158_DATA 0b00000100000001100000001100000101
#define DENALI_CTL_159_DATA 0b00001001000000000000000000000000
#define DENALI_CTL_160_DATA 0b00001001000001110000000100001010
#define DENALI_CTL_161_DATA 0b00000000000000000000111000001010
#define DENALI_CTL_162_DATA 0b00000111000000010000101000001001
#define DENALI_CTL_163_DATA 0b00000000000011100000101000001001
#define DENALI_CTL_164_DATA 0b00000001000010100000100100000000
#define DENALI_CTL_165_DATA 0b00001110000010100000100100000111
#define DENALI_CTL_166_DATA 0b00000000000001000000000100101111
#define DENALI_CTL_167_DATA 0b00000000000000000000000000000111
#define DENALI_CTL_168_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_169_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_170_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_171_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_172_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_173_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_174_DATA 0b00000001000000000000000000000000
#define DENALI_CTL_175_DATA 0b00000000000000001100000000000000
#define DENALI_CTL_176_DATA 0b00000000110000001000000000000000
#define DENALI_CTL_177_DATA 0b00000000110000001000000000000000
#define DENALI_CTL_178_DATA 0b00000000000000001000000000000000
#define DENALI_CTL_179_DATA 0b00000000000000000001011100000000
#define DENALI_CTL_180_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_181_DATA 0b00000000000000000000000000000001
#define DENALI_CTL_182_DATA 0b00000000000000000000000000000010
#define DENALI_CTL_183_DATA 0b00000000000000000001000000001110
#define DENALI_CTL_184_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_185_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_186_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_187_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_188_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_189_DATA 0b00000000000001000000000000000000
#define DENALI_CTL_190_DATA 0b00000000000001010000000000000010
#define DENALI_CTL_191_DATA 0b00000000000001010000010000000100
#define DENALI_CTL_192_DATA 0b00000000001010000000000001010000
#define DENALI_CTL_193_DATA 0b00000100000001000000000001100100
#define DENALI_CTL_194_DATA 0b00000000110101100000000001100100
#define DENALI_CTL_195_DATA 0b00000001000010110000000001101011
#define DENALI_CTL_196_DATA 0b00000001000010110000100000001000
#define DENALI_CTL_197_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_198_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_199_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_200_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_201_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_202_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_203_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_204_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_205_DATA 0b00000000000000000000000001100011
#define DENALI_CTL_206_DATA 0b00000000000000000000000000000100
#define DENALI_CTL_207_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_208_DATA 0b00000000000000000000000010100011
#define DENALI_CTL_209_DATA 0b00000000000000000000000000010100
#define DENALI_CTL_210_DATA 0b00000000000000000000000000001001
#define DENALI_CTL_211_DATA 0b00000000000000000000000101000011
#define DENALI_CTL_212_DATA 0b00000000000000000000000000110100
#define DENALI_CTL_213_DATA 0b00000000000000000000000000011011
#define DENALI_CTL_214_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_215_DATA 0b00000000000000000000000000110001
#define DENALI_CTL_216_DATA 0b00000000000000000000000000110001
#define DENALI_CTL_217_DATA 0b00000000000000000000000000110001
#define DENALI_CTL_218_DATA 0b00000000000000000001100000000000
#define DENALI_CTL_219_DATA 0b00000000000000000001100000000000
#define DENALI_CTL_220_DATA 0b00000000000000000001100000000000
#define DENALI_CTL_221_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_222_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_223_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_224_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_225_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_226_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_227_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_228_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_229_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_230_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_231_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_232_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_233_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_234_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_235_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_236_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_237_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_238_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_239_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_240_DATA 0b00000000000000000000000001100011
#define DENALI_CTL_241_DATA 0b00000000000000000000000000000100
#define DENALI_CTL_242_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_243_DATA 0b00000000000000000000000010100011
#define DENALI_CTL_244_DATA 0b00000000000000000000000000010100
#define DENALI_CTL_245_DATA 0b00000000000000000000000000001001
#define DENALI_CTL_246_DATA 0b00000000000000000000000101000011
#define DENALI_CTL_247_DATA 0b00000000000000000000000000110100
#define DENALI_CTL_248_DATA 0b00000000000000000000000000011011
#define DENALI_CTL_249_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_250_DATA 0b00000000000000000000000000110001
#define DENALI_CTL_251_DATA 0b00000000000000000000000000110001
#define DENALI_CTL_252_DATA 0b00000000000000000000000000110001
#define DENALI_CTL_253_DATA 0b00000000000000000001100000000000
#define DENALI_CTL_254_DATA 0b00000000000000000001100000000000
#define DENALI_CTL_255_DATA 0b00000000000000000001100000000000
#define DENALI_CTL_256_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_257_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_258_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_259_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_260_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_261_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_262_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_263_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_264_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_265_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_266_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_267_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_268_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_269_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_270_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_271_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_272_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_273_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_274_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_275_DATA 0b00000000000000000000000000100000
#define DENALI_CTL_276_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_277_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_278_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_279_DATA 0b00000000000000000000000100000000
#define DENALI_CTL_280_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_281_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_282_DATA 0b00000000000000000000000100000001
#define DENALI_CTL_283_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_284_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_285_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_286_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_287_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_288_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_289_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_290_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_291_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_292_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_293_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_294_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_295_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_296_DATA 0b00011010000110100000000100000000
#define DENALI_CTL_297_DATA 0b00000101000000010000000000011001
#define DENALI_CTL_298_DATA 0b00010101000100010000000000000000
#define DENALI_CTL_299_DATA 0b00000000000001000000110000011000
#define DENALI_CTL_300_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_301_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_302_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_303_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_304_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_305_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_306_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_307_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_308_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_309_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_310_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_311_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_312_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_313_DATA 0b00000000000000110000000000000000
#define DENALI_CTL_314_DATA 0b00000001000000000000001000000000
#define DENALI_CTL_315_DATA 0b00000000000101000000000001000000
#define DENALI_CTL_316_DATA 0b00000000000000100000000000000001
#define DENALI_CTL_317_DATA 0b00000000010000000000000100000000
#define DENALI_CTL_318_DATA 0b00000000000011000000000110010000
#define DENALI_CTL_319_DATA 0b00000001000000000000001000000000
#define DENALI_CTL_320_DATA 0b00000100001010110000000001000000
#define DENALI_CTL_321_DATA 0b00000000000000000000000000100000
#define DENALI_CTL_322_DATA 0b00000000000101000000000000000011
#define DENALI_CTL_323_DATA 0b00000001000000000000000000110110
#define DENALI_CTL_324_DATA 0b00000010000000100000000100000001
#define DENALI_CTL_325_DATA 0b00010000000000100000000100000001
#define DENALI_CTL_326_DATA 0b11111111111111110000101100000001
#define DENALI_CTL_327_DATA 0b00000001000000010000000100000001
#define DENALI_CTL_328_DATA 0b00000001000000010000000100000001
#define DENALI_CTL_329_DATA 0b00000001000010110000000100000001
#define DENALI_CTL_330_DATA 0b00001100000000110000000000000000
#define DENALI_CTL_331_DATA 0b00000100000000010000000100000000
#define DENALI_CTL_332_DATA 0b00000001000000010000000000000000
#define DENALI_CTL_333_DATA 0b00000000000000000000000000000100
#define DENALI_CTL_334_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_335_DATA 0b00000011000000110000000100000001
#define DENALI_CTL_336_DATA 0b00000000000000000000000100000011
#define DENALI_CTL_337_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_338_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_339_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_340_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_341_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_342_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_343_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_344_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_345_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_346_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_347_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_348_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_349_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_350_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_351_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_352_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_353_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_354_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_355_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_356_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_357_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_358_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_359_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_360_DATA 0b00000010000000010000000100000000
#define DENALI_CTL_361_DATA 0b00000001000000010000000000000010
#define DENALI_CTL_362_DATA 0b00000000000000010000000100000000
#define DENALI_CTL_363_DATA 0b00000001000000010000000100000001
#define DENALI_CTL_364_DATA 0b00000100000000110000000000000001
#define DENALI_CTL_365_DATA 0b00010001000010000000010100000110
#define DENALI_CTL_366_DATA 0b00000010000010000000100000001000
#define DENALI_CTL_367_DATA 0b00000010000011100000000000001000
#define DENALI_CTL_368_DATA 0b00000010000011110000000000001010
#define DENALI_CTL_369_DATA 0b00000000000001100000100000001001
#define DENALI_CTL_370_DATA 0b00000000000010010000101000001000
#define DENALI_CTL_371_DATA 0b00000010000000010000000100000000
#define DENALI_CTL_372_DATA 0b00000000000000100000010000000001
#define DENALI_CTL_373_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_374_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_375_DATA 0b00001101000000000000000000000001
#define DENALI_CTL_376_DATA 0b00000000000000010000000000101000
#define DENALI_CTL_377_DATA 0b00000000000000010000000000000000
#define DENALI_CTL_378_DATA 0b00000000000000000000000000000011
#define DENALI_CTL_379_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_380_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_381_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_382_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_383_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_384_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_385_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_386_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_387_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_388_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_389_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_390_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_391_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_392_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_393_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_394_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_395_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_396_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_397_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_398_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_399_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_400_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_401_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_402_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_403_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_404_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_405_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_406_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_407_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_408_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_409_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_410_DATA 0b00000001000000000000000000000000
#define DENALI_CTL_411_DATA 0b00000000000000000000000000000001
#define DENALI_CTL_412_DATA 0b00000000000000010000000100000000
#define DENALI_CTL_413_DATA 0b00000011000000110000000000000000
#define DENALI_CTL_414_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_415_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_416_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_417_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_418_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_419_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_420_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_421_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_422_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_423_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_424_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_425_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_426_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_427_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_428_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_429_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_430_DATA 0b00000000000001010101011010101010
#define DENALI_CTL_431_DATA 0b00000000000010101010101010101010
#define DENALI_CTL_432_DATA 0b00000000000010101010100101010101
#define DENALI_CTL_433_DATA 0b00000000000001010101010101010101
#define DENALI_CTL_434_DATA 0b00000000000010110011000100110011
#define DENALI_CTL_435_DATA 0b00000000000001001100110100110011
#define DENALI_CTL_436_DATA 0b00000000000001001100111011001100
#define DENALI_CTL_437_DATA 0b00000000000010110011001011001100
#define DENALI_CTL_438_DATA 0b00000000000000010000001100000000
#define DENALI_CTL_439_DATA 0b00000011000000000000000100000000
#define DENALI_CTL_440_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_441_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_442_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_443_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_444_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_445_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_446_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_447_DATA 0b00000000000000010000000000000000
#define DENALI_CTL_448_DATA 0b01100111011001110000000000000000
#define DENALI_CTL_449_DATA 0b01100111011001110110011101100111
#define DENALI_CTL_450_DATA 0b01100111011001110110011101100111
#define DENALI_CTL_451_DATA 0b01100111011001110110011101100111
#define DENALI_CTL_452_DATA 0b01100111011001110110011101100111
#define DENALI_CTL_453_DATA 0b01100111011001110110011101100111
#define DENALI_CTL_454_DATA 0b00000000000000000110011101100111
#define DENALI_CTL_455_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_456_DATA 0b00001000000010000000000000000000
#define DENALI_CTL_457_DATA 0b00001000000000000000000000000000
#define DENALI_CTL_458_DATA 0b00000000000000000000000000001000
#define DENALI_CTL_459_DATA 0b00000000000000000000100000001000
#define DENALI_CTL_460_DATA 0b00000000000010000000100000000000
#define DENALI_CTL_461_DATA 0b00000000000000010011001000000111
#define DENALI_CTL_462_DATA 0b00110010000000000000000100110010
#define DENALI_CTL_463_DATA 0b00000001001100100000000000000001
#define DENALI_CTL_464_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_465_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_466_DATA 0b00100110000111100001101100000000
#define DENALI_CTL_467_DATA 0b00000000000010100000000000000000
#define DENALI_CTL_468_DATA 0b00000000000000000000000010001100
#define DENALI_CTL_469_DATA 0b00000000000000000000001000000000
#define DENALI_CTL_470_DATA 0b00000000000000000000001000000000
#define DENALI_CTL_471_DATA 0b00000000000000000000001000000000
#define DENALI_CTL_472_DATA 0b00000000000000000000001000000000
#define DENALI_CTL_473_DATA 0b00000000000000000000011010010000
#define DENALI_CTL_474_DATA 0b00000000000000000000010101111000
#define DENALI_CTL_475_DATA 0b00000000000000000000001000000100
#define DENALI_CTL_476_DATA 0b00000000000000000000110000100000
#define DENALI_CTL_477_DATA 0b00000000000000000000001000000000
#define DENALI_CTL_478_DATA 0b00000000000000000000001000000000
#define DENALI_CTL_479_DATA 0b00000000000000000000001000000000
#define DENALI_CTL_480_DATA 0b00000000000000000000001000000000
#define DENALI_CTL_481_DATA 0b00000000000000000010010001100000
#define DENALI_CTL_482_DATA 0b00000000000000000111100101000000
#define DENALI_CTL_483_DATA 0b00000000000000000000010000001000
#define DENALI_CTL_484_DATA 0b00000000000000000010000001101010
#define DENALI_CTL_485_DATA 0b00000000000000000000001000000000
#define DENALI_CTL_486_DATA 0b00000000000000000000001000000000
#define DENALI_CTL_487_DATA 0b00000000000000000000001000000000
#define DENALI_CTL_488_DATA 0b00000000000000000000001000000000
#define DENALI_CTL_489_DATA 0b00000000000000000110000100111110
#define DENALI_CTL_490_DATA 0b00000000000000010100010000100100
#define DENALI_CTL_491_DATA 0b00000010000000100000011000010000
#define DENALI_CTL_492_DATA 0b00000011000000110000001000000010
#define DENALI_CTL_493_DATA 0b00000000000000000000000000100010
#define DENALI_CTL_494_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_495_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_496_DATA 0b00000000000000000001010000000011
#define DENALI_CTL_497_DATA 0b00000000000000000000011111010000
#define DENALI_CTL_498_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_499_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_500_DATA 0b00000000000000110000000000000000
#define DENALI_CTL_501_DATA 0b00000000000001100000000000011110
#define DENALI_CTL_502_DATA 0b00000000000010100000000000100010
#define DENALI_CTL_503_DATA 0b00000000000100010000000000101001
#define DENALI_CTL_504_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_505_DATA 0b00000000000000000000000000000000
#define DENALI_CTL_506_DATA 0b00000010000000000000000000000000
#define DENALI_CTL_507_DATA 0b00000001000000000000010000000010
#define DENALI_CTL_508_DATA 0b00000011000011100000001100000100
#define DENALI_CTL_509_DATA 0b00000001000000010000010100000000
#define DENALI_CTL_510_DATA 0b00000001000000010000000100000000
#define DENALI_CTL_511_DATA 0b00000001000000010000000100000001
#define DENALI_CTL_512_DATA 0b00000001000000000000000100000000
#define DENALI_CTL_513_DATA 0b00000000000000010000000100000000
#define DENALI_CTL_514_DATA 0b00000000000000100000000100000000
#define DENALI_CTL_515_DATA 0b00000001000000000000000000000010
#define DENALI_CTL_516_DATA 0b00000000000000100000000000000010
#define DENALI_CTL_517_DATA 0b00000000000101000000101000000110

#define DDR_CONTROLLER_BASE 0x58020000

static void mmio_wr32(uintptr_t addr, uint32_t value)
{
  *(volatile uint32_t*)addr = value;
}

static uint32_t mmio_rd32(uintptr_t addr)
{
  return *(volatile uint32_t*)addr;
}

static void opdelay(unsigned int times)
{
  while(times--) {
    __asm__ volatile ("nop");
  }
}

static void ddrc_init()
{
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_00_ADDR,DENALI_CTL_00_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_01_ADDR,DENALI_CTL_01_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_02_ADDR,DENALI_CTL_02_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_03_ADDR,DENALI_CTL_03_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_04_ADDR,DENALI_CTL_04_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_05_ADDR,DENALI_CTL_05_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_06_ADDR,DENALI_CTL_06_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_07_ADDR,DENALI_CTL_07_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_08_ADDR,DENALI_CTL_08_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_09_ADDR,DENALI_CTL_09_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_10_ADDR,DENALI_CTL_10_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_11_ADDR,DENALI_CTL_11_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_12_ADDR,DENALI_CTL_12_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_13_ADDR,DENALI_CTL_13_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_14_ADDR,DENALI_CTL_14_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_15_ADDR,DENALI_CTL_15_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_16_ADDR,DENALI_CTL_16_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_17_ADDR,DENALI_CTL_17_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_18_ADDR,DENALI_CTL_18_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_19_ADDR,DENALI_CTL_19_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_20_ADDR,DENALI_CTL_20_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_21_ADDR,DENALI_CTL_21_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_22_ADDR,DENALI_CTL_22_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_23_ADDR,DENALI_CTL_23_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_24_ADDR,DENALI_CTL_24_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_25_ADDR,DENALI_CTL_25_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_26_ADDR,DENALI_CTL_26_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_27_ADDR,DENALI_CTL_27_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_28_ADDR,DENALI_CTL_28_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_29_ADDR,DENALI_CTL_29_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_30_ADDR,DENALI_CTL_30_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_31_ADDR,DENALI_CTL_31_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_32_ADDR,DENALI_CTL_32_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_33_ADDR,DENALI_CTL_33_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_34_ADDR,DENALI_CTL_34_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_35_ADDR,DENALI_CTL_35_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_36_ADDR,DENALI_CTL_36_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_37_ADDR,DENALI_CTL_37_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_38_ADDR,DENALI_CTL_38_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_39_ADDR,DENALI_CTL_39_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_40_ADDR,DENALI_CTL_40_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_41_ADDR,DENALI_CTL_41_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_42_ADDR,DENALI_CTL_42_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_43_ADDR,DENALI_CTL_43_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_44_ADDR,DENALI_CTL_44_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_45_ADDR,DENALI_CTL_45_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_46_ADDR,DENALI_CTL_46_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_47_ADDR,DENALI_CTL_47_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_48_ADDR,DENALI_CTL_48_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_49_ADDR,DENALI_CTL_49_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_50_ADDR,DENALI_CTL_50_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_51_ADDR,DENALI_CTL_51_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_52_ADDR,DENALI_CTL_52_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_53_ADDR,DENALI_CTL_53_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_54_ADDR,DENALI_CTL_54_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_55_ADDR,DENALI_CTL_55_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_56_ADDR,DENALI_CTL_56_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_57_ADDR,DENALI_CTL_57_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_58_ADDR,DENALI_CTL_58_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_59_ADDR,DENALI_CTL_59_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_60_ADDR,DENALI_CTL_60_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_61_ADDR,DENALI_CTL_61_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_62_ADDR,DENALI_CTL_62_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_63_ADDR,DENALI_CTL_63_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_64_ADDR,DENALI_CTL_64_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_65_ADDR,DENALI_CTL_65_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_66_ADDR,DENALI_CTL_66_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_67_ADDR,DENALI_CTL_67_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_68_ADDR,DENALI_CTL_68_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_69_ADDR,DENALI_CTL_69_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_70_ADDR,DENALI_CTL_70_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_71_ADDR,DENALI_CTL_71_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_72_ADDR,DENALI_CTL_72_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_73_ADDR,DENALI_CTL_73_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_74_ADDR,DENALI_CTL_74_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_75_ADDR,DENALI_CTL_75_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_76_ADDR,DENALI_CTL_76_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_77_ADDR,DENALI_CTL_77_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_78_ADDR,DENALI_CTL_78_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_79_ADDR,DENALI_CTL_79_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_80_ADDR,DENALI_CTL_80_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_81_ADDR,DENALI_CTL_81_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_82_ADDR,DENALI_CTL_82_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_83_ADDR,DENALI_CTL_83_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_84_ADDR,DENALI_CTL_84_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_85_ADDR,DENALI_CTL_85_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_86_ADDR,DENALI_CTL_86_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_87_ADDR,DENALI_CTL_87_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_88_ADDR,DENALI_CTL_88_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_89_ADDR,DENALI_CTL_89_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_90_ADDR,DENALI_CTL_90_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_91_ADDR,DENALI_CTL_91_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_92_ADDR,DENALI_CTL_92_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_93_ADDR,DENALI_CTL_93_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_94_ADDR,DENALI_CTL_94_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_95_ADDR,DENALI_CTL_95_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_96_ADDR,DENALI_CTL_96_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_97_ADDR,DENALI_CTL_97_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_98_ADDR,DENALI_CTL_98_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_99_ADDR,DENALI_CTL_99_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_100_ADDR,DENALI_CTL_100_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_101_ADDR,DENALI_CTL_101_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_102_ADDR,DENALI_CTL_102_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_103_ADDR,DENALI_CTL_103_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_104_ADDR,DENALI_CTL_104_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_105_ADDR,DENALI_CTL_105_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_106_ADDR,DENALI_CTL_106_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_107_ADDR,DENALI_CTL_107_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_108_ADDR,DENALI_CTL_108_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_109_ADDR,DENALI_CTL_109_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_110_ADDR,DENALI_CTL_110_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_111_ADDR,DENALI_CTL_111_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_112_ADDR,DENALI_CTL_112_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_113_ADDR,DENALI_CTL_113_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_114_ADDR,DENALI_CTL_114_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_115_ADDR,DENALI_CTL_115_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_116_ADDR,DENALI_CTL_116_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_117_ADDR,DENALI_CTL_117_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_118_ADDR,DENALI_CTL_118_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_119_ADDR,DENALI_CTL_119_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_120_ADDR,DENALI_CTL_120_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_121_ADDR,DENALI_CTL_121_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_122_ADDR,DENALI_CTL_122_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_123_ADDR,DENALI_CTL_123_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_124_ADDR,DENALI_CTL_124_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_125_ADDR,DENALI_CTL_125_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_126_ADDR,DENALI_CTL_126_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_127_ADDR,DENALI_CTL_127_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_128_ADDR,DENALI_CTL_128_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_129_ADDR,DENALI_CTL_129_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_130_ADDR,DENALI_CTL_130_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_131_ADDR,DENALI_CTL_131_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_132_ADDR,DENALI_CTL_132_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_133_ADDR,DENALI_CTL_133_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_134_ADDR,DENALI_CTL_134_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_135_ADDR,DENALI_CTL_135_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_136_ADDR,DENALI_CTL_136_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_137_ADDR,DENALI_CTL_137_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_138_ADDR,DENALI_CTL_138_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_139_ADDR,DENALI_CTL_139_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_140_ADDR,DENALI_CTL_140_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_141_ADDR,DENALI_CTL_141_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_142_ADDR,DENALI_CTL_142_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_143_ADDR,DENALI_CTL_143_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_144_ADDR,DENALI_CTL_144_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_145_ADDR,DENALI_CTL_145_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_146_ADDR,DENALI_CTL_146_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_147_ADDR,DENALI_CTL_147_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_148_ADDR,DENALI_CTL_148_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_149_ADDR,DENALI_CTL_149_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_150_ADDR,DENALI_CTL_150_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_151_ADDR,DENALI_CTL_151_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_152_ADDR,DENALI_CTL_152_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_153_ADDR,DENALI_CTL_153_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_154_ADDR,DENALI_CTL_154_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_155_ADDR,DENALI_CTL_155_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_156_ADDR,DENALI_CTL_156_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_157_ADDR,DENALI_CTL_157_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_158_ADDR,DENALI_CTL_158_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_159_ADDR,DENALI_CTL_159_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_160_ADDR,DENALI_CTL_160_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_161_ADDR,DENALI_CTL_161_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_162_ADDR,DENALI_CTL_162_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_163_ADDR,DENALI_CTL_163_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_164_ADDR,DENALI_CTL_164_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_165_ADDR,DENALI_CTL_165_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_166_ADDR,DENALI_CTL_166_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_167_ADDR,DENALI_CTL_167_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_168_ADDR,DENALI_CTL_168_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_169_ADDR,DENALI_CTL_169_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_170_ADDR,DENALI_CTL_170_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_171_ADDR,DENALI_CTL_171_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_172_ADDR,DENALI_CTL_172_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_173_ADDR,DENALI_CTL_173_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_174_ADDR,DENALI_CTL_174_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_175_ADDR,DENALI_CTL_175_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_176_ADDR,DENALI_CTL_176_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_177_ADDR,DENALI_CTL_177_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_178_ADDR,DENALI_CTL_178_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_179_ADDR,DENALI_CTL_179_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_180_ADDR,DENALI_CTL_180_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_181_ADDR,DENALI_CTL_181_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_182_ADDR,DENALI_CTL_182_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_183_ADDR,DENALI_CTL_183_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_184_ADDR,DENALI_CTL_184_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_185_ADDR,DENALI_CTL_185_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_186_ADDR,DENALI_CTL_186_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_187_ADDR,DENALI_CTL_187_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_188_ADDR,DENALI_CTL_188_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_189_ADDR,DENALI_CTL_189_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_190_ADDR,DENALI_CTL_190_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_191_ADDR,DENALI_CTL_191_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_192_ADDR,DENALI_CTL_192_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_193_ADDR,DENALI_CTL_193_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_194_ADDR,DENALI_CTL_194_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_195_ADDR,DENALI_CTL_195_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_196_ADDR,DENALI_CTL_196_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_197_ADDR,DENALI_CTL_197_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_198_ADDR,DENALI_CTL_198_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_199_ADDR,DENALI_CTL_199_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_200_ADDR,DENALI_CTL_200_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_201_ADDR,DENALI_CTL_201_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_202_ADDR,DENALI_CTL_202_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_203_ADDR,DENALI_CTL_203_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_204_ADDR,DENALI_CTL_204_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_205_ADDR,DENALI_CTL_205_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_206_ADDR,DENALI_CTL_206_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_207_ADDR,DENALI_CTL_207_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_208_ADDR,DENALI_CTL_208_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_209_ADDR,DENALI_CTL_209_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_210_ADDR,DENALI_CTL_210_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_211_ADDR,DENALI_CTL_211_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_212_ADDR,DENALI_CTL_212_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_213_ADDR,DENALI_CTL_213_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_214_ADDR,DENALI_CTL_214_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_215_ADDR,DENALI_CTL_215_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_216_ADDR,DENALI_CTL_216_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_217_ADDR,DENALI_CTL_217_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_218_ADDR,DENALI_CTL_218_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_219_ADDR,DENALI_CTL_219_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_220_ADDR,DENALI_CTL_220_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_221_ADDR,DENALI_CTL_221_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_222_ADDR,DENALI_CTL_222_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_223_ADDR,DENALI_CTL_223_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_224_ADDR,DENALI_CTL_224_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_225_ADDR,DENALI_CTL_225_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_226_ADDR,DENALI_CTL_226_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_227_ADDR,DENALI_CTL_227_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_228_ADDR,DENALI_CTL_228_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_229_ADDR,DENALI_CTL_229_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_230_ADDR,DENALI_CTL_230_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_231_ADDR,DENALI_CTL_231_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_232_ADDR,DENALI_CTL_232_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_233_ADDR,DENALI_CTL_233_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_234_ADDR,DENALI_CTL_234_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_235_ADDR,DENALI_CTL_235_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_236_ADDR,DENALI_CTL_236_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_237_ADDR,DENALI_CTL_237_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_238_ADDR,DENALI_CTL_238_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_239_ADDR,DENALI_CTL_239_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_240_ADDR,DENALI_CTL_240_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_241_ADDR,DENALI_CTL_241_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_242_ADDR,DENALI_CTL_242_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_243_ADDR,DENALI_CTL_243_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_244_ADDR,DENALI_CTL_244_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_245_ADDR,DENALI_CTL_245_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_246_ADDR,DENALI_CTL_246_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_247_ADDR,DENALI_CTL_247_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_248_ADDR,DENALI_CTL_248_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_249_ADDR,DENALI_CTL_249_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_250_ADDR,DENALI_CTL_250_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_251_ADDR,DENALI_CTL_251_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_252_ADDR,DENALI_CTL_252_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_253_ADDR,DENALI_CTL_253_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_254_ADDR,DENALI_CTL_254_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_255_ADDR,DENALI_CTL_255_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_256_ADDR,DENALI_CTL_256_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_257_ADDR,DENALI_CTL_257_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_258_ADDR,DENALI_CTL_258_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_259_ADDR,DENALI_CTL_259_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_260_ADDR,DENALI_CTL_260_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_261_ADDR,DENALI_CTL_261_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_262_ADDR,DENALI_CTL_262_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_263_ADDR,DENALI_CTL_263_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_264_ADDR,DENALI_CTL_264_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_265_ADDR,DENALI_CTL_265_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_266_ADDR,DENALI_CTL_266_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_267_ADDR,DENALI_CTL_267_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_268_ADDR,DENALI_CTL_268_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_269_ADDR,DENALI_CTL_269_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_270_ADDR,DENALI_CTL_270_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_271_ADDR,DENALI_CTL_271_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_272_ADDR,DENALI_CTL_272_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_273_ADDR,DENALI_CTL_273_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_274_ADDR,DENALI_CTL_274_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_275_ADDR,DENALI_CTL_275_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_276_ADDR,DENALI_CTL_276_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_277_ADDR,DENALI_CTL_277_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_278_ADDR,DENALI_CTL_278_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_279_ADDR,DENALI_CTL_279_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_280_ADDR,DENALI_CTL_280_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_281_ADDR,DENALI_CTL_281_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_282_ADDR,DENALI_CTL_282_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_283_ADDR,DENALI_CTL_283_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_284_ADDR,DENALI_CTL_284_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_285_ADDR,DENALI_CTL_285_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_286_ADDR,DENALI_CTL_286_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_287_ADDR,DENALI_CTL_287_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_288_ADDR,DENALI_CTL_288_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_289_ADDR,DENALI_CTL_289_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_290_ADDR,DENALI_CTL_290_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_291_ADDR,DENALI_CTL_291_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_292_ADDR,DENALI_CTL_292_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_293_ADDR,DENALI_CTL_293_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_294_ADDR,DENALI_CTL_294_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_295_ADDR,DENALI_CTL_295_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_296_ADDR,DENALI_CTL_296_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_297_ADDR,DENALI_CTL_297_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_298_ADDR,DENALI_CTL_298_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_299_ADDR,DENALI_CTL_299_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_300_ADDR,DENALI_CTL_300_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_301_ADDR,DENALI_CTL_301_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_302_ADDR,DENALI_CTL_302_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_303_ADDR,DENALI_CTL_303_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_304_ADDR,DENALI_CTL_304_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_305_ADDR,DENALI_CTL_305_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_306_ADDR,DENALI_CTL_306_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_307_ADDR,DENALI_CTL_307_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_308_ADDR,DENALI_CTL_308_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_309_ADDR,DENALI_CTL_309_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_310_ADDR,DENALI_CTL_310_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_311_ADDR,DENALI_CTL_311_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_312_ADDR,DENALI_CTL_312_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_313_ADDR,DENALI_CTL_313_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_314_ADDR,DENALI_CTL_314_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_315_ADDR,DENALI_CTL_315_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_316_ADDR,DENALI_CTL_316_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_317_ADDR,DENALI_CTL_317_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_318_ADDR,DENALI_CTL_318_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_319_ADDR,DENALI_CTL_319_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_320_ADDR,DENALI_CTL_320_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_321_ADDR,DENALI_CTL_321_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_322_ADDR,DENALI_CTL_322_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_323_ADDR,DENALI_CTL_323_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_324_ADDR,DENALI_CTL_324_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_325_ADDR,DENALI_CTL_325_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_326_ADDR,DENALI_CTL_326_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_327_ADDR,DENALI_CTL_327_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_328_ADDR,DENALI_CTL_328_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_329_ADDR,DENALI_CTL_329_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_330_ADDR,DENALI_CTL_330_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_331_ADDR,DENALI_CTL_331_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_332_ADDR,DENALI_CTL_332_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_333_ADDR,DENALI_CTL_333_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_334_ADDR,DENALI_CTL_334_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_335_ADDR,DENALI_CTL_335_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_336_ADDR,DENALI_CTL_336_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_337_ADDR,DENALI_CTL_337_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_338_ADDR,DENALI_CTL_338_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_339_ADDR,DENALI_CTL_339_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_340_ADDR,DENALI_CTL_340_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_341_ADDR,DENALI_CTL_341_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_342_ADDR,DENALI_CTL_342_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_343_ADDR,DENALI_CTL_343_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_344_ADDR,DENALI_CTL_344_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_345_ADDR,DENALI_CTL_345_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_346_ADDR,DENALI_CTL_346_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_347_ADDR,DENALI_CTL_347_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_348_ADDR,DENALI_CTL_348_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_349_ADDR,DENALI_CTL_349_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_350_ADDR,DENALI_CTL_350_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_351_ADDR,DENALI_CTL_351_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_352_ADDR,DENALI_CTL_352_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_353_ADDR,DENALI_CTL_353_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_354_ADDR,DENALI_CTL_354_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_355_ADDR,DENALI_CTL_355_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_356_ADDR,DENALI_CTL_356_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_357_ADDR,DENALI_CTL_357_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_358_ADDR,DENALI_CTL_358_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_359_ADDR,DENALI_CTL_359_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_360_ADDR,DENALI_CTL_360_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_361_ADDR,DENALI_CTL_361_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_362_ADDR,DENALI_CTL_362_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_363_ADDR,DENALI_CTL_363_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_364_ADDR,DENALI_CTL_364_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_365_ADDR,DENALI_CTL_365_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_366_ADDR,DENALI_CTL_366_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_367_ADDR,DENALI_CTL_367_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_368_ADDR,DENALI_CTL_368_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_369_ADDR,DENALI_CTL_369_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_370_ADDR,DENALI_CTL_370_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_371_ADDR,DENALI_CTL_371_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_372_ADDR,DENALI_CTL_372_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_373_ADDR,DENALI_CTL_373_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_374_ADDR,DENALI_CTL_374_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_375_ADDR,DENALI_CTL_375_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_376_ADDR,DENALI_CTL_376_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_377_ADDR,DENALI_CTL_377_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_378_ADDR,DENALI_CTL_378_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_379_ADDR,DENALI_CTL_379_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_380_ADDR,DENALI_CTL_380_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_381_ADDR,DENALI_CTL_381_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_382_ADDR,DENALI_CTL_382_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_383_ADDR,DENALI_CTL_383_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_384_ADDR,DENALI_CTL_384_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_385_ADDR,DENALI_CTL_385_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_386_ADDR,DENALI_CTL_386_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_387_ADDR,DENALI_CTL_387_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_388_ADDR,DENALI_CTL_388_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_389_ADDR,DENALI_CTL_389_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_390_ADDR,DENALI_CTL_390_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_391_ADDR,DENALI_CTL_391_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_392_ADDR,DENALI_CTL_392_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_393_ADDR,DENALI_CTL_393_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_394_ADDR,DENALI_CTL_394_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_395_ADDR,DENALI_CTL_395_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_396_ADDR,DENALI_CTL_396_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_397_ADDR,DENALI_CTL_397_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_398_ADDR,DENALI_CTL_398_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_399_ADDR,DENALI_CTL_399_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_400_ADDR,DENALI_CTL_400_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_401_ADDR,DENALI_CTL_401_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_402_ADDR,DENALI_CTL_402_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_403_ADDR,DENALI_CTL_403_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_404_ADDR,DENALI_CTL_404_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_405_ADDR,DENALI_CTL_405_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_406_ADDR,DENALI_CTL_406_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_407_ADDR,DENALI_CTL_407_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_408_ADDR,DENALI_CTL_408_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_409_ADDR,DENALI_CTL_409_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_410_ADDR,DENALI_CTL_410_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_411_ADDR,DENALI_CTL_411_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_412_ADDR,DENALI_CTL_412_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_413_ADDR,DENALI_CTL_413_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_414_ADDR,DENALI_CTL_414_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_415_ADDR,DENALI_CTL_415_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_416_ADDR,DENALI_CTL_416_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_417_ADDR,DENALI_CTL_417_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_418_ADDR,DENALI_CTL_418_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_419_ADDR,DENALI_CTL_419_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_420_ADDR,DENALI_CTL_420_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_421_ADDR,DENALI_CTL_421_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_422_ADDR,DENALI_CTL_422_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_423_ADDR,DENALI_CTL_423_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_424_ADDR,DENALI_CTL_424_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_425_ADDR,DENALI_CTL_425_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_426_ADDR,DENALI_CTL_426_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_427_ADDR,DENALI_CTL_427_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_428_ADDR,DENALI_CTL_428_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_429_ADDR,DENALI_CTL_429_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_430_ADDR,DENALI_CTL_430_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_431_ADDR,DENALI_CTL_431_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_432_ADDR,DENALI_CTL_432_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_433_ADDR,DENALI_CTL_433_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_434_ADDR,DENALI_CTL_434_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_435_ADDR,DENALI_CTL_435_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_436_ADDR,DENALI_CTL_436_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_437_ADDR,DENALI_CTL_437_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_438_ADDR,DENALI_CTL_438_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_439_ADDR,DENALI_CTL_439_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_440_ADDR,DENALI_CTL_440_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_441_ADDR,DENALI_CTL_441_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_442_ADDR,DENALI_CTL_442_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_443_ADDR,DENALI_CTL_443_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_444_ADDR,DENALI_CTL_444_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_445_ADDR,DENALI_CTL_445_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_446_ADDR,DENALI_CTL_446_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_447_ADDR,DENALI_CTL_447_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_448_ADDR,DENALI_CTL_448_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_449_ADDR,DENALI_CTL_449_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_450_ADDR,DENALI_CTL_450_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_451_ADDR,DENALI_CTL_451_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_452_ADDR,DENALI_CTL_452_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_453_ADDR,DENALI_CTL_453_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_454_ADDR,DENALI_CTL_454_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_455_ADDR,DENALI_CTL_455_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_456_ADDR,DENALI_CTL_456_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_457_ADDR,DENALI_CTL_457_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_458_ADDR,DENALI_CTL_458_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_459_ADDR,DENALI_CTL_459_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_460_ADDR,DENALI_CTL_460_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_461_ADDR,DENALI_CTL_461_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_462_ADDR,DENALI_CTL_462_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_463_ADDR,DENALI_CTL_463_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_464_ADDR,DENALI_CTL_464_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_465_ADDR,DENALI_CTL_465_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_466_ADDR,DENALI_CTL_466_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_467_ADDR,DENALI_CTL_467_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_468_ADDR,DENALI_CTL_468_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_469_ADDR,DENALI_CTL_469_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_470_ADDR,DENALI_CTL_470_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_471_ADDR,DENALI_CTL_471_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_472_ADDR,DENALI_CTL_472_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_473_ADDR,DENALI_CTL_473_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_474_ADDR,DENALI_CTL_474_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_475_ADDR,DENALI_CTL_475_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_476_ADDR,DENALI_CTL_476_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_477_ADDR,DENALI_CTL_477_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_478_ADDR,DENALI_CTL_478_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_479_ADDR,DENALI_CTL_479_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_480_ADDR,DENALI_CTL_480_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_481_ADDR,DENALI_CTL_481_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_482_ADDR,DENALI_CTL_482_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_483_ADDR,DENALI_CTL_483_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_484_ADDR,DENALI_CTL_484_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_485_ADDR,DENALI_CTL_485_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_486_ADDR,DENALI_CTL_486_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_487_ADDR,DENALI_CTL_487_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_488_ADDR,DENALI_CTL_488_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_489_ADDR,DENALI_CTL_489_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_490_ADDR,DENALI_CTL_490_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_491_ADDR,DENALI_CTL_491_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_492_ADDR,DENALI_CTL_492_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_493_ADDR,DENALI_CTL_493_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_494_ADDR,DENALI_CTL_494_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_495_ADDR,DENALI_CTL_495_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_496_ADDR,DENALI_CTL_496_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_497_ADDR,DENALI_CTL_497_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_498_ADDR,DENALI_CTL_498_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_499_ADDR,DENALI_CTL_499_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_500_ADDR,DENALI_CTL_500_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_501_ADDR,DENALI_CTL_501_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_502_ADDR,DENALI_CTL_502_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_503_ADDR,DENALI_CTL_503_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_504_ADDR,DENALI_CTL_504_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_505_ADDR,DENALI_CTL_505_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_506_ADDR,DENALI_CTL_506_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_507_ADDR,DENALI_CTL_507_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_508_ADDR,DENALI_CTL_508_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_509_ADDR,DENALI_CTL_509_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_510_ADDR,DENALI_CTL_510_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_511_ADDR,DENALI_CTL_511_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_512_ADDR,DENALI_CTL_512_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_513_ADDR,DENALI_CTL_513_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_514_ADDR,DENALI_CTL_514_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_515_ADDR,DENALI_CTL_515_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_516_ADDR,DENALI_CTL_516_DATA);
  mmio_wr32(DDR_CONTROLLER_BASE+DENALI_CTL_517_ADDR,DENALI_CTL_517_DATA);
}

static void wait_mc_initialization_complete()
{
  uint32_t read_value;
  read_value = 0x0;
  while(!(read_value & (0x1<<4))){
    read_value=mmio_rd32(DDR_CONTROLLER_BASE+4*INT_STATUS_ADDR);
  }
}

void ddr_init()
{
  mmio_wr32(0x500100f4,0x551102); //ddr0_pll 1066M Hz
  opdelay(100);
  ddrc_init();
  mmio_wr32(DDR_CONTROLLER_BASE+0x538,0x1);
  mmio_wr32(DDR_CONTROLLER_BASE+0x524,0x10b0001);
  mmio_wr32(DDR_CONTROLLER_BASE+0x520,0x10101);
  mmio_wr32(DDR_CONTROLLER_BASE+0x5c,0x2010002);
  mmio_wr32(DDR_CONTROLLER_BASE+0x0,0x20400b01);
  wait_mc_initialization_complete();
}
