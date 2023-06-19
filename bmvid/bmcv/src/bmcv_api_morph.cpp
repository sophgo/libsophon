#include "bmcv_api_ext.h"
#include "bmcv_internal.h"
#include "bmlib_runtime.h"
#include "bmcv_common_bm1684.h"
#include "device_mem_allocator.h"
#include "pcie_cpu/bmcv_api_struct.h"
#include <memory>
#include <vector>
#include <cmath>
#include <thread>

using namespace std;

static bm_status_t bmcv_morph_check(
        bm_handle_t handle,
        bm_image input,
        bm_image output) {
    if (handle == NULL) {
        bmlib_log("MORPH", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
        return BM_ERR_PARAM;
    }
    if (input.height != output.height ||
        input.width != output.width) {
        bmlib_log("MORPH", BMLIB_LOG_ERROR, "input and output image size should be same\n");
        return BM_NOT_SUPPORTED;
    }
    if (input.image_format != FORMAT_BGR_PACKED &&
        input.image_format != FORMAT_RGB_PACKED &&
        input.image_format != FORMAT_BGR_PLANAR &&
        input.image_format != FORMAT_RGB_PLANAR &&
        input.image_format != FORMAT_GRAY) {
        bmlib_log("MORPH", BMLIB_LOG_ERROR, "Not supported input image format\n");
        return BM_NOT_SUPPORTED;
    }
    if (input.image_format != output.image_format) {
        bmlib_log("MORPH", BMLIB_LOG_ERROR, "input and output image format should be same\n");
        return BM_NOT_SUPPORTED;
    }
    if (input.data_type != DATA_TYPE_EXT_1N_BYTE ||
        output.data_type != DATA_TYPE_EXT_1N_BYTE) {
        bmlib_log("MORPH", BMLIB_LOG_ERROR, "Not supported image data type\n");
        return BM_NOT_SUPPORTED;
    }
    return BM_SUCCESS;
}

static void pad_const(
        const unsigned char* in,
        unsigned char* out,
        int c,
        int w,
        int h,
        int srcstep,
        int pad_w_l,
        int pad_w_r,
        int pad_h_t,
        int pad_h_b,
        unsigned char val
        ) {
    auto PAD = [&](int start, int lines) {
        for (int i = start; i < start + lines; i++) {
            for (int j = 0; j < pad_w_l * c; j++) {
                out[(i + pad_h_t) * (w + pad_w_l + pad_w_r) * c + j] = val;
            }
            for (int j = 0; j < pad_w_r * c; j++) {
                out[(i + pad_h_t + 1) * (w + pad_w_l + pad_w_r) * c - j - 1] = val;
            }
            memcpy(out + (i + pad_h_t) * (w + pad_w_l + pad_w_r) * c + pad_w_l * c,
                   in + i * srcstep,
                   w * c);
        }
    };
    int threadNum = thread::hardware_concurrency() > 5 ? 5 : thread::hardware_concurrency();
    vector<pair<int, int>> param(threadNum);
    int lines = (h + threadNum - 1) / threadNum;
    int last_lines = h - lines * (threadNum - 1);
    for (int i = 0; i < threadNum - 1; i++) {
        param.push_back(make_pair(i * lines, lines));
    }
    param.push_back(make_pair((threadNum - 1) * lines, last_lines));
    vector<thread> threads;
    for (auto it : param)
        threads.push_back(std::thread(PAD, it.first, it.second));
    // add top and bottom pad
    for (int i = 0; i < pad_h_t; i++) {
        for (int j = 0; j < (w + pad_w_l + pad_w_r) * c; j++) {
            out[i * (w + pad_w_l + pad_w_r) * c + j] = val;
        }
    }
    for (int i = 0; i < pad_h_b; i++) {
        for (int j = 0; j < (w + pad_w_l + pad_w_r) * c; j++) {
            out[(h + pad_h_t + i) * (w + pad_w_l + pad_w_r) * c + j] = val;
        }
    }
    for (auto &it : threads)
        it.join();
}

static void get_kernel_coord(
        const unsigned char* kernel,
        int kw,
        int kh,
        vector<bmcv_point_t>& coords) {
    for (int i = 0; i < kh; i++) {
        for (int j = 0; j < kw; j++) {
            if (kernel[i * kw +j]) {
                coords.push_back({j, i});
            }
        }
    }
}

template<typename T>
struct MaxOp {
    #ifdef __linux__
    T operator ()(const T a, const T b) const { return std::max(a, b); }
    #else
    T operator()(const T a, const T b) const {
        return (std::max)(a, b);
    }
    #endif
    int val = 0;
};

template<typename T>
struct MinOp {
    #ifdef __linux__
    T operator ()(const T a, const T b) const { return std::min(a, b); }
    #else
    T operator()(const T a, const T b) const {
        return (std::min)(a, b);
    }
    #endif
    int val = 255;
};

template<class Op>
static void morph_cpu(
        const unsigned char* src,
        const unsigned char* kernel,
        unsigned char* dst,
        int kw,
        int kh,
        int srcstep,
        int dststep,
        int width,
        int height,
        bool packed) {
    Op op;
    int cn = packed ? 3 : 1;
    int pad_w_l = kw / 2;
    int pad_w_r = kw - 1 - pad_w_l;
    int pad_h_t = kh / 2;
    int pad_h_b = kh - 1 - pad_h_t;
    int sw = width + pad_w_l + pad_w_r;
    int sh = height + pad_h_t + pad_h_t;
    unsigned char* src_padded = new unsigned char [sw * sh * cn];
    pad_const(
            src,
            src_padded,
            cn,
            width,
            height,
            srcstep,
            pad_w_l,
            pad_w_r,
            pad_h_t,
            pad_h_b,
            op.val);
    vector<bmcv_point_t> coords;
    get_kernel_coord(kernel, kw, kh, coords);
    const bmcv_point_t* pt = coords.data();
    int nz = (int)coords.size();
    width *= cn;
    auto MORPH = [&](int start, int lines) {
        vector<unsigned char*> ptrs(nz);
        const unsigned char** kp = (const unsigned char**)(ptrs.data());
        const unsigned char* S = static_cast<const unsigned char*>(src_padded) + start * sw * cn;
        unsigned char* D = dst + start * dststep;
        while (lines--) {
            for (int k = 0; k < nz; k++) {
                kp[k] = S + pt[k].y * sw * cn + pt[k].x * cn;
            }
            int i = 0;
            for (; i <= width - 4; i += 4) {
                const unsigned char* sptr = kp[0] + i;
                unsigned char s0 = sptr[0], s1 = sptr[1], s2 = sptr[2], s3 = sptr[3];
                for (int k = 1; k < nz; k++ ) {
                    sptr = kp[k] + i;
                    s0 = op(s0, sptr[0]); s1 = op(s1, sptr[1]);
                    s2 = op(s2, sptr[2]); s3 = op(s3, sptr[3]);
                }
                D[i] = s0; D[i+1] = s1;
                D[i+2] = s2; D[i+3] = s3;
            }
            for (; i < width; i++) {
                unsigned char s0 = kp[0][i];
                for (int k = 1; k < nz; k++)
                    s0 = op(s0, kp[k][i]);
                D[i] = s0;
            }
            S += sw * cn;
            D += dststep;
        }
    };
    int threadNum = thread::hardware_concurrency();
    vector<pair<int, int>> param(threadNum);
    int lines = (height + threadNum - 1) / threadNum;
    int last_lines = height - lines * (threadNum - 1);
    param.clear();
    for (int i = 0; i < threadNum - 1; i++) {
        param.push_back(make_pair(i * lines, lines));
    }
    param.push_back(make_pair((threadNum - 1) * lines, last_lines));
    vector<thread> threads;
    for (auto it : param)
        threads.push_back(std::thread(MORPH, it.first, it.second));
    for (auto &it : threads)
        it.join();
}

bm_device_mem_t bmcv_get_structuring_element(
        bm_handle_t handle,
        bmcv_morph_shape_t shape,
        int kw,
        int kh
        ){
    int i, j;
    int r = 0, c = 0;
    double inv_r2 = 0;
    int center_x = kw / 2;
    int center_y = kh / 2;
    if (kh == 1 && kw == 1)
        shape = BM_MORPH_RECT;
    if (shape == BM_MORPH_ELLIPSE) {
        r = kh / 2;
        c = kw / 2;
        inv_r2 = r ? 1./((double)r * r) : 0;
    }
    unsigned char* kernel = new unsigned char [kh * kw];
    for (i = 0; i < kh; i++) {
        unsigned char* ptr = kernel + i * kw;
        int j1 = 0, j2 = 0;
        if (shape == BM_MORPH_RECT || (shape == BM_MORPH_CROSS && i == center_y))
            j2 = kw;
        else if (shape == BM_MORPH_CROSS)
            j1 = center_x, j2 = j1 + 1;
        else {
            int dy = i - r;
            if (std::abs(dy) <= r) {
                int dx = ceil(c * std::sqrt((r * r - dy * dy) * inv_r2));
                #ifdef __linux__
                j1 = std::max(c - dx, 0);
                j2 = std::min(c + dx + 1, kw);
                #else
                j1 = (std::max)(c - dx, 0);
                j2 = (std::min)(c + dx + 1, kw);
                #endif
            }
        }

        for (j = 0; j < j1; j++)
            ptr[j] = 0;
        for (; j < j2; j++)
            ptr[j] = 1;
        for (; j < kw; j++)
            ptr[j] = 0;
    }
    bm_device_mem_t kmem;
    bm_status_t ret = bm_malloc_device_byte(handle, &kmem, kw * kh * sizeof(unsigned char));
    if (BM_SUCCESS != ret) {
        bmlib_log("MORPH", BMLIB_LOG_ERROR, "malloc kernel device memory failed!\r\n");
    }
    ret = bm_memcpy_s2d(handle, kmem, kernel);
    if (BM_SUCCESS != ret) {
        bmlib_log("MORPH", BMLIB_LOG_ERROR, "kernel s2d failed!\r\n");
        bm_free_device(handle, kmem);
    }
    delete [] kernel;
    return kmem;
}

bm_status_t bmcv_image_morph_bm1684(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        int kw,
        int kh,
        bm_device_mem_t kmem,
        int op) {
    bm_status_t ret = BM_SUCCESS;
    ret = bmcv_morph_check(handle, input, output);
    if (BM_SUCCESS != ret) {
        return ret;
    }
    if (!bm_image_is_attached(output)) {
        ret = bm_image_alloc_dev_mem(output, BMCV_HEAP_ANY);
        if (ret != BM_SUCCESS) {
            return ret;
        }
    }
    int stride_i[3], stride_o[3];
    bm_image_get_stride(input, stride_i);
    bm_image_get_stride(output, stride_o);
    // if kernel shape is rectangle
    unsigned char* kernel = new unsigned char [kh * kw];
    ret = bm_memcpy_d2s(handle, kernel, kmem);
    if (BM_SUCCESS != ret) {
        bmlib_log("MORPH", BMLIB_LOG_ERROR, "kernel d2s failed!\r\n");
        delete [] kernel;
        return BM_ERR_FAILURE;
    }
    int not_zero_cnt = 0;
    for (int i = 0; i < kw * kh; i++) {
        if (kernel[i]) not_zero_cnt++;
    }
    bool using_tpu = not_zero_cnt == kh * kw;
    if (using_tpu) {
        bm_device_mem_t imem, omem;
        bm_image_get_device_mem(input, &imem);
        bm_image_get_device_mem(output, &omem);
        bm_api_cv_morph_t api;
        api.src_addr = bm_mem_get_device_addr(imem);
        api.dst_addr = bm_mem_get_device_addr(omem);
        api.height = input.height;
        api.width = input.width;
        api.kh = kh;
        api.kw = kw;
        api.stride_i = stride_i[0];
        api.stride_o = stride_o[0];
        api.format = input.image_format;
        api.op = op;
        ret = bm_send_api(handle,  BM_API_ID_CV_MORPH, (uint8_t*)&api, sizeof(api));
        if (BM_SUCCESS != ret) {
            bmlib_log("MORPH", BMLIB_LOG_ERROR, "MORPH send api error\n");
            delete [] kernel;
            return ret;
        }
        ret = bm_sync_api(handle);
        if (BM_SUCCESS != ret) {
            bmlib_log("MORPH", BMLIB_LOG_ERROR, "morph sync api error\n");
            delete [] kernel;
            return ret;
        }
        delete [] kernel;
        return BM_SUCCESS;
    }
    int timeout = -1;
    int process_id = get_cpu_process_id(handle);
    if (process_id != -1) {
        bm_device_mem_t imem, omem;
        bm_image_get_device_mem(input, &imem);
        bm_image_get_device_mem(output, &omem);
        bmcv_morph_param_t morph_p;
        morph_p.src_addr = get_mapped_addr(handle, &imem);
        morph_p.dst_addr = get_mapped_addr(handle, &omem);
        morph_p.kernel_addr = get_mapped_addr(handle, &kmem);
        morph_p.kh = kh;
        morph_p.kw = kw;
        morph_p.width = input.width;
        morph_p.height = input.height;
        morph_p.srcstep = stride_i[0];
        morph_p.dststep = stride_o[0];
        morph_p.format = input.image_format;
        morph_p.op = op;
        // copy param to device memory
        DeviceMemAllocator allocator(handle);
        bm_device_mem_t pmem;
        try {
            pmem = allocator.map_input_to_device<char>(
                              bm_mem_from_system(&morph_p),
                              sizeof(bmcv_morph_param_t));
        } catch (bm_status_t ret) {
            delete [] kernel;
            return ret;
        }
        u64 param_addr_mapped = get_mapped_addr(handle, &pmem);
        int ret = bmcpu_exec_function_ext(
                handle,
                process_id,
                (char*)"bmcv_cpu_morph",
                (void*)&param_addr_mapped,
                sizeof(void*),
                1,
                timeout);
        if (ret != 0) {
            bmlib_log("MORPH", BMLIB_LOG_ERROR, "exec function failed! return %d\r\n", ret);
            delete [] kernel;
            return BM_ERR_FAILURE;
        }
        delete [] kernel;
    } else {
        int ch = (input.image_format == FORMAT_GRAY ||
                  input.image_format == FORMAT_BGR_PACKED ||
                  input.image_format == FORMAT_RGB_PACKED) ? 1 : 3;
        bool packed = (input.image_format == FORMAT_BGR_PACKED ||
                       input.image_format == FORMAT_RGB_PACKED);
        unsigned char* data_i = new unsigned char [input.height * stride_i[0] * ch];
        unsigned char* data_o = new unsigned char [output.height * stride_o[0] * ch];
        bm_image_copy_device_to_host(input, (void **)&data_i);
        for (int i = 0; i < ch; i++) {
            if (op)
                morph_cpu<MaxOp<unsigned char>>(
                        data_i + i * input.height * stride_i[0],
                        kernel,
                        data_o + i * input.height * stride_i[0],
                        kw,
                        kh,
                        stride_i[0],
                        stride_o[0],
                        input.width,
                        input.height,
                        packed);
            else
                morph_cpu<MinOp<unsigned char>>(
                        data_i + i * input.height * stride_i[0],
                        kernel,
                        data_o + i * input.height * stride_i[0],
                        kw,
                        kh,
                        stride_i[0],
                        stride_o[0],
                        input.width,
                        input.height,
                        packed);
        }
        bm_image_copy_host_to_device(output, (void **)&data_o);
        delete [] data_i;
        delete [] data_o;
        delete [] kernel;
    }
    return BM_SUCCESS;
}

bm_status_t bmcv_image_morph(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        int kw,
        int kh,
        bm_device_mem_t kmem,
        int op) {

    unsigned int chipid = BM1684X;
    bm_status_t ret = BM_SUCCESS;

    ret = bm_get_chipid(handle, &chipid);
    if (BM_SUCCESS != ret)
      return ret;

    switch(chipid)
    {

      case 0x1684:
        ret = bmcv_image_morph_bm1684(handle, input, output, kw, kh, kmem, op);
        break;

      case BM1684X:
        printf("bm1684x not support\n");
        ret = BM_NOT_SUPPORTED;
        break;

      default:
        ret = BM_NOT_SUPPORTED;
        break;
    }

    return ret;
}

bm_status_t bmcv_image_erode(
        bm_handle_t handle,
        bm_image src,
        bm_image dst,
        int kw,
        int kh,
        bm_device_mem_t kmem
        ) {
    return bmcv_image_morph(handle, src, dst, kw, kh, kmem, 0);
}

bm_status_t bmcv_image_dilate(
        bm_handle_t handle,
        bm_image src,
        bm_image dst,
        int kw,
        int kh,
        bm_device_mem_t kmem
        ) {
    return bmcv_image_morph(handle, src, dst, kw, kh, kmem, 1);
}