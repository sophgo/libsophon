cmake_minimum_required(VERSION 3.6)
cmake_policy(SET CMP0048 NEW)

set(SOC_MODE 0)
if("${PLATFORM}" STREQUAL "soc")
    set(SOC_MODE 1)
endif()

set(SYNC_API_INT_MODE 1)
set(FW_SIMPLE 0)
set(PCIE_MODE_ENABLE_CPU 1)
if(${SOC_MODE} EQUAL 1)
    set(TARGET_PROJECT "sg_aarch64_soc_device")
    set(DKMS_NAME "bmtpu")
    set(PCIE_MODE_ENABLE_CPU 0)

    set(DRIVER_BIN_TARGET "${TARGET_PROJECT}/${DKMS_NAME}.ko")
    add_custom_command(OUTPUT ${DRIVER_BIN_TARGET}
                    COMMAND make SOC_MODE=${SOC_MODE}
                            PLATFORM=asic
                            SYNC_API_INT_MODE=${SYNC_API_INT_MODE}
                            TARGET_PROJECT=${TARGET_PROJECT}
                            FW_SIMPLE=${FW_SIMPLE}
                            PCIE_MODE_ENABLE_CPU=${PCIE_MODE_ENABLE_CPU}
                            SOC_LINUX_DIR=${SOC_LINUX_DIR}
                            CLEAN_BUILD=1
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/driver)
else()
    set(TARGET_PROJECT "sg_pcie_device")
    set(DKMS_NAME "bmsophon")
endif()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/debian")
set(DKMS_CONF_TARGET "dkms.conf")
add_custom_command(OUTPUT ${DKMS_CONF_TARGET}
    # setup dkms.conf
    COMMAND rm -f dkms.conf
    COMMAND echo "PACKAGE_NAME=\"${DKMS_NAME}\"" > dkms.conf
    COMMAND echo "PACKAGE_VERSION=${PROJECT_VERSION}" >> dkms.conf
    COMMAND echo "BUILT_MODULE_NAME[0]=\"${DKMS_NAME}\"" >> dkms.conf VERBATIM
    COMMAND echo "MAKE[0]=\"make SOC_MODE=${SOC_MODE}"
                 "PLATFORM=asic"
                 "SYNC_API_INT_MODE=${SYNC_API_INT_MODE}"
                 "TARGET_PROJECT=${TARGET_PROJECT}"
                 "FW_SIMPLE=${FW_SIMPLE}"
                 "PCIE_MODE_ENABLE_CPU=${PCIE_MODE_ENABLE_CPU}"
                 "-C \${kernel_source_dir} M=\${dkms_tree}/\${PACKAGE_NAME}/\${PACKAGE_VERSION}/build \"" >> dkms.conf
    COMMAND echo "CLEAN=\"make -C \${kernel_source_dir} M=\${dkms_tree}/\${PACKAGE_NAME}/\${PACKAGE_VERSION}/build clean\"" >> dkms.conf
    COMMAND echo "DEST_MODULE_NAME[0]=\"${DKMS_NAME}\"" >> dkms.conf
    COMMAND echo "DEST_MODULE_LOCATION[0]=\"/updates\"" >> dkms.conf
    COMMAND echo "AUTOINSTALL=\"yes\"" >> dkms.conf
    # setup DEB scripts
    # post-install
    COMMAND echo "if [ ! -d \"/lib/firmware\" ]; then mkdir -p /lib/firmware; fi" > postinst
    COMMAND echo "ln -s /opt/sophon/driver-${CPACK_PACKAGE_VERSION}/a53lite_pkg.bin /lib/firmware/bm1684x_firmware.bin" >> postinst
    COMMAND echo "ln -s /opt/sophon/driver-${CPACK_PACKAGE_VERSION}/bm1684_ddr.bin_v3.1.2-3dfbe057-221128 /lib/firmware/bm1684_ddr_firmware.bin" >> postinst
    COMMAND echo "ln -s /opt/sophon/driver-${CPACK_PACKAGE_VERSION}/bm1684_tcm.bin_v3.1.2-3dfbe057-221128 /lib/firmware/bm1684_tcm_firmware.bin" >> postinst
    COMMAND echo >> postinst

    COMMAND echo "if [ -e \"/usr/share/misc/pci.ids\" ]; then" >> postinst
    COMMAND echo "used_path=/usr/share/misc/pci.ids" >> postinst
    COMMAND echo "elif [ -e \"/usr/share/hwdata/pci.ids\" ]; then" >> postinst
    COMMAND echo "used_path=/usr/share/hwdata/pci.ids" >> postinst
    COMMAND echo "else [ -e \"/usr/share/lshw/pci.ids\" ];" >> postinst
    COMMAND echo "used_path=/usr/share/lshw/pci.ids" >> postinst
    COMMAND echo "fi" >> postinst
    COMMAND echo "dir_name=$(dirname $used_path)" >> postinst
    COMMAND echo "used_dir=\${dir_name##*/}" >> postinst
    COMMAND echo "sed -i '/Bitmain/d' /usr/share/$used_dir/pci.ids" >> postinst
    COMMAND echo "sed -i '/Sophgo/d' /usr/share/$used_dir/pci.ids" >> postinst
    COMMAND echo "sed -i '/SC3 with BM1682/d' /usr/share/$used_dir/pci.ids" >> postinst
    COMMAND echo "sed -i '/BM1684/d' /usr/share/$used_dir/pci.ids" >> postinst
    COMMAND echo "echo \"1e30	Bitmain Technologies Inc.\" >> /usr/share/$used_dir/pci.ids" >> postinst
    COMMAND echo "echo \"	1682	SC3 with BM1682, Sophon Series Deep Learning Accelerator\" >> /usr/share/$used_dir/pci.ids" >> postinst
    COMMAND echo "echo \"	1684	BM1684, Sophon Series Deep Learning Accelerator\" >> /usr/share/$used_dir/pci.ids" >> postinst
    COMMAND echo "echo \"1f1c  Sophgo Technologies Inc.\" >> /usr/share/$used_dir/pci.ids" >> postinst
    COMMAND echo "echo \"	1686	BM1684X, Sophon Series Deep Learning Accelerator\" >> /usr/share/$used_dir/pci.ids" >> postinst

    COMMAND echo "cp -r /opt/sophon/driver-${CPACK_PACKAGE_VERSION} /usr/src/${DKMS_NAME}-${PROJECT_VERSION}" >> postinst
    COMMAND echo "dkms add -m ${DKMS_NAME} -v ${PROJECT_VERSION}" >> postinst
    COMMAND echo "dkms build -m ${DKMS_NAME} -v ${PROJECT_VERSION} && dkms install -m ${DKMS_NAME} -v ${PROJECT_VERSION} || true" >> postinst
    COMMAND echo "modprobe -r ${DKMS_NAME}" >> postinst
    COMMAND echo "modprobe ${DKMS_NAME}" >> postinst

    COMMAND echo "pcinum_1684=`lspci | grep 1684 -c`" >> postinst
    COMMAND echo "pcinum_1686=`lspci | grep 1686 -c`" >> postinst
    COMMAND echo "pcinum=`expr $pcinum_1684 + $pcinum_1686`" >> postinst
    COMMAND echo "devicenum=`ls /dev/ | grep bm-sophon -c`" >> postinst
    COMMAND echo "if [ \"$pcinum\" -ne \"$devicenum\" ]" >> postinst
    COMMAND echo "then" >> postinst
    COMMAND echo "    echo \"sophon driver install error happened, please see dmesg for more infomation\"" >> postinst
    COMMAND echo "fi" >> postinst
    # pre-remove
    COMMAND echo "modprobe -r ${DKMS_NAME}" > prerm
    COMMAND echo "dkms remove -m ${DKMS_NAME} -v ${PROJECT_VERSION} --all || true" >> prerm
    COMMAND echo "rm -rf /usr/src/${DKMS_NAME}-${PROJECT_VERSION}" >> prerm
    COMMAND echo >> prerm
    COMMAND echo "rm -f /lib/firmware/bm1684x_firmware.bin" >> prerm
    COMMAND echo "rm -f /lib/firmware/bm1684_ddr_firmware.bin" >> prerm
    COMMAND echo "rm -f /lib/firmware/bm1684_tcm_firmware.bin" >> prerm
    COMMAND echo "devicenum=`ls /dev/ | grep bm-sophon -c`" >> prerm
    COMMAND echo "if [ 0 -ne \"$devicenum\" ]" >> prerm
    COMMAND echo "then" >> prerm
    COMMAND echo "    echo \"sophon driver may busy or something wrong, please stop all application that use bmsophon.ko.Then run the command 'sudo rmmod bmsophon.ko' to remove the driver.\"" >> prerm
    COMMAND echo "fi" >> prerm
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/debian
)

