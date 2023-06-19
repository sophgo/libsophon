
function(ADD_TARGET_VPU_DRIVER target_name chip_name platform subtype debug ion component out_abs_path)

    if(NOT ${platform} STREQUAL "soc")
        message(WARNING "build vpu_driver only at soc mode")
    else()
        set(VPU_DRIVER_TARGET ${out_abs_path}/vpu.ko ${out_abs_path}/load.sh ${out_abs_path}/unload.sh)
        if(${chip_name} STREQUAL "bm1684")
            set(VPU_FW_LIST chagall_dec.bin chagall.bin)
        elseif(${chip_name} STREQUAL "bm1682")
            set(VPU_FW_LIST  coda960.out picasso.bin)
        elseif(${chip_name} STREQUAL "bm1880")
            set(VPU_FW_LIST coda960.out)
        endif()
        foreach(_f ${VPU_FW_LIST})
            list(APPEND VPU_FW_TARGET ${out_abs_path}/${_f})
        endforeach()

        set(DUMMY_LIST *.o *~ core .depend .*.cmd *.mod.c .tmp_versions modules.order)
        foreach(_f ${DUMMY_LIST})
            list(APPEND DUMMY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/video/driver/linux/${_f})
        endforeach()
        add_custom_command(OUTPUT ${VPU_DRIVER_TARGET} ${VPU_FW_TARGET}
            COMMAND rm -rf ${DUMMY_FILE}
            COMMAND make KERNELDIR=${KERNEL_PATH}
                CHIP=${chip_name}
                SUBTYPE=${subtype}
                DEBUG=${debug}
                ION=${ion}
                ARCH=arm64
                BMVID_TOP_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                CROSS_COMPILE=${CROSS_COMPILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${out_abs_path}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different vpu.ko load.sh unload.sh ${out_abs_path}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${VPU_FW_LIST} ${out_abs_path}
            COMMAND rm -rf ${DUMMY_FILE}
            BYPRODUCTS ${CMAKE_CURRENT_SOURCE_DIR}/video/driver/linux/vpu.ko
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/video/driver/linux
        )

        add_custom_target (${target_name} DEPENDS ${VPU_DRIVER_TARGET} ${VPU_FW_TARGET})
        install(FILES ${VPU_DRIVER_TARGET}
            DESTINATION data
            COMPONENT ${component})
    endif()

endfunction(ADD_TARGET_VPU_DRIVER)

function(ADD_TARGET_JPU_DRIVER target_name chip_name platform subtype debug ion component out_abs_path)

    if (NOT ${platform} STREQUAL "soc")
        message(WARNING "build jpu_driver only at soc mode")
    else()
        set(JPU_DRIVER_TARGET ${out_abs_path}/jpu.ko ${out_abs_path}/load_jpu.sh ${out_abs_path}/unload_jpu.sh)

        set(DUMMY_LIST *.o *~ core .depend .*.cmd *.mod.c .tmp_versions modules.order)
        foreach(_f ${DUMMY_LIST})
            list(APPEND DUMMY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/jpeg/driver/jdi/linux/driver/${_f})
        endforeach()

        add_custom_command(OUTPUT ${JPU_DRIVER_TARGET}
            COMMAND rm -rf ${DUMMY_FILE}
            COMMAND make KERNELDIR=${KERNEL_PATH}
                CHIP=${chip_name}
                SUBTYPE=${subtype}
                DEBUG=${debug}
                ION=${ion}
                ARCH=arm64
                BMVID_TOP_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                CROSS_COMPILE=${CROSS_COMPILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${out_abs_path}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different jpu.ko load_jpu.sh unload_jpu.sh ${out_abs_path}
            COMMAND rm -rf ${DUMMY_FILE}
            BYPRODUCTS ${CMAKE_CURRENT_SOURCE_DIR}/jpeg/driver/jdi/linux/driver/jpu.ko
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/jpeg/driver/jdi/linux/driver
        )

        add_custom_target (${target_name} DEPENDS ${JPU_DRIVER_TARGET})
        install(FILES ${JPU_DRIVER_TARGET}
            DESTINATION data
            COMPONENT ${component})
    endif()

endfunction(ADD_TARGET_JPU_DRIVER)

function(ADD_TARGET_ION_LIB target_name chip_name platform subtype debug ion component out_abs_path)
    set(ION_LIB_TARGET ${out_abs_path}/lib/libbmion.so)
    set(ION_HEADER_TARGET ${out_abs_path}/include)
    set(ION_APP_TARGET ${out_abs_path}/bin)

    add_custom_command(OUTPUT ${ION_LIB_TARGET}
        COMMAND make clean CHIP=${chip_name} PRODUCTFORM=${platform}
        COMMAND make CHIP=${chip_name}
            DEBUG=${debug}
            ION=${ion}
            PRODUCTFORM=${platform}
            BMVID_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            INSTALL_DIR=${out_abs_path}
        COMMAND make install
            CHIP=${chip_name}
            DEBUG=${debug}
            ION=${ion}
            PRODUCTFORM=${platform}
            BMVID_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            INSTALL_DIR=${out_abs_path}
        COMMAND make clean CHIP=${chip_name} PRODUCTFORM=${platform}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/allocator/ion)

    add_custom_target (${target_name} ALL
        DEPENDS ${ION_LIB_TARGET})
    get_filename_component(ION_LIB_FILENAME ${ION_LIB_TARGET} NAME)
    install(DIRECTORY ${out_abs_path}/lib/
        DESTINATION lib
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        COMPONENT ${component}
        FILES_MATCHING
        PATTERN ${ION_LIB_FILENAME}*)
    install(DIRECTORY ${ION_HEADER_TARGET}/
        DESTINATION include
        COMPONENT ${component}-dev)
    install(DIRECTORY ${ION_APP_TARGET}/
        DESTINATION bin
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        COMPONENT ${component})

endfunction(ADD_TARGET_ION_LIB)

function(ADD_TARGET_JPU_LIB target_name chip_name platform subtype debug ion component out_abs_path ion_abs_path)

    set(JPULITE_LIB_TARGET ${out_abs_path}/lib/libbmjpulite.so)
    set(JPUAPI_LIB_TARGET ${out_abs_path}/lib/libbmjpuapi.so)
    set(JPU_HEADER_TARGET ${out_abs_path}/include)
    set(JPU_APP_TARGET ${out_abs_path}/bin)

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/jpeg/driver/bmjpulite)
        add_custom_command(OUTPUT ${JPULITE_LIB_TARGET}
            COMMAND make clean CHIP=${chip_name} PRODUCTFORM=${platform}
            COMMAND ${CMAKE_COMMAND} -E chdir .. ./update_version.sh
            COMMAND make CHIP=${chip_name}
                DEBUG=${debug}
                ION=${ion}
                PRODUCTFORM=${platform}
                BMVID_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
                INSTALL_DIR=${out_abs_path}
            COMMAND make install
                CHIP=${chip_name}
                DEBUG=${debug}
                ION=${ion}
                PRODUCTFORM=${platform}
                BMVID_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
                INSTALL_DIR=${out_abs_path}
            COMMAND make clean CHIP=${chip_name} PRODUCTFORM=${platform}
            COMMAND ${CMAKE_COMMAND} -E chdir .. git checkout -- include/version.h
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/jpeg/driver/bmjpulite)
        add_custom_command(OUTPUT ${JPUAPI_LIB_TARGET}
            COMMAND make clean CHIP=${chip_name} PRODUCTFORM=${platform}
            COMMAND ${CMAKE_COMMAND} -E chdir .. ./update_version.sh
            COMMAND make CHIP=${chip_name}
                DEBUG=${debug}
                ION=${ion}
                ION_DIR=${ion_abs_path}
                PRODUCTFORM=${platform}
                BMVID_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
                INSTALL_DIR=${out_abs_path}
            COMMAND make install
                CHIP=${chip_name}
                DEBUG=${debug}
                ION=${ion}
                ION_DIR=${ion_abs_path}
                PRODUCTFORM=${platform}
                BMVID_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
                INSTALL_DIR=${out_abs_path}
            COMMAND make clean CHIP=${chip_name} PRODUCTFORM=${platform}
            COMMAND ${CMAKE_COMMAND} -E chdir .. git checkout -- include/version.h
            DEPENDS ${JPULITE_LIB_TARGET}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/jpeg/driver/bmjpuapi)

    else()
        file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/jpeg/binary/${platform}/lib/libbmjpulite.so DESTINATION ${out_abs_path}/lib/ FOLLOW_SYMLINK_CHAIN)
        file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/jpeg/binary/${platform}/lib/libbmjpuapi.so DESTINATION ${out_abs_path}/lib/ FOLLOW_SYMLINK_CHAIN)
        foreach(file_i ${CMAKE_CURRENT_SOURCE_DIR}/jpeg/binary/${platform}/include/)
            file(COPY ${file_i} DESTINATION ${out_abs_path}/include/)
        endforeach( file_i )
        foreach(file_i ${CMAKE_CURRENT_SOURCE_DIR}/jpeg/binary/${platform}/bin/)
            file(COPY ${file_i} DESTINATION ${out_abs_path}/bin/)
        endforeach( file_i )
    endif()

    add_custom_target (${target_name} ALL DEPENDS ${JPULITE_LIB_TARGET} ${JPUAPI_LIB_TARGET})

    get_filename_component(JPUAPI_LIB_FILENAME ${JPUAPI_LIB_TARGET} NAME)
    get_filename_component(JPULITE_LIB_FILENAME ${JPULITE_LIB_TARGET} NAME)
    install(DIRECTORY ${out_abs_path}/lib/
        DESTINATION lib
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        COMPONENT ${component}
        FILES_MATCHING
        PATTERN ${JPUAPI_LIB_FILENAME}*
        PATTERN ${JPULITE_LIB_FILENAME}*)
    install(DIRECTORY ${JPU_HEADER_TARGET}/
        DESTINATION include
        COMPONENT ${component}-dev)
    install(DIRECTORY ${JPU_APP_TARGET}/
        DESTINATION bin
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        COMPONENT ${component})

