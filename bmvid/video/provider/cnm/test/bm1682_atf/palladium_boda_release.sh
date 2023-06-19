#cp plat/bitmain/bm1682_fvp/include/platform_def.h.palladium plat/bitmain/bm1682_fvp/include/platform_def.h
#cp /home/xun/projects/arm-trusted-firmware/test_case/cnm/vdi/nonos/vdi.c.palladium /home/xun/projects/arm-trusted-firmware/test_case/cnm/vdi/nonos/vdi.c
#cp /home/xun/projects/arm-trusted-firmware/test_case/cnm/vdi/nonos/vdi_osal.c.palladium /home/xun/projects/arm-trusted-firmware/test_case/cnm/vdi/nonos/vdi_osal.c
make clean TEST_APP=boda  PLAT=bm1682_fvp SIM_PLAT=1 RESET_TO_BL31=1 
make all TEST_APP=boda  PLAT=bm1682_fvp SIM_PLAT=1 RESET_TO_BL31=1 
