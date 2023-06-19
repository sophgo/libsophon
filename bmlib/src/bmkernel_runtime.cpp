#include "bmlib_runtime.h"
#include "bmlib_internal.h"
#include "bmlib_log.h"
#include "api.h"
#include <string.h>

#define ITCM_FW_SIZE 512 * 1024
#define DDR_FW_SIZE 16 * 1024 * 1024
#define MAGIC_NUM_DDR_FW 0xaabbccdd
#define BMLIB_OKKERNEL_LOG_TAG "okkernel_runtime"
#define BM_API_BMKERNEL                   0x3f995ec4
#define BM_API_BMKERNEL_PLUS              0x3f995233
#define BM_API_BMKERNEL_PLUS_1684X        0xffffffe
#define BM_API_QUIT                       0xffffffff
#define L2_SRAM_START_ADDR                0x10000000
#define L2_STATIC_END_OFFSET              0x10000000 // to modify

bm_status_t bmkernel_init(bm_handle_t *handle, int dev_id) {
  bm_status_t ret;
  ret = bm_dev_request(handle, dev_id);
  if (BM_SUCCESS != ret) {
    bmlib_log(BMLIB_OKKERNEL_LOG_TAG, BMLIB_LOG_ERROR,
          "request dev%d failed\n", 0);
    return BM_ERR_BUSY;
  }
  return BM_SUCCESS;
}

void bmkernel_deinit(bm_handle_t handle) {
  bm_dev_free(handle);
}

bm_status_t bm_load_firmware(bm_handle_t handle, const char *firmware_tcm,
                             const char *firmware_ddr) {
    return okkernel_load_firmware(handle,firmware_tcm,firmware_ddr);
}

bm_status_t okkernel_load_firmware(bm_handle_t handle, const char *firmware_tcm,
                                   const char *firmware_ddr) {
#if USING_CMODEL
    UNUSED(handle);
    UNUSED(firmware_tcm);
    UNUSED(firmware_ddr);
    return BM_SUCCESS;
#else
    if (handle->misc_info.chipid != 0x1686) {
        if (firmware_tcm == nullptr) {
            bmlib_log(BMLIB_OKKERNEL_LOG_TAG, BMLIB_LOG_ERROR,
                    "okkernel_load_firmware firmware_tcm is null \n");
            return BM_ERR_FAILURE;
        }
    }
    bm_fw_desc fw;
    FILE * pdl_f;
    char *itcm_fw_arr = NULL;
    char *ddr_fw_arr = NULL;
    bm_status_t ret = BM_SUCCESS;
    int itcm_fw_size = 0;  // bytes
    int ddr_fw_size = 0;  // bytes
    memset(&fw, 0, sizeof(fw));
    /* read ddr img */
    if (firmware_ddr) {
        if ((pdl_f = fopen(firmware_ddr, "rb")) == NULL) {
            bmlib_log(BMLIB_OKKERNEL_LOG_TAG, BMLIB_LOG_ERROR,
                      "%s can not be opened\n", firmware_ddr);
            ret = BM_ERR_FAILURE;
            goto out;
        }
        fseek(pdl_f, 0, SEEK_END);
        ddr_fw_size = ftell(pdl_f);
        fseek(pdl_f, 0, SEEK_SET);
        ddr_fw_arr = new char[ddr_fw_size];
        if (!ddr_fw_arr) {
            ret = BM_ERR_FAILURE;
            goto out;
        }
        if (0 == fread(ddr_fw_arr, 1, ddr_fw_size, pdl_f))
            bmlib_log(BMLIB_OKKERNEL_LOG_TAG, BMLIB_LOG_ERROR,
                      "read zero bytes from %s\n", firmware_ddr);
        fclose(pdl_f);
    }
    /* read tcm img */
    if (handle->misc_info.chipid != 0x1686) {
        if ((pdl_f = fopen(firmware_tcm, "rb")) == NULL) {
            bmlib_log(BMLIB_OKKERNEL_LOG_TAG, BMLIB_LOG_ERROR,
                    "%s can not be opened\n", firmware_tcm);
            ret = BM_ERR_FAILURE;
            goto out;
        }
        fseek(pdl_f, 0, SEEK_END);
        itcm_fw_size = ftell(pdl_f);
        fseek(pdl_f, 0, SEEK_SET);
        itcm_fw_arr = new char[itcm_fw_size];
        if (!itcm_fw_arr) {
            ret = BM_ERR_FAILURE;
            goto out;
        }
        if (0 == fread(itcm_fw_arr, 1, itcm_fw_size, pdl_f)) {
            fclose(pdl_f);
            ret = BM_ERR_FAILURE;
            bmlib_log(BMLIB_OKKERNEL_LOG_TAG, BMLIB_LOG_ERROR,
                    "read zero bytes from %s\n", firmware_tcm);
            goto out;
        }
        fclose(pdl_f);
    }
    fw.itcm_fw = (unsigned int*)itcm_fw_arr;
    fw.itcmfw_size = itcm_fw_size;
    fw.ddr_fw = (unsigned int*)ddr_fw_arr;
    fw.ddrfw_size = ddr_fw_size;
    bmlib_log(BMLIB_OKKERNEL_LOG_TAG, BMLIB_LOG_INFO,
              "firmware tcm img size is %d bytes + ddr img size is %d bytes\n",
              itcm_fw_size, ddr_fw_size);
    ret = bm_update_firmware_a9(handle, &fw);
out:
    if (ddr_fw_arr) delete[] ddr_fw_arr;
    if (itcm_fw_arr) delete[] itcm_fw_arr;
    return ret;
#endif
}