endfunction(ADD_TARGET_JPU_LIB)

function(ADD_TARGET_VPU_LIB target_name chip_name platform subtype debug ion component out_abs_path ion_abs_path)

    set(VPUDEC_LIB_TARGET ${out_abs_path}/lib/libbmvideo.so)
    set(VPUENC_LITE_LIB_TARGET ${out_abs_path}/lib/libbmvpulite.so)
    set(VPUENC_API_LIB_TARGET  ${out_abs_path}/lib/libbmvpuapi.so)
    set(VPUENC_APP_TARGET ${out_abs_path}/bin/w5_enc_test)
    set(VPU_HEADER_TARGET ${out_abs_path}/include)
    set(VPU_APP_TARGET  ${out_abs_path}/bin)

    if(${chip_name} STREQUAL "bm1684")
        set(VPU_DEC_FW_LIST chagall_dec.bin)
        set(VPU_ENC_FW_LIST chagall.bin)
    elseif(${chip_name} STREQUAL "bm1682")
        set(VPU_DEC_FW_LIST  coda960.out picasso.bin)
    elseif(${chip_name} STREQUAL "bm1880")
        set(VPU_DEC_FW_LIST coda960.out)
    endif()
    foreach(_f ${VPU_DEC_FW_LIST})
        list(APPEND VPU_DEC_FW_TARGET ${out_abs_path}/${_f})
    endforeach()
    foreach(_f ${VPU_ENC_FW_LIST})
        list(APPEND VPU_ENC_FW_TARGET ${out_abs_path}/${_f})
    endforeach()

    set(TARGET_DEPEND_OBJ ${VPUDEC_LIB_TARGET} ${VPU_DEC_FW_TARGET})

    if(${chip_name} STREQUAL "bm1684")
        set(PRODUCT wave)
    else()
        set(PRODUCT boda)
    endif()

    set(MAKE_OPT CHIP=${chip_name}
                PRODUCTFORM=${platform}
                SUBTYPE=${subtype}
                ION=${ion}
                BMVID_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
                DEBUG=${debug})

    add_custom_command(OUTPUT ${VPUDEC_LIB_TARGET} ${VPU_DEC_FW_TARGET}
        COMMAND make clean
            PRODUCT=${PRODUCT}
            OBJDIR=obj/${platform}_${chip_name}_${subtype}
            BMVID_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            INSTALL_DIR=release/${platform}_${chip_name}_${subtype}
        COMMAND make ${MAKE_OPT}
            INSTALL_DIR=release/${platform}_${chip_name}_${subtype}
            ION_DIR=${ion_abs_path}
            OBJDIR=obj/${platform}_${chip_name}_${subtype}
            PRODUCT=${PRODUCT}
            OS=linux
            BUILD_ORG_TEST=no
        COMMAND make install-lib
            PRODUCT=${PRODUCT}
            INSTALL_DIR=release/${platform}_${chip_name}_${subtype}
            OBJDIR=obj/${platform}_${chip_name}_${subtype}
            BMVID_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            DESTDIR=${out_abs_path}
        COMMAND make install-bin
            PRODUCT=${PRODUCT}
            INSTALL_DIR=release/${platform}_${chip_name}_${subtype}
            OBJDIR=obj/${platform}_${chip_name}_${subtype}
            DESTDIR=${out_abs_path}/bin/
        COMMAND make clean
            PRODUCT=${PRODUCT}
            OBJDIR=obj/${platform}_${chip_name}_${subtype}
            BMVID_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            INSTALL_DIR=release/${platform}_${chip_name}_${subtype}
        COMMAND ${CMAKE_COMMAND} -E chdir ../../driver/linux cp -f
            ${VPU_DEC_FW_LIST} ${out_abs_path}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/video/provider/cnm)


    get_filename_component(VPUDEC_LIB_FILENAME ${VPUDEC_LIB_TARGET} NAME)
    install(DIRECTORY ${out_abs_path}/lib/
        DESTINATION lib
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        COMPONENT ${component}
        FILES_MATCHING
        PATTERN ${VPUDEC_LIB_FILENAME}*
        PATTERN libvideo_bm.so)
    install(FILES ${VPU_DEC_FW_TARGET}
        DESTINATION lib/vpu_firmware
        COMPONENT ${component})
    install(DIRECTORY ${VPU_HEADER_TARGET}/
        DESTINATION include
        COMPONENT ${component}-dev)
    install(DIRECTORY ${VPU_APP_TARGET}/
        DESTINATION bin
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        COMPONENT ${component})

    if(${chip_name} STREQUAL "bm1684")

        add_custom_command(OUTPUT ${VPUENC_LITE_LIB_TARGET}
            COMMAND make clean ${MAKE_OPT}
            COMMAND make ${MAKE_OPT} PRO=1 INSTALL_DIR=${out_abs_path}
                ION_DIR=${ion_abs_path}
            COMMAND make install ${MAKE_OPT} INSTALL_DIR=${out_abs_path}
            COMMAND make clean ${MAKE_OPT}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/video/provider/cnm/encoder)
        add_custom_command(OUTPUT ${VPUENC_API_LIB_TARGET} ${VPU_ENC_FW_TARGET}
            COMMAND make clean ${MAKE_OPT}
            COMMAND make ${MAKE_OPT} INSTALL_DIR=${out_abs_path}
                ION_DIR=${ion_abs_path}
            COMMAND make install ${MAKE_OPT} INSTALL_DIR=${out_abs_path}
            COMMAND make clean ${MAKE_OPT}
            COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR}/video/driver/linux cp -f
                ${VPU_ENC_FW_LIST} ${out_abs_path}
            DEPENDS ${VPUENC_LITE_LIB_TARGET}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/video/encoder/bm_enc_api)

        list(APPEND TARGET_DEPEND_OBJ ${VPUENC_LITE_LIB_TARGET} ${VPUENC_API_LIB_TARGET} ${VPU_ENC_FW_TARGET})
        get_filename_component(VPUENC_LITE_LIB_FILENAME ${VPUENC_LITE_LIB_TARGET} NAME)
        get_filename_component(VPUENC_API_LIB_FILENAME ${VPUENC_API_LIB_TARGET} NAME)
        install(DIRECTORY ${out_abs_path}/lib/
            DESTINATION lib
            FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
            COMPONENT ${component}
            FILES_MATCHING
            PATTERN ${VPUENC_LITE_LIB_FILENAME}*
            PATTERN ${VPUENC_API_LIB_FILENAME}*)
        install(FILES ${VPU_ENC_FW_TARGET}
            DESTINATION lib/vpu_firmware
            COMPONENT ${component})

        if(${platform} STREQUAL "soc")

            add_custom_command(OUTPUT ${VPUENC_APP_TARGET}
                COMMAND make -f Makefile.wave521c clean ${MAKE_OPT}
                COMMAND make -f Makefile.wave521c ${MAKE_OPT} INSTALL_DIR=${out_abs_path}
                    ION_DIR=${ion_abs_path}
                COMMAND make -f Makefile.wave521c install ${MAKE_OPT} INSTALL_DIR=${out_abs_path}
                COMMAND make -f Makefile.wave521c clean ${MAKE_OPT}
                DEPENDS ${VPUENC_API_LIB_TARGET} ${VPUENC_LITE_LIB_TARGET}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/video/provider/cnm)

            list(APPEND TARGET_DEPEND_OBJ ${VPUENC_APP_TARGET})
            install(FILES ${VPUENC_APP_TARGET}
                DESTINATION bin
                PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
                COMPONENT ${component})

        endif()

    endif()

    add_custom_target (${target_name} ALL DEPENDS ${TARGET_DEPEND_OBJ})

