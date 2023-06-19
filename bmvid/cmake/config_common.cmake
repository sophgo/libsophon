macro(bm168x_variable)
    message(STATUS "bm168x_variable ${ARGV0}=${ARGV1}")
    add_definitions(-DCONFIG_${ARGV0}=${ARGV1})
endmacro()

bm168x_variable(MAJOR_VERSION 1)
bm168x_variable(MINOR_VERSION 0)
#Initial fill_data for global memory
bm168x_variable(GLOBAL_DATA_INITIAL 0xdeadbeef)
#Initial fill_data for local memory
bm168x_variable(LOCAL_DATA_INITIAL 0xdeadbeef)
bm168x_variable(DDR_BW 64)
bm168x_variable(TIMELINE_GRIDS 128)
bm168x_variable(LOCAL_MEM_LATENCY 128)
bm168x_variable(DDR_LATENCY 64)

if (CHIP_NAME STREQUAL bm1684)
    bm168x_variable(NPU_SHIFT 6)
    bm168x_variable(EU_SHIFT 5)
    bm168x_variable(LOCAL_MEM_ADDRWIDTH 19)
    bm168x_variable(L2_SRAM_SIZE 0x400000)
    bm168x_variable(KERNEL_MEM_SIZE 0)
    bm168x_variable(GLOBAL_MEM_SIZE 0x100000000)
    #This is the size of the global memory which can be allocated to nps, we use this to allocate array of global
    #memory in our c_model, if you are using tv_gen, this size should be as small as poosible (needed in soc version)
    #This is the mem size of host allocated memory in host DDR (needed in soc version)
    bm168x_variable(HOST_REALMEM_SIZE 0x400000)
    #This is the size of the ARM reserved memory in ddr (needed in soc version)
    bm168x_variable(COUNT_RESERVED_DDR_ARM 0x1000000)
elseif (CHIP_NAME STREQUAL bm1686)
    bm168x_variable(NPU_SHIFT 6)
    bm168x_variable(EU_SHIFT 4)
    bm168x_variable(LOCAL_MEM_ADDRWIDTH 19)
    bm168x_variable(L2_SRAM_SIZE 0x1000000)
    bm168x_variable(KERNEL_MEM_SIZE 0x800000)
    bm168x_variable(GLOBAL_MEM_SIZE 0x100000000)
    bm168x_variable(HOST_REALMEM_SIZE 0x400000)
    bm168x_variable(COUNT_RESERVED_DDR_ARM 0x1000000)
endif()

option(USING_CMODEL "USING_CMODEL" OFF)
#fast gen cmd mode
option(FAST_GEN_CMD "FAST_GEN_CMD" ON)
option(DEBUG "DEBUG" OFF)
#If you want to use gdb debug, this should be set to 1 (needed in soc version)
option(ARM_DEBUG "ARM_DEBUG " OFF)
###############################################################
#MODE C"" ONFIGURATION
###############################################################
#Using fullnet mode (needed in soc version, always to be 1 in soc mode)
#only in cmodel and compile backend, USING_FULLNET must not define
option(USING_FULLNET "USING_FULLNET" OFF)
#enable the timer print in fullnet mode
option(ENABLE_TIMER_FOR_FULLNET "ENABLE_TIMER_FOR_FULLNET" ON)

#if enable, compiler will generate each layer result, runtime will compare each layer result
#option(USING_RUNTIME_COMPARE "USING_RUNTIME_COMPARE" OFF)
#if enable, print dynamic compile flow
#option(DYNAMIC_CONTEXT_DEBUG "DYNAMIC_CONTEXT_DEBUG" OFF)

