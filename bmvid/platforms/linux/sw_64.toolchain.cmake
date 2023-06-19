set(CMAKE_SYSTEM_PROCESSOR sw_64)
set(GCC_COMPILER_VERSION "" CACHE STRING "GCC Compiler version")
set(GNU_MACHINE "sw_64-sunway-linux-gnu" CACHE STRING "GNU compiler triple")

include("${CMAKE_CURRENT_LIST_DIR}/sw.toolchain.cmake")
message(STATUS "####### debug cmake #######")