endfunction(ADD_TARGET_VPU_LIB)

function(ADD_TARGET_YUV_LIB target_name platform debug component yuv_abs_path)

    set(YUV_LIB_TARGET ${yuv_abs_path}/lib/libyuv.so)
    set(YUV_ALIB_TARGET ${yuv_abs_path}/lib/libyuv.a)
    set(YUV_HEADER_TARGET ${yuv_abs_path}/include)
    set(YUV_BIN_TARGET ${yuv_abs_path}/bin/yuvconvert)

    if (${platform} STREQUAL "soc" OR ${platform} STREQUAL "pcie_arm64")
        set(ENABLE_NEON ON CACHE BOOL "libyuv internal neon option" FORCE)
    elseif(${platform} STREQUAL "pcie")
        set(ENABLE_NEON OFF CACHE BOOL "libyuv internal neon option" FORCE)
    else()
        message(WARNING "libyuv is used on x86 and arm64 platform")
    endif()

    if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        if (${platform} STREQUAL "pcie")
            set(CMAKE_TOOLCHAIN_FILE ../x86_toolchain/x86_64.toolchain.cmake)
        elseif(${platform} STREQUAL "pcie_mips64")
            set(CMAKE_TOOLCHAIN_FILE ../mips_toolchain/mips_64.toolchain.cmake)
        elseif(${platform} STREQUAL "pcie_sw64")
            set(CMAKE_TOOLCHAIN_FILE ../sunway_toolchain/sw_64.toolchain.cmake)
        elseif(${platform} STREQUAL "pcie_loongarch64")
            set(CMAKE_TOOLCHAIN_FILE ../loongarch_toolchain/loongarch64.toolchain.cmake)
        else()
            set(CMAKE_TOOLCHAIN_FILE ../toolchain/aarch64-gnu.toolchain.cmake)
        endif()
    endif()

    set(TEST_OPT OFF CACHE BOOL "libyuv cached TEST option" FORCE)
    set(CMAKE_MAKE_PROGRAM make)

    set(OPTION_LIST CMAKE_BUILD_TYPE ENABLE_NEON TEST_OPT CMAKE_TOOLCHAIN_FILE CMAKE_MAKE_PROGRAM CROSS_COMPILE_PATH)
    foreach (opt in ${OPTION_LIST})
        if (DEFINED ${opt})
            list(APPEND CMAKE_OPTION -D${opt}=${${opt}})
        endif()
    endforeach()

    add_custom_command(OUTPUT ${YUV_LIB_TARGET} ${YUV_ALIB_TARGET} ${YUV_HEADER_TARGET} ${YUV_BIN_TARGET}
        COMMAND ${CMAKE_COMMAND} -E make_directory ./build
        COMMAND rm -rf ./build/*
        COMMAND ${CMAKE_COMMAND} -E chdir ./build ${CMAKE_COMMAND} ${CMAKE_OPTION} -DCMAKE_INSTALL_PREFIX=${yuv_abs_path} ..
        COMMAND ${CMAKE_COMMAND} -E chdir ./build make install -j`nproc`
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ./build/yuvconvert ${yuv_abs_path}/bin/yuvconvert
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libyuv)

    add_custom_target(${target_name} ALL DEPENDS ${YUV_LIB_TARGET} ${YUV_ALIB_TARGET} ${YUV_HEADER_TARGET} ${YUV_BIN_TARGET})
    set_property(TARGET ${target_name}
                APPEND
                PROPERTY ADDITIONAL_CLEAN_FILES ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libyuv/build)

    get_filename_component(YUV_LIB_FILENAME ${YUV_LIB_TARGET} NAME)
    install(DIRECTORY ${yuv_abs_path}/lib/
        DESTINATION lib
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        COMPONENT ${component}
        FILES_MATCHING
        PATTERN ${YUV_LIB_FILENAME}*)
    install(FILES ${YUV_BIN_TARGET}
        DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        COMPONENT ${component})

    install(FILES ${YUV_ALIB_TARGET}
        DESTINATION lib
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        COMPONENT ${component}-dev)
    install(DIRECTORY ${YUV_HEADER_TARGET}/ DESTINATION include COMPONENT ${component}-dev)


endfunction(ADD_TARGET_YUV_LIB)

function(ADD_TARGET_VPP_LIB target_name chip_name platform subtype debug ion component out_abs_path ion_abs_path)

    set(VPP_LIB_TARGET ${out_abs_path}/lib/libvpp.so)
    set(VPP_ALIB_TARGET ${out_abs_path}/lib/libvpp.a)
    set(VPP_HEADER_TARGET ${out_abs_path}/include)
    set(VPP_APP_TARGET ${out_abs_path}/bin)
    set(VPP_LIB2_TARGET ${out_abs_path}/lib/libbmvppapi.so)
    set(TARGET_DEPEND_OBJ ${VPP_LIB_TARGET} ${VPP_ALIB_TARGET} ${VPP_APP_TARGET}/vpp_resize)
    set(VPP_LIBS_TARGET ${VPP_LIB_TARGET})

    if (${platform} STREQUAL "pcie_mips64")
        set(CONFIG mipsLinux)
    elseif(${platform} STREQUAL "pcie_sw64")
        set(CONFIG sunwayLinux)
    elseif(${platform} STREQUAL "pcie_loongarch64")
        set(CONFIG loongLinux)
    elseif(${platform} STREQUAL "soc")
        set(CONFIG EmbeddedLinux)
    elseif(${platform} STREQUAL "pcie_arm64")
        set(CONFIG arm64Linux)
    else()
        set(CONFIG x86Linux)
    endif()

    if (${chip_name} STREQUAL "bm1684")
        list(APPEND TARGET_DEPEND_OBJ ${VPP_LIB2_TARGET})
        list(APPEND VPP_LIBS_TARGET ${VPP_LIB2_TARGET})
    endif()

    set(MAKE_OPT CHIP=${chip_name}
                SUBTYPE=${subtype}
                BMVID_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
                DEBUG=${debug})
    add_custom_command(OUTPUT ${VPP_LIB_TARGET} ${VPP_ALIB_TARGET}
        COMMAND make -f makefile_lib.mak clean
            BMVID_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            OBJDIR=obj/${platform}_${chip_name}_${subtype}
        COMMAND make -f makefile_lib.mak ${MAKE_OPT}
            OBJDIR=obj/${platform}_${chip_name}_${subtype}
            BUILD_CONFIGURATION=${CONFIG}
        COMMAND make -f makefile_lib.mak install ${MAKE_OPT}
            OBJDIR=obj/${platform}_${chip_name}_${subtype}
            DESTDIR=${out_abs_path}
        COMMAND make -f makefile_lib.mak clean
            BMVID_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            OBJDIR=obj/${platform}_${chip_name}_${subtype}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/vpp/driver/)

    add_custom_command(OUTPUT ${VPP_LIB2_TARGET}
        COMMAND make clean ${MAKE_OPT}
           PRODUCTFORM=${platform}
        COMMAND make ${MAKE_OPT}
           PRODUCTFORM=${platform}
           BMCV_PATH=${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libbmcv
           ION_DIR=${ion_abs_path}
           INSTALL_DIR=${out_abs_path}
        COMMAND make install ${MAKE_OPT}
           PRODUCTFORM=${platform}
           INSTALL_DIR=${out_abs_path}
        COMMAND make clean ${MAKE_OPT}
           PRODUCTFORM=${platform}
        DEPENDS ${VPP_LIB_TARGET} ${VPP_ALIB_TARGET}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/vpp/bmvppapi/)

    add_custom_command(OUTPUT ${VPP_APP_TARGET}/vpp_resize
        COMMAND make -f Makefile clean ${MAKE_OPT}
            OBJDIR=obj/${platform}_${chip_name}_${subtype}
            INSTALL_DIR=release/${platform}_${chip_name}_${subtype}
            productform=${platform}
        COMMAND make -f Makefile ${MAKE_OPT}
            OBJDIR=obj/${platform}_${chip_name}_${subtype}
            INSTALL_DIR=release/${platform}_${chip_name}_${subtype}
            VPP_LIB_DIR=${out_abs_path}/
            BMCV_PATH=${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libbmcv
            productform=${platform}
        COMMAND make -f Makefile install ${MAKE_OPT}
            OBJDIR=obj/${platform}_${chip_name}_${subtype}
            INSTALL_DIR=release/${platform}_${chip_name}_${subtype}
            VPP_LIB_DIR=${out_abs_path}/
            DESTDIR=${out_abs_path}/bin
            productform=${platform}
        COMMAND make -f Makefile clean ${MAKE_OPT}
            OBJDIR=obj/${platform}_${chip_name}_${subtype}
            INSTALL_DIR=release/${platform}_${chip_name}_${subtype}
            productform=${platform}
        DEPENDS ${VPP_LIBS_TARGET} ${VPP_ALIB_TARGET}

        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/vpp/test/)

    add_custom_target (${target_name} ALL
        DEPENDS ${TARGET_DEPEND_OBJ})
    foreach(f ${VPP_LIBS_TARGET})
        get_filename_component(f_name ${f} NAME)
        SET(MATCH_VPP_LIBS ${MATCH_VPP_LIBS} PATTERN ${f_name}*)
    endforeach()

    install(DIRECTORY ${out_abs_path}/lib/
        DESTINATION lib
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        COMPONENT ${component}
        FILES_MATCHING ${MATCH_VPP_LIBS})
    install(DIRECTORY ${VPP_APP_TARGET}/
        DESTINATION bin
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        COMPONENT ${component})

    install(FILES ${VPP_ALIB_TARGET}
        DESTINATION lib
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        COMPONENT ${component}-dev)
    install(DIRECTORY ${VPP_HEADER_TARGET}/
        DESTINATION include
        COMPONENT ${component}-dev)

endfunction(ADD_TARGET_VPP_LIB)

function(ADD_TARGET_BMCV_LIB target_name chip_name platform subtype debug ion component out_abs_path jpu_abs_path vpp_abs_path ion_abs_path)
    if (NOT ${chip_name} STREQUAL "bm1684")
        message(WARNING "bmcv is unavailable for bm1682")
    else()
        set(BMCV_LIB_TARGET ${out_abs_path}/lib/libbmcv.so)
        set(BMCV_TPU_MODULE_TARGET ${out_abs_path}/lib/tpu_module/libbm1684x_kernel_module.so)
        set(BMCV_ALIB_TARGET ${out_abs_path}/lib/libbmcv.a)
        set(BMCV_HEADER_TARGET ${out_abs_path}/include)
        set(BMCV_APP_TARGET ${out_abs_path}/bin)
        set(BMCV_CPU_LIB_TARGET ${out_abs_path}/lib/libbmcv_cpu_func.so)

        SET(BMCV_LIBS_TARGET ${BMCV_LIB_TARGET})
        if(NOT ${platform} STREQUAL "soc")
            list(APPEND BMCV_LIBS_TARGET ${BMCV_CPU_LIB_TARGET})

        endif()

        set(MAKE_OPT CHIP=${chip_name}
                PRODUCTFORM=${platform}
                SUBTYPE=${subtype}
                DEBUG=${debug}
                BMVID_ROOT=${CMAKE_CURRENT_SOURCE_DIR})
        if(${subtype} STREQUAL "cmodel")
            set(MAKE_OPT ${MAKE_OPT} USING_CMODEL=1)
        endif()

        find_program(BMCPU_CROSS_COMPILE aarch64-linux-gnu-g++)
        if(BMCPU_CROSS_COMPILE STREQUAL "BMCPU_CROSS_COMPILE-NOTFOUND")
            message(ERROR "Add aarch64 linux toolchain to PATH! BMCV function on bmcpu requires it.")
        endif()

        add_custom_command(OUTPUT ${BMCV_LIBS_TARGET} ${BMCV_ALIB_TARGET}
            COMMAND make clean ${MAKE_OPT}
                BMCPU_CROSS_COMPILE=${BMCPU_CROSS_COMPILE}
                OUT_DIR=./release
            COMMAND make bmcv ${MAKE_OPT}
                BMCPU_CROSS_COMPILE=${BMCPU_CROSS_COMPILE}
                JPU_DIR=${jpu_abs_path}
                VPP_DIR=${vpp_abs_path}
                OUT_DIR=./release
            COMMAND cp -aprf ./release/bmcv/* ${out_abs_path}
            COMMAND make clean ${MAKE_OPT}
                BMCPU_CROSS_COMPILE=${BMCPU_CROSS_COMPILE}
                OUT_DIR=./release
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bmcv)

        add_custom_command(OUTPUT ${BMCV_APP_TARGET}/test_cv_vpp
            COMMAND make clean_test ${MAKE_OPT}
                BMCPU_CROSS_COMPILE=${BMCPU_CROSS_COMPILE}
                OUT_DIR=./release
            COMMAND make test_bmcv ${MAKE_OPT}
                BMCPU_CROSS_COMPILE=${BMCPU_CROSS_COMPILE}
                JPU_DIR=${jpu_abs_path}
                VPP_DIR=${vpp_abs_path}
                BMCV_DIR=${out_abs_path}
                BMION_DIR=${ion_abs_path}
                OUT_DIR=./release -j4
            COMMAND cp -apf ./release/bmcv/test/* ${out_abs_path}/bin
            COMMAND make clean_test ${MAKE_OPT}
                BMCPU_CROSS_COMPILE=${BMCPU_CROSS_COMPILE}
                OUT_DIR=./release
            DEPENDS ${BMCV_LIBS_TARGET} ${BMCV_ALIB_TARGET}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bmcv)

        add_custom_target (${target_name} ALL
            DEPENDS ${BMCV_LIBS_TARGET} ${BMCV_ALIB_TARGET} ${BMCV_APP_TARGET}/test_cv_vpp)
        get_filename_component(BMCV_LIB_FILENAME ${BMCV_LIB_TARGET} NAME)
        install(DIRECTORY ${out_abs_path}/lib/
            DESTINATION lib
            FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
            COMPONENT ${component}
            FILES_MATCHING
            PATTERN ${BMCV_LIB_FILENAME}*)
        if(NOT ${platform} STREQUAL "soc")
            get_filename_component(BMCV_CPU_LIB_FILENAME ${BMCV_CPU_LIB_TARGET} NAME)
            install(DIRECTORY ${out_abs_path}/lib/
                DESTINATION lib/bmcpu_bmcv
                FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
                COMPONENT ${component}
                FILES_MATCHING
                PATTERN ${BMCV_CPU_LIB_FILENAME}*)
        endif()
        install(FILES ${BMCV_ALIB_TARGET}
            DESTINATION lib
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
            COMPONENT ${component}-dev)
        install(FILES ${BMCV_TPU_MODULE_TARGET}
            DESTINATION lib/tpu_module/
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
            COMPONENT ${component})
        install(DIRECTORY ${BMCV_HEADER_TARGET}/
            DESTINATION include
            COMPONENT ${component}-dev)
        install(FILES ${BMCV_APP_TARGET}/test_cv_vpp
            DESTINATION bin
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
            COMPONENT ${component})
        install(DIRECTORY ${BMCV_APP_TARGET}/
            DESTINATION bin
            FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
            COMPONENT ${component})

    endif()

endfunction(ADD_TARGET_BMCV_LIB)

function(ADD_TARGET_BMVID_DOC target_name)
    set(BMCV_DOC_TARGET_ZH ${CMAKE_BINARY_DIR}/doc/BMCV开发参考手册.pdf)
    set(BMCV_DOC_TARGET_EN ${CMAKE_BINARY_DIR}/doc/BMCV_Technical_Reference_Manual.pdf)

    add_custom_command(OUTPUT ${BMCV_DOC_TARGET_EN} ${BMCV_DOC_TARGET_ZH}
        COMMAND ${CMAKE_COMMAND} -E remove_directory ./build
        COMMAND make html LANG=en BUILD_RELEASE_VERSION="${CPACK_PACKAGE_VERSION}"
        COMMAND make pdf LANG=en BUILD_RELEASE_VERSION="${CPACK_PACKAGE_VERSION}"
        COMMAND make html LANG=zh BUILD_RELEASE_VERSION="${CPACK_PACKAGE_VERSION}"
        COMMAND make pdf LANG=zh BUILD_RELEASE_VERSION="${CPACK_PACKAGE_VERSION}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ./build/BMCV_Technical_Reference_Manual_en.pdf ${BMCV_DOC_TARGET_EN}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ./build/BMCV_Technical_Reference_Manual_zh.pdf ${BMCV_DOC_TARGET_ZH}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/document/bmcv)

    add_custom_target (${target_name}
        DEPENDS ${BMCV_DOC_TARGET_ZH}  #do not compile BMCV_APP_TARGET/test_cv_vpp now
        DEPENDS ${BMCV_DOC_TARGET_EN})

endfunction(ADD_TARGET_BMVID_DOC)
