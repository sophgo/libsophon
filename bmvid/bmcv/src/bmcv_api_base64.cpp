#include <stdint.h>
#include <stdio.h>
#include <sys/types.h> // windows is same
#include <fcntl.h>
#include <stdlib.h>
#include "bmcv_api_ext.h"
#include "bmcv_api.h"
#include "bmcv_internal.h"
#include "bmcv_common_bm1684.h"
#include "bmlib_interface.h"

#ifdef __linux__
  #include <unistd.h>
  #include <sys/ioctl.h>
  #include <asm/ioctl.h>
#else
  #include <windows.h>
#endif

#define S2D 0
#define D2S 1
#define MAX_LEN 178956972  //128M/3*4
#define MAX_LOOP_LEN 3145728 //3M
#define BASE64_ENC 1
#define BASE64_DEC 0

#ifndef SOC_MODE
struct ce_base {
    unsigned long long int     src;
    unsigned long long int     dst;
    unsigned long long int     len;
    #ifdef __linux__
    bool direction;
    #else
    int direction;
    #endif
};
#endif


static unsigned long int base64_compute_dstlen(unsigned long int len, bool enc) {
    unsigned long roundup;

    if (len)
    // encode
    if (enc)
        return (len + 2) /
                3 * 4;
    // decode
    if (len == 0)
        return 0;
    roundup = len / 4 * 3;
    return roundup;
}

#ifndef USING_CMODEL



