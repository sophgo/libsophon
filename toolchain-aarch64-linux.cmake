# usage
# cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-arm-linux.cmake ..

# The Generic system name is used for embedded targets (targets without OS) in
# CMake
set( CMAKE_SYSTEM_NAME          Linux )
set( CMAKE_SYSTEM_PROCESSOR     aarch64 )

# The toolchain prefix for all toolchain executables
set( CROSS_COMPILE ${CROSS_COMPILE_PATH}/bin/aarch64-linux-gnu- )
set( ARCH arm64 )

# specify the cross compiler. We force the compiler so that CMake doesn't
# attempt to build a simple test program as this will fail without us using
# the -nostartfiles option on the command line
set(CMAKE_C_COMPILER ${CROSS_COMPILE}gcc)
set(CMAKE_CXX_COMPILER ${CROSS_COMPILE}g++)
