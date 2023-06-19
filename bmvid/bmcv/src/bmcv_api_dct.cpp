#include <memory>
#include <vector>
#include <numeric>
#include "bmcv_api_ext.h"
#include "bmcv_common_bm1684.h"
#include "device_mem_allocator.h"


static bm_status_t check_image(bm_image input) {
    if (input.image_format != FORMAT_GRAY) {
        bmlib_log("DCT", BMLIB_LOG_ERROR, "input format only support gray!\r\n");
        return BM_NOT_SUPPORTED;
    }
    if (input.data_type != DATA_TYPE_EXT_FLOAT32) {
        bmlib_log("DCT", BMLIB_LOG_ERROR, "input data type only support float32!\r\n");
        return BM_NOT_SUPPORTED;
    }
    int stride[3];
    bm_image_get_stride(input, stride);
    if ((u32)stride[0] != input.width * sizeof(float)) {
        bmlib_log("DCT", BMLIB_LOG_ERROR, "stride[0] should equal to input.width*sizeof(float)!\r\n");
        return BM_NOT_SUPPORTED;
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_dct_coeff_bm1684(
        bm_handle_t handle,
        int H,
        int W,
        bm_device_mem_t hcoeff_output,
        bm_device_mem_t wcoeff_output,
        bool is_inversed
        ){
    DeviceMemAllocator allocator(handle);
    int N = H>W?H:W;
    auto range_mem = allocator.alloc_on_device<float>(N, [](float* ptr, u32 len){
        std::iota(ptr, ptr+len, 0);
        return len;
    });
    hcoeff_output = allocator.map_output_to_device<float>(hcoeff_output, H*H);
    wcoeff_output = allocator.map_output_to_device<float>(wcoeff_output, W*W);

    bm_api_dct_coeff_t api_param;
    api_param.H = H;
    api_param.W = W;
    api_param.range_addr = bm_mem_get_device_addr(range_mem);
    api_param.output_hcoeff_addr = bm_mem_get_device_addr(hcoeff_output);
    api_param.output_wcoeff_addr = bm_mem_get_device_addr(wcoeff_output);
    api_param.is_inversed = is_inversed;
    BM_CHECK_RET(bm_send_api(handle,  BM_API_ID_CV_DCT_COEFF, (u8*)&api_param, sizeof(api_param)));
    BM_CHECK_RET(bm_sync_api(handle));

    return BM_SUCCESS;
}

bm_status_t bmcv_dct_coeff(
        bm_handle_t handle,
        int H,
        int W,
        bm_device_mem_t hcoeff_output,
        bm_device_mem_t wcoeff_output,
        bool is_inversed
        ){

    unsigned int chipid = 0x1686;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    switch(chipid)
    {

      case 0x1684:
        ret = bmcv_dct_coeff_bm1684(handle, H, W, hcoeff_output, wcoeff_output, is_inversed);
        break;

      case 0x1686:
        printf("bm1684x not support\n");
        ret = BM_NOT_SUPPORTED;
        break;

      default:
        ret = BM_NOT_SUPPORTED;
        break;
    }

    return ret;
}

static bm_status_t __bmcv_dct_inner_bm1684(
        bm_handle_t handle,
        bm_image input,
        bool is_inversed,
        bool coeff_ready,
        bm_device_mem_t hcoeff,
        bm_device_mem_t wcoeff,
        bm_image output
        ){
        bm_status_t ret = check_image(input);
        if (ret != BM_SUCCESS)
            return ret;
        u32 image_size = input.height * input.width;
        bm_device_mem_t buffer_mem;
        bm_device_mem_t input_mem;
        bm_device_mem_t output_mem;
        try {
            DeviceMemAllocator allocator(handle);
            if(coeff_ready){
                if(bm_mem_get_type(hcoeff) == BM_MEM_TYPE_DEVICE){
                    ASSERT(bm_mem_get_device_size(hcoeff) == input.height*input.height*sizeof(float));
                } else {
                    bmlib_log(__func__, BMLIB_LOG_WARNING, "hcoeff should be a device mem!");
                }
                if(bm_mem_get_type(wcoeff) == BM_MEM_TYPE_DEVICE){
                    ASSERT(bm_mem_get_device_size(wcoeff) == input.width*input.width*sizeof(float));
                } else {
                    bmlib_log(__func__, BMLIB_LOG_WARNING, "wcoeff should be a device mem!");
                }
                hcoeff = allocator.map_input_to_device<float>(hcoeff, input.height * input.height);
                wcoeff = allocator.map_input_to_device<float>(wcoeff, input.width * input.width);
                buffer_mem = allocator.alloc_on_device<float>(image_size);
            } else {
                hcoeff = allocator.alloc_on_device<float>(input.height * input.height);
                wcoeff = allocator.alloc_on_device<float>(input.width * input.width);
                u32 N = input.height> input.width? input.height: input.width;
                buffer_mem = allocator.alloc_on_device<float>(image_size, [N](float* ptr, u32){
                    std::iota(ptr, ptr+N, 0);
                    return N;
                });
            }
            BM_CHECK_RET(bm_image_get_device_mem(input, &input_mem));
            input_mem = allocator.map_input_to_device<float>(input_mem, image_size);

            output.height = input.height;
            output.width = input.width;
            output.image_format = input.image_format;
            output.data_type = input.data_type;
            if(!bm_image_is_attached(output)){
                BM_CHECK_RET(bm_image_alloc_dev_mem(output, BMCV_HEAP_ANY));
            }
            BM_CHECK_RET(bm_image_get_device_mem(output, &output_mem));
            output_mem = allocator.map_output_to_device<float>(output_mem, image_size);

            bm_api_dct_t api_param;
            api_param.N = 1;
            api_param.C = 1;
            api_param.H = input.height;
            api_param.W = input.width;
            api_param.input_addr = bm_mem_get_device_addr(input_mem);
            api_param.hcoeff_addr = bm_mem_get_device_addr(hcoeff);
            api_param.wcoeff_addr = bm_mem_get_device_addr(wcoeff);
            api_param.output_addr = bm_mem_get_device_addr(output_mem);
            api_param.buffer_addr = bm_mem_get_device_addr(buffer_mem);
            api_param.coeff_ready = coeff_ready;
            api_param.is_inversed = is_inversed;
            BM_CHECK_RET(bm_send_api(handle,  BM_API_ID_CV_DCT, (u8*)&api_param, sizeof(api_param)));
            BM_CHECK_RET(bm_sync_api(handle));
        } catch (bm_status_t ret) {
            return ret;
        }
        return BM_SUCCESS;
}

static bm_status_t __bmcv_dct_inner(
        bm_handle_t handle,
        bm_image input,
        bool is_inversed,
        bool coeff_ready,
        bm_device_mem_t hcoeff,
        bm_device_mem_t wcoeff,
        bm_image output
        ){

    unsigned int chipid = 0x1686;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    switch(chipid)
    {

      case 0x1684:
        ret = __bmcv_dct_inner_bm1684(handle, input, is_inversed, coeff_ready,
                                      hcoeff, wcoeff, output);
        break;

      case 0x1686:
        printf("bm1684x not support\n");
        ret = BM_NOT_SUPPORTED;
        break;

      default:
        ret = BM_NOT_SUPPORTED;
        break;
    }

    return ret;
}

bm_status_t bmcv_image_dct_with_coeff(
        bm_handle_t handle,
        bm_image input,
        bm_device_mem_t hcoeff,
        bm_device_mem_t wcoeff,
        bm_image output
        ){
    return __bmcv_dct_inner(handle, input, 0, 1, hcoeff, wcoeff, output);
}

bm_status_t bmcv_image_dct(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        bool is_inversed
        ){
    bm_device_mem_t dummy_mem;
    return __bmcv_dct_inner(handle, input, is_inversed, 0, dummy_mem, dummy_mem, output);
}
