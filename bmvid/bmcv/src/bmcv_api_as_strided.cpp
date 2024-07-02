#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_bm1684x.h"

bm_status_t bmcv_as_strided(
        bm_handle_t handle,
        bm_device_mem_t input,
        bm_device_mem_t output,
        int input_row,
        int input_col,
        int output_row,
        int output_col,
        int row_stride,
        int col_stride){
    bm_status_t ret = BM_SUCCESS;
    if (handle == NULL){
        bmlib_log("AS_STRIDED", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_DEVNOTREADY;
    }

    bm_api_cv_as_strided_t api;
    api.input_addr = bm_mem_get_device_addr(input);
    api.output_addr = bm_mem_get_device_addr(output);
    api.input_row = input_row;
    api.input_col = input_col;
    api.output_row = output_row;
    api.output_col = output_col;
    api.row_stride = row_stride;
    api.col_stride = col_stride;

    unsigned int chipid = BM1684X;
    bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    switch (chipid)
    {
        case 0x1684:
            bmlib_log("AS_STRIDED", BMLIB_LOG_ERROR, "1684 not support!\r\n");
            return BM_ERR_NOFEATURE;

        case BM1684X:
            ret = bm_tpu_kernel_launch(handle, "cv_as_strided", (u8 *)&api, sizeof(api));
            if (BM_SUCCESS != ret) {
                bmlib_log("AS_STRIDED", BMLIB_LOG_ERROR, "as_strided sync api error\n");
            }
            break;

        default:
            ret = BM_ERR_NOFEATURE;
            break;
    }

    return ret;
}