bm_status_t bmcv_base64_enc(bm_handle_t handle, bm_device_mem_t src,
    bm_device_mem_t dst, unsigned long len[2])
{
    len[1] = base64_compute_dstlen(len[0], BASE64_ENC);
    return bmcv_base64_codec(handle, src, dst, len[0], 1);
}
bm_status_t bmcv_base64_dec(bm_handle_t handle, bm_device_mem_t src,
    bm_device_mem_t dst, unsigned long len[2])
{
    int cnt = 0;
    int i;
    unsigned char *src_addr;
    unsigned char check_buf[2];
    bm_device_mem_t src_device;

    src_addr = (unsigned char *)bm_mem_get_system_addr(src);
    len[1] = base64_compute_dstlen(len[0], BASE64_DEC);
    if (bm_mem_get_type(src) == BM_MEM_TYPE_SYSTEM) {
        for (i = 1; i < 3; i++) {
            if(src_addr[len[0] - i] == '=')
                cnt++;
        }
    }

    if (bm_mem_get_type(src) == BM_MEM_TYPE_DEVICE) {
        src_device = src;
        src_device.u.device.device_addr 
            = src.u.device.device_addr + len[0] - 2;
        src_device.size = 2;
        if (BM_SUCCESS !=bm_memcpy_d2s(handle,
            (void *)check_buf,
            src_device)) {
            BMCV_ERR_LOG("bm_memcpy_d2s when check len error\r\n");
            return BM_ERR_FAILURE;
        }
        for (i = 0; i < 2; i++) {
            if(check_buf[i] == '=')
                cnt++;
        }
    }
    len[1] = len[1] - cnt;
    return bmcv_base64_codec(handle, src, dst, len[0], 0);
}
bm_status_t bmcv_base64_codec(bm_handle_t handle, bm_device_mem_t src,
    bm_device_mem_t dst, unsigned long len, bool direction) {

    if (handle == NULL) {
        bmlib_log("BASE64", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_FAILURE;
    }
    struct ce_base base;
    int fd;
    int ret;
    unsigned long loop_len;
    unsigned long long src_addr; //naive addr of src as src is a struct
    unsigned long long dst_addr;
    bm_device_mem_t src_buf_device;
    bm_device_mem_t dst_buf_device;
#ifndef SOC_MODE
  #ifdef __linux__
    bm_get_handle_fd(handle, DEV_FD, &fd);
  #endif
#else
  #ifdef __linux__
    bm_get_handle_fd(handle, SPACC_FD, &fd);
  #endif
#endif

    src_addr = 0;
    dst_addr = 0;
    ret = 0;
    if (fd < 0) {
        perror("open");
        return BM_ERR_FAILURE;
    }

    if (len > MAX_LEN) {
        printf("base64 lenth should be less than 128M!\n");
        return BM_NOT_SUPPORTED;
    }

    if (bm_mem_get_type(src) == BM_MEM_TYPE_SYSTEM)
        src_addr = (unsigned long long)bm_mem_get_system_addr(src);
    if (bm_mem_get_type(dst) == BM_MEM_TYPE_SYSTEM)
        dst_addr = (unsigned long long)bm_mem_get_system_addr(dst);
    if ((bm_mem_get_type(src) == BM_MEM_TYPE_DEVICE) &&
        len > MAX_LOOP_LEN) {
        printf("len of device_mem should be less than 3M!\n");
        return BM_NOT_SUPPORTED;
    }
   
    while (len > 0) {
        if (len > MAX_LOOP_LEN) {
            loop_len = MAX_LOOP_LEN;
            len = len - MAX_LOOP_LEN;
        } else {
            loop_len = len;
            len = 0;
        }
        if (bm_mem_get_type(src) == BM_MEM_TYPE_SYSTEM) {
            if(BM_SUCCESS !=
                bm_malloc_device_byte(handle,
                                      &src_buf_device,
                                      loop_len)) {
                BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

                return BM_ERR_FAILURE;
            }
            if(BM_SUCCESS !=
                bm_memcpy_s2d(handle,
                              src_buf_device,
                              (void *)src_addr)) {
                BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

                if(bm_mem_get_type(src) == BM_MEM_TYPE_SYSTEM){
                     bm_free_device(handle, src_buf_device);
                     return BM_ERR_FAILURE;
                }

            }
        } else {
            src_buf_device = src;
        }
        if (bm_mem_get_type(dst) == BM_MEM_TYPE_SYSTEM) {
            if(BM_SUCCESS !=bm_malloc_device_byte(
                handle, &dst_buf_device, base64_compute_dstlen(loop_len, direction))) {
                BMCV_ERR_LOG("bm_malloc_device_byte error\r\n");

                if(bm_mem_get_type(src) == BM_MEM_TYPE_SYSTEM){
                     bm_free_device(handle, src_buf_device);
                }
                return BM_ERR_FAILURE;

            }
        /* system to device for destination device ? */
        /*
            if(BM_SUCCESS !=
                bm_memcpy_s2d(handle,
                              dst_buf_device,
                              (void *)dst_addr)) {
                BMCV_ERR_LOG("bm_memcpy_s2d error\r\n");

                if (bm_mem_get_type(dst) == BM_MEM_TYPE_SYSTEM){
                     bm_free_device(handle, dst);
                     return BM_ERR_FAILURE;
                }

            }*/
        } else {
            dst_buf_device = dst;
        }  

#ifndef SOC_MODE
        base.src = (unsigned long long)bm_mem_get_device_addr(src_buf_device);
        base.dst = (unsigned long long)bm_mem_get_device_addr(dst_buf_device);
        base.len = loop_len;
        base.direction = direction;
        #ifdef __linux__
        ret = ioctl(fd, BMDEV_BASE64_CODEC, &base);
        // ret = ioctl(fd, BMDEV_BASE64_START, &base);
        #else
        ret = platform_ioctl(handle, BMDEV_BASE64_CODEC, &base);
        #endif
#else
        base.alg = CE_BASE_IOC_ALG_BASE64;
        base.enc = (u32)direction;
        base.src = (u64)bm_mem_get_device_addr(src_buf_device);
        base.dst = (u64)bm_mem_get_device_addr(dst_buf_device);
        base.len = (u64)loop_len;
        base.dstlen = (u64)base64_compute_dstlen((unsigned long)loop_len, direction);

        ret = ioctl(fd, CE_BASE_IOC_OP_PHY, &base);
#endif
        if (ret < 0) {
            printf("ioctl failed!\n");
            return BM_ERR_FAILURE;
        }

        if (bm_mem_get_type(dst) == BM_MEM_TYPE_SYSTEM) {
            if (BM_SUCCESS != bm_memcpy_d2s(handle,
                                   (void *)dst_addr,
                                   dst_buf_device)) {
                BMCV_ERR_LOG("bm_memcpy_d2s error\r\n");

                if (bm_mem_get_type(src) == BM_MEM_TYPE_SYSTEM) {
                    bm_free_device(handle, src_buf_device);
                }
                if (bm_mem_get_type(dst) == BM_MEM_TYPE_SYSTEM) {
                    bm_free_device(handle, dst_buf_device);
                }
                return BM_ERR_FAILURE;
            }
            bm_free_device(handle, dst_buf_device);
        }
        if (bm_mem_get_type(src) == BM_MEM_TYPE_SYSTEM) {
            bm_free_device(handle, src_buf_device);
        }

        src_addr = src_addr + (unsigned long long)loop_len;
        dst_addr = dst_addr + (unsigned long long)base64_compute_dstlen
                   ((unsigned long)loop_len, direction);
    }

    return BM_SUCCESS;
}
#endif
