# dump kernel_module.so to kernel_module.h
set(KERNEL_MODULE_PATH_1684X "${PROJECT_SOURCE_DIR}/lib/libbm1684x_kernel_module.so")
set(KERNEL_MODULE_PATH_TPULV60 "${PROJECT_SOURCE_DIR}/lib/libtpulv60_kernel_module.so")
set(KERNEL_MODULE_PATH_MARS3 "${PROJECT_SOURCE_DIR}/lib/libmars3_kernel_module.so")
set(KERNEL_HEADER_FILE "${CMAKE_BINARY_DIR}/kernel_module.h")
add_custom_command(
    OUTPUT ${KERNEL_HEADER_FILE}
    COMMAND echo "" > ${KERNEL_HEADER_FILE}
    COMMAND echo "static const unsigned char kernel_module_data_1684x[] = {" >> ${KERNEL_HEADER_FILE}
    COMMAND hexdump -v -e '8/1 \"0x%02x,\" \"\\n\"' ${KERNEL_MODULE_PATH_1684X} >> ${KERNEL_HEADER_FILE}
    COMMAND echo "}\;" >> ${KERNEL_HEADER_FILE}

    COMMAND echo "static const unsigned char kernel_module_data_tpulv60[] = {" >> ${KERNEL_HEADER_FILE}
    COMMAND hexdump -v -e '8/1 \"0x%02x,\" \"\\n\"' ${KERNEL_MODULE_PATH_TPULV60} >> ${KERNEL_HEADER_FILE}
    COMMAND echo "}\;" >> ${KERNEL_HEADER_FILE}

    COMMAND echo "static const unsigned char kernel_module_data_mars3[] = {" >> ${KERNEL_HEADER_FILE}
    COMMAND hexdump -v -e '8/1 \"0x%02x,\" \"\\n\"' ${KERNEL_MODULE_PATH_MARS3} >> ${KERNEL_HEADER_FILE}
    COMMAND echo "}\;" >> ${KERNEL_HEADER_FILE}
    )