#If this option is opened, the gdma / bdc are running in
#different threads , this can check some errors(such as error in sync id)
option(USING_MULTI_THREAD_ENGINE "USING_MULTI_THREAD_ENGINE" ON)
#BLAS is is used
option(USING_BLAS "USING_BLAS" OFF)
#This option should be opened when you are providing firmware for ASIC simulation
#It will disable printf/malloc and enable fw tracing
option(USING_FW_SIMULATION  "USING_FW_SIMULATION " OFF)
#when this macro is opened, only md scalar and fullnet api is used
option(FW_SIMPLE "FW_SIMPLE" OFF)
#No real atomic operation in cmodel
option(NO_ATOMIC "NO_ATOMIC" OFF)
#Constant table for blas lib
option(USING_INDEX_TABLE "USING_INDEX_TABLE" ON)
#Disable the denorm floating operation (Our asic does not support denorm feature, we should keep the same with it)
option(DISABLE_DENORM "DISABLE_DENORM" ON)
#Eanable this macro to enable the printf operation
option(USING_FW_PRINT "USING_FW_PRINT" ON)
option(USING_FW_DEBUG "USING_FW_DEBUG" OFF)
option(USING_FW_API_PERF "USING_FW_API_PERF" OFF)
#Enable this macro to disable the printf operation in assert(This will save code_size )
option(NO_PRINTF_IN_ASSERT "NO_PRINTF_IN_ASSERT" OFF)
#If using memory pool recording
option(MEMPOOL_RECORD "MEMPOOL_RECORD" ON)
#Enable the macro to use access bdc and gdma by fast reg access
option(FAST_REG_ACCESS "FAST_REG_ACCESS" ON)
#Enable the macro to use access bdc and gdma by fast reg access
option(FAST_GEN_IGNORE_CHECK "FAST_GEN_IGNORE_CHECK" OFF)
###############################################################
#API mode
###############################################################
#When this option is enabled, api is running in an async mode, the api returned immediately after calling
#without done in the chip (needed in soc version)
option(USING_NO_WAIT_API "USING_NO_WAIT_API" ON)
#When this option is opened, interrupt is used instead of polling for CDMA copy (needed in soc version)
option(USING_INT_CDMA "USING_INT_CDMA" OFF)
#When this option is opened, interrupt is used instead of polling for polling empty slot, (needed in soc version)
option(USING_INT_MSGFIFO "USING_INT_MSGFIFO" OFF)
###############################################################
# TV_GEN C"" ONFIGURATION
###############################################################
#Dump test vector
option(TV_GEN "TV_GEN" OFF)
#Dump mask for local mem status
option(TV_GEN_DUMP_MASK "TV_GEN_DUMP_MASK" ON)
#Dump binary mode
option(DUMP_BIN "DUMP_BIN" OFF)
#Do not dump the data file
option(NO_DUMP "NO_DUMP" OFF)
option(NO_INIT_TABLE "NO_INIT_TABLE" OFF)
#Do not dump the gddr.dat
option(NO_GDDR_DUMP "NO_GDDR_DUMP" OFF)
#Do not dump the cddr.dat
option(NO_CDDR_DUMP "NO_CDDR_DUMP" OFF)
option(PRINT_CDMA_TO_STATUS "PRINT_CDMA_TO_STATUS" ON)
#Only dump the difference
option(DIFF_DUMP "DIFF_DUMP" OFF)
#Dump local mem into different sections
option(LOCAL_MEM_DUMP_BY_STEP "LOCAL_MEM_DUMP_BY_STEP" OFF)
#Dump the command buffer
option(TV_GEN_CMD "TV_GEN_CMD" OFF)
#Dump the command buffer
option(SOC_MODE "SOC_MODE" OFF)
#log gdma operation detail
option(TV_GEN_GDMA_LOG "TV_GEN_GDMA_LOG" OFF)
#driver intr mode
option(SYNC_API_INT_MODE "SYNC_API_INT_MODE" ON)
#is support lpddr4x
option(LPDDR4X_SUPPORT "LPDDR4X_SUPPORT" ON)
#Pcie mode enable CPU
option(PCIE_MODE_ENABLE_CPU "PCIE_MODE_ENABLE_CPU" OFF)
#############################################################
# TEST C"" ONFIGURATION
#############################################################
option(FIXED_TEST "FIXED_TEST" OFF)
option(PIO_DESC_INTERLEAVE "PIO_DESC_INTERLEAVE" OFF)
################################################################
#STAS_GEN C"" ONFIGURATION
################################################################
option(STAS_GEN "STAS_GEN" OFF)
option(STA_SHOW_BY_ROW "STA_SHOW_BY_ROW" OFF)
option(BM_STAS_VISUAL "BM_STAS_VISUAL" OFF)
option(BM_HW_RT "BM_HW_RT" OFF)

if(LINUX)
include(./projects/$ENV{TARGET_PROJECT}.cmake)
elseif(WINDOWS)
include(projects/bm1684_x86_pcie_device.cmake)
endif()

if(USING_CMODEL)
 add_definitions(-DUSING_CMODEL)
 if(CHIP_NAME STREQUAL bm1684)
   add_definitions(-DCMODEL_CHIPID=0x1684)
 elseif(CHIP_NAME STREQUAL bm1686)
   add_definitions(-DCMODEL_CHIPID=0x1686)
 endif()
endif()
if(FAST_GEN_CMD)
 add_definitions(-DFAST_GEN_CMD)
endif()
if(DEBUG)
 add_definitions(-DDEBUG)
endif()
if(USING_FULLNET)
 add_definitions(-DUSING_FULLNET)
 add_definitions(-DUSING_MULTI_THREAD_ENGINE)