# only soc mode driver code and binary will be both released
if(${SOC_MODE} EQUAL 1)
    add_custom_target(driver
         DEPENDS ${DKMS_CONF_TARGET}
         DEPENDS ${DRIVER_BIN_TARGET})
else()
    add_custom_target(driver DEPENDS ${DKMS_CONF_TARGET})
endif()
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/debian/${DKMS_CONF_TARGET}
        DESTINATION ../driver-${CPACK_PACKAGE_VERSION}
        COMPONENT driver)
if(${SOC_MODE} EQUAL 1)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${DRIVER_BIN_TARGET}
        DESTINATION ./data
        COMPONENT libsophon)
endif()
file(GLOB firmware_files ${CMAKE_SOURCE_DIR}/driver/bm1684*.bin_*)
foreach(file ${firmware_files})
         install(FILES ${file}
                 DESTINATION ./data
                 COMPONENT libsophon)
endforeach()

install(FILES ${CMAKE_SOURCE_DIR}/driver/a53lite_pkg.bin DESTINATION ./data COMPONENT libsophon)

# copy driver code
file(GLOB driver_files ${CMAKE_SOURCE_DIR}/driver/*)
foreach(file ${driver_files})
    if(IS_DIRECTORY  ${file})
         install(DIRECTORY ${file}
                 DESTINATION ../driver-${CPACK_PACKAGE_VERSION}
                 COMPONENT driver)
    else()
        install(FILES ${file}
                 DESTINATION ../driver-${CPACK_PACKAGE_VERSION}
                 COMPONENT driver)
    endif()
endforeach()

