#include <memory>
#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmcv_json.hpp"

bm_status_t bmcv_debug_savedata(bm_image image, const char* name) {
    uint32_t data_offset =
        (6 * sizeof(uint32_t) + MAX_bm_image_CHANNEL * sizeof(uint64_t) +
         MAX_bm_image_CHANNEL * 8 * sizeof(uint32_t) +
         MAX_bm_image_CHANNEL * sizeof(uint64_t));
    uint32_t width        = image.width;
    uint32_t height       = image.height;
    uint32_t image_format = (uint32_t)image.image_format;
    uint32_t data_type    = (uint32_t)image.data_type;
    uint32_t plane_num =
        image.image_private ? image.image_private->plane_num : 0;

    std::vector<std::unique_ptr<unsigned char[]>> host_ptr;
    host_ptr.resize(plane_num);

    if (plane_num == 0 || !bm_image_is_attached(image)) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "error It is a empty image\n");
        return BM_ERR_FAILURE;
    }

    uint64_t size[MAX_bm_image_CHANNEL]         = {0};
    uint32_t pitch_stride[MAX_bm_image_CHANNEL] = {0};

    uint32_t N[MAX_bm_image_CHANNEL] = {0};
    uint32_t C[MAX_bm_image_CHANNEL] = {0};
    uint32_t H[MAX_bm_image_CHANNEL] = {0};
    uint32_t W[MAX_bm_image_CHANNEL] = {0};

    uint32_t channel_stride[MAX_bm_image_CHANNEL] = {0};
    uint32_t batch_stride[MAX_bm_image_CHANNEL]   = {0};

    uint32_t meta_data_size[MAX_bm_image_CHANNEL] = {0};

    uint64_t reserved0[MAX_bm_image_CHANNEL] = {0};

    for (uint32_t i = 0; i < plane_num; i++) {
        N[i] = image.image_private->memory_layout[i].N;
        C[i] = image.image_private->memory_layout[i].C;
        H[i] = image.image_private->memory_layout[i].H;
        W[i] = image.image_private->memory_layout[i].W;

        pitch_stride[i] = image.image_private->memory_layout[i].pitch_stride;
        channel_stride[i] =
            image.image_private->memory_layout[i].channel_stride;
        batch_stride[i]   = image.image_private->memory_layout[i].batch_stride;
        meta_data_size[i] = image.image_private->memory_layout[i].data_size;

        size[i] = image.image_private->memory_layout[i].size;
    }

    for (uint32_t i = 0; i < plane_num; i++) {
        host_ptr[i] =
            std::unique_ptr<unsigned char[]>(new unsigned char[size[i]]);

        if (BM_SUCCESS != bm_memcpy_d2s(image.image_private->handle,
                                        host_ptr[i].get(),
                                        image.image_private->data[i])) {
            bmlib_log(BMCV_LOG_TAG,
                      BMLIB_LOG_ERROR,
                      "bmcv copy device to host error\n");
            return BM_ERR_FAILURE;
        }
    }

    FILE* fp = fopen(name, "wb");
    if (fp == NULL) {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_ERROR, "error failed to open file\n");
        return BM_ERR_FAILURE;
    }

    assert(fwrite(&data_offset, 1, sizeof(data_offset), fp) ==
           sizeof(data_offset));
    assert(fwrite(&width, 1, sizeof(width), fp) == sizeof(width));
    assert(fwrite(&height, 1, sizeof(height), fp) == sizeof(height));
    assert(fwrite(&image_format, 1, sizeof(image_format), fp) ==
           sizeof(image_format));
    assert(fwrite(&data_type, 1, sizeof(data_type), fp) == sizeof(data_type));
    assert(fwrite(&plane_num, 1, sizeof(plane_num), fp) == sizeof(plane_num));

    assert(fwrite(size, 1, sizeof(size), fp) == sizeof(size));
    assert(fwrite(pitch_stride, 1, sizeof(pitch_stride), fp) ==
           sizeof(pitch_stride));

    assert(fwrite(channel_stride, 1, sizeof(channel_stride), fp) ==
           sizeof(channel_stride));
    assert(fwrite(batch_stride, 1, sizeof(batch_stride), fp) ==
           sizeof(batch_stride));
    assert(fwrite(meta_data_size, 1, sizeof(meta_data_size), fp) ==
           sizeof(meta_data_size));

    assert(fwrite(N, 1, sizeof(N), fp) == sizeof(N));
    assert(fwrite(C, 1, sizeof(C), fp) == sizeof(C));
    assert(fwrite(H, 1, sizeof(H), fp) == sizeof(H));
    assert(fwrite(W, 1, sizeof(W), fp) == sizeof(W));

    assert(fwrite(reserved0, 1, sizeof(reserved0), fp) == sizeof(reserved0));

    for (uint32_t i = 0; i < plane_num; i++) {
        assert(fwrite(host_ptr[i].get(), 1, size[i], fp) == size[i]);
    }
    fclose(fp);
    return BM_SUCCESS;
}