endif()
if(ENABLE_TIMER_FOR_FULLNET)
 add_definitions(-DENABLE_TIMER_FOR_FULLNET)
endif()
if(USING_MULTI_THREAD_ENGINE)
 add_definitions(-DUSING_MULTI_THREAD_ENGINE)
endif()
if(USING_BLAS)
 add_definitions(-DUSING_BLAS)
endif()
if(USING_FW_SIMULATION)
 add_definitions(-DUSING_FW_SIMULATION)
endif()
if(FW_SIMPLE)
 add_definitions(-DFW_SIMPLE)
endif()
if(NO_ATOMIC)
 add_definitions(-DCONFIG_NO_ATOMIC)
endif()
if(USING_INDEX_TABLE)
 add_definitions(-DUSING_INDEX_TABLE)
endif()
if(DISABLE_DENORM)
 add_definitions(-DDISABLE_DENORM)
endif()
if(USING_FW_PRINT)
 add_definitions(-DUSING_FW_PRINT)
 if(USING_FW_DEBUG)
   add_definitions(-DUSING_FW_DEBUG)
 endif()
endif()
if(USING_FW_API_PERF)
 add_definitions(-DUSING_FW_API_PERF)
endif()
if(NO_PRINTF_IN_ASSERT)
 add_definitions(-DNO_PRINTF_IN_ASSERT)
endif()
if(MEMPOOL_RECORD)
 add_definitions(-DMEMPOOL_RECORD)
endif()
if(FAST_REG_ACCESS)
 add_definitions(-DFAST_REG_ACCESS)
endif()
if(FAST_GEN_IGNORE_CHECK)
 add_definitions(-DFAST_GEN_IGNORE_CHECK)
endif()
if(USING_INT_CDMA)
 add_definitions(-DUSING_INT_CDMA)
endif()
if(USING_INT_MSGFIFO)
  if(NOT TV_GEN)#TV_GEN==0
  add_definitions(-DUSING_INT_MSGFIFO)
  endif()
endif()
if(TV_GEN)
 add_definitions(-DBM_TV_GEN)
endif()
if(USING_NO_WAIT_API)
 if(NOT TV_GEN)#TV_GEN==0
    add_definitions(-DBM_MSG_API_NO_WAIT_RESULT)
 endif()
endif()

if(TV_GEN_DUMP_MASK)
 add_definitions(-DBM_TV_GEN_DUMP_MASK)
endif()
if(DUMP_BIN)
 add_definitions(-DDUMP_BIN)
endif()
if(NO_DUMP)
 add_definitions(-DNO_DUMP)
endif()
if(NO_INIT_TABLE)
 add_definitions(-DNO_INIT_TABLE)
endif()
if(NO_GDDR_DUMP)
 add_definitions(-DNO_GDDR_DUMP)
endif()
if(NO_CDDR_DUMP)
 add_definitions(-DNO_CDDR_DUMP)
endif()
if(PRINT_CDMA_TO_STATUS)
 add_definitions(-DPRINT_CDMA_TO_STATUS)
endif()
if(DIFF_DUMP)
 add_definitions(-DDIFF_DUMP)
endif()
if(LOCAL_MEM_DUMP_BY_STEP)
 add_definitions(-DLOCAL_MEM_DUMP_BY_STEP)
endif()
if(TV_GEN_CMD)
 add_definitions(-DTV_GEN_CMD)
endif()
if(SOC_MODE)
 add_definitions(-DSOC_MODE)
endif()
if(TV_GEN_GDMA_LOG)
 add_definitions(-DTV_GEN_GDMA_LOG)
endif()
if(SYNC_API_INT_MODE)
 add_definitions(-DSYNC_API_INT_MODE)
endif()
if(LPDDR4X_SUPPORT)
 add_definitions(-DLPDDR4X_SUPPORT)
endif()
if(PCIE_MODE_ENABLE_CPU)
 add_definitions(-DPCIE_MODE_ENABLE_CPU)
endif()
if(FIXED_TEST)
 add_definitions(-DFIXED_TEST)
endif()
if(PIO_DESC_INTERLEAVE)
 add_definitions(-DPIO_DESC_INTERLEAVE)
endif()
if(STAS_GEN)
 add_definitions(-DBM_STAS_GEN)
endif()
if(STA_SHOW_BY_ROW)
 add_definitions(-DSTA_SHOW_BY_ROW)
endif()
if(BM_STAS_VISUAL)
 add_definitions(-DBM_STAS_VISUAL)
endif()
if(BM_HW_RT)
 add_definitions(-DBM_HW_RT)
endif()

if(LINUX)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wno-error=deprecated-declarations -fno-strict-aliasing -std=c++11")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wno-error=deprecated-declarations")
endif()