bm_status_t bmkernel_launch(bm_handle_t handle, const void *args,
                            unsigned int size) {
    bm_status_t ret = BM_SUCCESS;
    ret = bm_send_api(handle, (sglib_api_id_t)BM_API_BMKERNEL,
                      reinterpret_cast < const u8 *>(args),
                      nullptr == args ? 0 : size);
    return ret;
}

bm_status_t okkernel_launch_async(bm_handle_t handle, const char *func_name, const void *args,
                                  unsigned int size) {
    static const int MAX_FUNC_NAME_LENGTH = 64;
    if (strlen(func_name) > MAX_FUNC_NAME_LENGTH) {
        bmlib_log(BMLIB_OKKERNEL_LOG_TAG, BMLIB_LOG_ERROR,
                  "Length of the function name %s is greater than %d\n", MAX_FUNC_NAME_LENGTH);
        return BM_ERR_PARAM;
    }
    bm_status_t ret = BM_SUCCESS;
    char *buf = new char[size + MAX_FUNC_NAME_LENGTH + 4];
    strcpy(buf, func_name);
    memcpy(buf + MAX_FUNC_NAME_LENGTH + 4, args, size);
    ret = bm_send_api(handle, (sglib_api_id_t)BM_API_BMKERNEL_PLUS,
                      reinterpret_cast< const u8 *>(buf),
                      MAX_FUNC_NAME_LENGTH + 4 + (nullptr == args ? 0 : size));
    delete [] buf;
    return ret;
}

bm_status_t okkernel_launch_sync(bm_handle_t handle, const char *func_name, const void *args,
                                 unsigned int size) {
    static const int MAX_FUNC_NAME_LENGTH = 64;
    unsigned int chip_id = 0x0;
    if (strlen(func_name) > MAX_FUNC_NAME_LENGTH) {
        bmlib_log(BMLIB_OKKERNEL_LOG_TAG, BMLIB_LOG_ERROR,
                  "Length of the function name %s is greater than %d\n", MAX_FUNC_NAME_LENGTH);
        return BM_ERR_PARAM;
    }
    bm_status_t ret = BM_SUCCESS;
    char *buf = new char[size + MAX_FUNC_NAME_LENGTH + 4];
    strcpy(buf, func_name);
    memcpy(buf + MAX_FUNC_NAME_LENGTH + 4, args, size);
    bm_get_chipid(handle, &chip_id);
    if (chip_id == 0x1686) {
      ret = bm_send_api(handle, (sglib_api_id_t)BM_API_BMKERNEL_PLUS_1684X,
                      reinterpret_cast< const u8 *>(buf),
                      MAX_FUNC_NAME_LENGTH + 4 + (nullptr == args ? 0 : size));
    } else {
      ret = bm_send_api(handle, (sglib_api_id_t)BM_API_BMKERNEL_PLUS,
                      reinterpret_cast< const u8 *>(buf),
                      MAX_FUNC_NAME_LENGTH + 4 + (nullptr == args ? 0 : size));
    }
    delete [] buf;
    if (ret != BM_SUCCESS)
        return ret;
    ret = bm_handle_sync(handle);
    return ret;
}

bm_status_t tpu_kernel_launch_sync(bm_handle_t handle, const char *func_name, const void *args,
                                 unsigned int size) {
    static const int MAX_FUNC_NAME_LENGTH = 64;
    unsigned int chip_id = 0x0;
    if (strlen(func_name) > MAX_FUNC_NAME_LENGTH) {
        bmlib_log("tpu_kernel", BMLIB_LOG_ERROR,
                  "Length of the function name %s is greater than %d\n", MAX_FUNC_NAME_LENGTH);
        return BM_ERR_PARAM;
    }
    bm_status_t ret = BM_SUCCESS;
    char *buf = new char[size + MAX_FUNC_NAME_LENGTH + 4];
    strcpy(buf, func_name);
    memcpy(buf + MAX_FUNC_NAME_LENGTH + 4, args, size);
    bm_get_chipid(handle, &chip_id);
    if(chip_id == 0x1686) {
        ret = bm_send_api(handle, (sglib_api_id_t)BM_API_BMKERNEL_PLUS_1684X,
                      reinterpret_cast< const u8 *>(buf),
                      MAX_FUNC_NAME_LENGTH + 4 + (nullptr == args ? 0 : size));
    } else {
        ret = bm_send_api(handle, (sglib_api_id_t)BM_API_BMKERNEL_PLUS,
                      reinterpret_cast< const u8 *>(buf),
                      MAX_FUNC_NAME_LENGTH + 4 + (nullptr == args ? 0 : size));
    }

    delete [] buf;
    if (ret != BM_SUCCESS)
        return ret;
    ret = bm_handle_sync(handle);
    return ret;
}

bm_status_t okkernel_sync(bm_handle_t handle) {
    return bm_handle_sync(handle);
}

bm_status_t bmkernel_load_lookup_table(
    bm_handle_t handle,
    const void* table,
    unsigned int size) {
    bm_device_mem_t dev_mem = bm_mem_from_device(L2_SRAM_START_ADDR + L2_STATIC_END_OFFSET, size);
    return bm_memcpy_s2d(handle, dev_mem, (void*)table);
}

