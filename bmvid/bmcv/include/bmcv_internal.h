#ifndef BMCV_INTERNAL_H
#define BMCV_INTERNAL_H
#include "bmcv_api_ext.h"
#include "bmlib_runtime.h"
#include <string.h>
#ifndef USING_CMODEL

#include "bmjpuapi_jpeg.h"

#endif

#if defined(__cplusplus)
extern "C" {
#endif

#define BMCV_VERSION "1.1.0"

/*
 * bmcv api for using internal.
 */
#define MIN_PROPOSAL_NUM (1)
#define BMCV_LOG_TAG "BMCV"
#define MAX_BMCV_LOG_BUFFER_SIZE (256)
#define MAX_bm_image_CHANNEL 4
#define BM1684X 0x1686
#define COLOR_SPACE_YUV             0
#define COLOR_SPACE_RGB             1
#define COLOR_SPACE_HSV             2
#define COLOR_SPACE_RGBY            3
#define COLOR_NOT_DEF               4

#define filename(x) (strrchr(x, '/') ? strrchr(x, '/') + 1 : x)

#define VPP_MAX_SCALING_RATIO (32)
#define MAX_BATCH_SIZE (32)
#define MOSAIC_SIZE 8
#define VPPALIGN(x, mask)  (((x) + ((mask)-1)) & ~((mask)-1))

struct sg_device_mem_st
{
    bool flag = 0;
    bm_device_mem_t bm_device_mem;
};

typedef struct bmcv_shape_st {
    int n;
    int c;
    int h;
    int w;
} bmcv_shape_t;

typedef struct {
    bmcv_shape_t score_shape;
    bmcv_shape_t bbox_shape;
    float        scale_factor;
    int          feat_factor;
    int          feat_w;
    int          feat_h;
    int          width;
    int          height;
    int          feat_stride;
    int          anchor_scale_size;
    int          base_size;
    float        im_scale;
    int          min_size;
    float        base_threshold;
    int          per_nms_topn;
} bmcv_gen_proposal_attr_t;

typedef struct {
    int   index;
    float val;
#ifndef WIN32
} __attribute__((packed)) sort_t;
#else
} sort_t;
#endif

#define MAX_INTERGRATED_NUM (3)
typedef enum {
    UINT8_C1 = 0,
    UINT8_C3,
    INT8_C1,
    INT8_C3,
    FLOAT32_C1,
    FLOAT32_C3,
    MAX_COLOR_TYPE
} cv_color_e;

typedef enum {
    DO_WITH_ALIGN = 0,
    DO_HEIGHT_ALIGN,
    DO_WIDTH_HEIGHT_ALIGN,
    MAX_ALIGN_TYPE
} align_type_e;

typedef enum {
    CONVERT_1N_TO_1N = 0,
    CONVERT_4N_TO_4N,
    CONVERT_4N_TO_1N
} convert_storage_mode_e;

struct bm_image_tensor {
    bm_image image;
    int      image_c;
    int      image_n;
};

typedef struct {
    int   img_w;
    int   img_h;
    float alpha_0;
    float beta_0;
    float alpha_1;
    float beta_1;
    float alpha_2;
    float beta_2;
    int   convert_storage_mode;
    int   image_num;
    int   input_img_data_type;
    int   output_img_data_type;
#ifndef WIN32
} __attribute__((packed)) bmcv_convert_to_attr_t;
#else
} bmcv_convert_to_attr_t;
#endif
#if 0
struct vpp_cmd_n {
  int src_format;
  int src_stride;
  int src_endian;
  int src_endian_a;
  int src_plannar;
  int src_fd0;
  int src_fd1;
  int src_fd2;
  unsigned long src_addr0;
  unsigned long src_addr1;
  unsigned long src_addr2;
  unsigned short src_axisX;
  unsigned short src_axisY;
  unsigned short src_cropW;
  unsigned short src_cropH;

  int dst_format;
  int dst_stride;
  int dst_endian;
  int dst_endian_a;
  int dst_plannar;
  int dst_fd0;
  int dst_fd1;
  int dst_fd2;
  unsigned long dst_addr0;
  unsigned long dst_addr1;
  unsigned long dst_addr2;
  unsigned short dst_axisX;
  unsigned short dst_axisY;
  unsigned short dst_cropW;
  unsigned short dst_cropH;

  int src_csc_en;
  int hor_filter_sel;
  int ver_filter_sel;
  int scale_x_init;
  int scale_y_init;
  int csc_type;
  int mapcon_enable;
  int src_fd3;
  unsigned long src_addr3;
  int cols;
  int rows;
};

struct vpp_batch_n {
    int num;
    struct vpp_cmd_n cmd[16];
};
#endif

typedef struct{
    unsigned short src_address_align;
    unsigned short dst_address_align;
    unsigned short src_stride_align[MAX_bm_image_CHANNEL];
    unsigned short dst_stride_align[MAX_bm_image_CHANNEL];
    unsigned short width_max;
    unsigned short width_min;
    unsigned short height_max;
    unsigned short height_min;
    unsigned short zoom_limitation_max;
    unsigned short zoom_limitation_min;
    ///////////
    unsigned short src_width_align;
    unsigned short src_height_align;
    unsigned short src_offset_x_align;
    unsigned short src_offset_y_align;
    unsigned short dst_width_align;
    unsigned short dst_height_align;
    unsigned short dst_offset_x_align;
    unsigned short dst_offset_y_align;
    //////////////////////
    unsigned char support_crop;
    unsigned char support_scale;
}vpp_limitation;

typedef struct{
    unsigned char ch0;
    unsigned char ch1;
    unsigned char ch2;
}color_per_ch;

typedef struct {
    int width;
    int height;
    bm_image_format_ext format;
    void** data;
    int* step;
} bmMat;

struct dynamic_load_param{
    int api_id;
    size_t size;
    char param[0];
};

int find_tpufirmaware_path(char fw_path[512], const char* path);
bm_status_t bm_load_tpu_module(bm_handle_t handle, tpu_kernel_module_t *tpu_module);
bm_status_t bm_kernel_main_launch(bm_handle_t handle, int api_id, void *param, size_t size);
bm_status_t bm_tpu_kernel_launch(bm_handle_t handle,
                         const char *func_name,
                         void       *param,
                         size_t      size);

void draw_line(
        bmMat &inout,
        bmcv_point_t start,
        bmcv_point_t end,
        bmcv_color_t color,
        int thickness);

void put_text(
        bmMat mat,
        const char* text,
        bmcv_point_t org,
        int fontFace,
        float fontScale,
        bmcv_color_t color,
        int thickness);

void lkpyramid_postprocess(
        int cn,
        int width,
        int height,
        int prevStep,
        int nextStep,
        int derivStep,
        unsigned char* prevPtr,
        unsigned char* nextPtr,
        short* derivPtr,
        bmcv_point2f_t* prevPts,
        bmcv_point2f_t* nextPts,
        int ptsNum,
        int kw,
        int kh,
        int level,
        int maxLevel,
        bool* status,
        bmcv_term_criteria_t criteria);

int get_cpu_process_id(bm_handle_t handle);

unsigned long long get_mapped_addr(bm_handle_t handle, bm_device_mem_t* mem);

bm_status_t bmcv_fft_1d_create_plan_by_factors(bm_handle_t handle,
                                               int batch,
                                               int len,
                                               const int *factors,
                                               int factorSize,
                                               bool forward,
                                               void *&plan);

bm_status_t bmcv_gen_prop_nms(bm_handle_t     handle,
                              bm_device_mem_t scores_addr_0,
                              bm_device_mem_t bbox_deltas_addr_0,
                              bm_device_mem_t anchor_scales_addr_0,
                              bm_device_mem_t gen_proposal_attr_addr_0,
                              bm_device_mem_t scores_addr_1,
                              bm_device_mem_t bbox_deltas_addr_1,
                              bm_device_mem_t anchor_scales_addr_1,
                              bm_device_mem_t gen_proposal_attr_addr_1,
                              bm_device_mem_t scores_addr_2,
                              bm_device_mem_t bbox_deltas_addr_2,
                              bm_device_mem_t anchor_scales_addr_2,
                              bm_device_mem_t gen_proposal_attr_addr_2,
                              int             anchor_num,
                              float           nms_threshold,
                              float           filter_threshold,
                              bm_device_mem_t filter_output,
                              bm_device_mem_t filter_shape_output);

bm_status_t bmcv_convert_to_intergrated(bm_handle_t     handle,
                                        bm_device_mem_t input_img_addr_0,
                                        bm_device_mem_t output_img_addr_0,
                                        bm_device_mem_t input_img_addr_1,
                                        bm_device_mem_t output_img_addr_1,
                                        bm_device_mem_t input_img_addr_2,
                                        bm_device_mem_t output_img_addr_2,
                                        bm_device_mem_t convert_to_attr_addr,
                                        int             times);

bm_status_t bmcv_sort_test(bm_handle_t     handle,
                           bm_device_mem_t src_index_addr,
                           bm_device_mem_t src_data_addr,
                           bm_device_mem_t dst_index_addr,
                           bm_device_mem_t dst_data_addr,
                           int             order,
                           int             location,
                           int             data_cnt,
                           int             sort_cnt);

bm_status_t bm_vpp_query_limitation(bm_image_format_ext input_format,
                                    bm_image_format_ext output_format,
                                    vpp_limitation &limit);

bm_status_t bm_image_tensor_init(bm_image_tensor *image_tensor);

bm_status_t bm_image_tensor_create(bm_handle_t              handle,
                                   int                      img_n,
                                   int                      img_c,
                                   int                      img_h,
                                   int                      img_w,
                                   bm_image_format_ext      image_format,
                                   bm_image_data_format_ext data_type,
                                   bm_image_tensor *        image);

extern bm_status_t bm_image_tensor_destroy(bm_image_tensor image_tensor);

bm_status_t bm_image_tensor_alloc_dev_mem(bm_image_tensor image_tensor,
                                          int             heap_id);

bm_status_t bm_image_tensor_copy_to_device(bm_image_tensor image_tensor,
                                           void **         buffers);

bm_status_t bm_image_tensor_copy_from_device(bm_image_tensor image_tensor,
                                             void **         buffers);

bool bm_image_tensor_is_attached(bm_image_tensor image_tensor);

bm_status_t bm_image_tensor_get_device_mem(bm_image_tensor  image_tensor,
                                           bm_device_mem_t *mem);

bm_status_t bm_image_tensor_attach(bm_image_tensor  image_tensor,
                                   bm_device_mem_t *device_memory);

bm_status_t bm_image_tensor_detach(bm_image_tensor);

extern bm_status_t concat_images_to_tensor(bm_handle_t      handle,
                                    int              image_num,
                                    bm_image *       images,
                                    bm_image_tensor *image_tensor);

bm_status_t bm_shape_dealign(bm_image in_image,
                             bm_image out_image,
                             int      align_option = DO_HEIGHT_ALIGN);

bm_status_t bm_shape_align(bm_image  image,
                           bm_image *out_image,
                           int       align_option = DO_HEIGHT_ALIGN,
                           int       align_num    = 2);

bm_status_t bm_yuv_height_align(bm_image  input_image,
                                bm_image *output_image,
                                int &     if_yuv_input);

bm_status_t bm_separate_to_planar(bm_handle_t handle,
                                  bm_image input_image,
                                  bm_image output_image);

bm_status_t bm_planar_to_separate(bm_handle_t handle,
                                  bm_image input_image,
                                  bm_image output_image);

bm_status_t bmcv_warp_affine_bilinear(bm_handle_t       handle,
                                      bm_image          src,
                                      int               output_num,
                                      bm_image *        dst,
                                      bmcv_warp_matrix *matrix);

bm_status_t bmcv_image_write_to_json(bm_image image, const char* file);
bm_status_t bmcv_image_create_from_json(bm_handle_t handle, const char* file, bm_image *res);
int is_yuv_or_rgb(bm_image_format_ext fmt);
void calculate_yuv(unsigned char r, unsigned char g, unsigned char b, unsigned char* y_, unsigned char* u_, unsigned char* v_);

void bmcv_err_log_internal(char *log_buf, size_t string_sz, const char *frmt, ...);
void sg_free_device_mem(bm_handle_t handle, sg_device_mem_st mem);
bm_status_t sg_malloc_device_mem(bm_handle_t handle, sg_device_mem_st *pmem, unsigned int size);
bm_status_t sg_image_alloc_dev_mem(bm_image image, int heap_id);

#ifdef __linux__
#define BMCV_ERR_LOG(frmt, args...)                                            \
    do {                                                                       \
        char log_buffer[MAX_BMCV_LOG_BUFFER_SIZE] = {0};                       \
        int  string_sz                            = 0;                         \
        snprintf(log_buffer,                                                   \
                 MAX_BMCV_LOG_BUFFER_SIZE,                                     \
                 " [MESSAGE FROM %s: %s: %d]: ",                               \
                 filename(__FILE__),                                           \
                 __func__,                                                     \
                 __LINE__);                                                    \
        string_sz = strlen(log_buffer);                                        \
        bmcv_err_log_internal(log_buffer, string_sz, frmt, ##args);            \
    } while (0)
#else
#define BMCV_ERR_LOG(frmt)                                                     \
    do {                                                                       \
        char log_buffer[MAX_BMCV_LOG_BUFFER_SIZE] = {0};                       \
        size_t  string_sz                         = 0;                         \
        snprintf(log_buffer,                                                   \
                 MAX_BMCV_LOG_BUFFER_SIZE,                                     \
                 " [MESSAGE FROM %s: %s: %d]: ",                               \
                 filename(__FILE__),                                           \
                 __func__,                                                     \
                 __LINE__);                                                    \
        string_sz = strlen(log_buffer);                                        \
        bmcv_err_log_internal(log_buffer, string_sz, frmt);                    \
    } while (0)
#endif
#if defined(__cplusplus)
}
#endif

#include <mutex>
namespace layout
{
    // This is only internal used, so no debug check, please make sure all is correct
    struct plane_layout
    {
        int N;
        int C;
        int H;
        int W;
        int data_size;
        int pitch_stride;
        int channel_stride;
        int batch_stride;
        #ifdef __linux__
        unsigned long long size;
        #else
        int size;
        #endif

        plane_layout()
            : N(0),
            C(0),
            H(0),
            W(0),
            data_size(0),
            pitch_stride(0),
            channel_stride(0),
            batch_stride(0),
            size(0) {};

        plane_layout(int n, int c, int h, int w, int Dsize = 1) : N(n), C(c), H(h), W(w), data_size(Dsize)
        {
            pitch_stride    = data_size * W;
            channel_stride  = pitch_stride * H;
            batch_stride    = channel_stride * C;
            size            = batch_stride * N;
        };
    };

    plane_layout align_width(plane_layout src, int align);
    plane_layout align_height(plane_layout src, int align);
    plane_layout align_channel(plane_layout src,
                               int          pitch_align,
                               int          height_align);
    plane_layout stride_width(plane_layout src, int stride);
    bm_status_t  update_memory_layout(bm_handle_t     handle,
                                      bm_device_mem_t src,
                                      plane_layout    src_layout,
                                      bm_device_mem_t dst,
                                      plane_layout    dst_layout);
}  // namespace layout

struct bm_image_private {
    bm_handle_t     handle = 0;
    bm_device_mem_t data[MAX_bm_image_CHANNEL];
    layout::plane_layout    memory_layout[MAX_bm_image_CHANNEL];
    int             plane_num            = 0;
    int             internal_alloc_plane = 0;
    std::mutex      memory_lock;
    bool            attached       = false;
    bool            data_owned     = false;
    bool            default_stride = true;
#ifndef USING_CMODEL
    BmJpuJPEGDecoder *decoder      = NULL;
#endif
};

#define MAX_SOFT_SUPPORT_PROPOSAL_NUM (1024)
#ifndef WIN32
typedef struct {
    int size;
    face_rect_t face_rect[MAX_PROPOSAL_NUM];
    int capacity;
    face_rect_t *begin;
    face_rect_t *end;
} __attribute__((packed)) nms_tpu_proposal_t;
#else
#pragma pack(push, 1)
typedef struct {
    int size;
    face_rect_t face_rect[MAX_PROPOSAL_NUM];
    int capacity;
    face_rect_t *begin;
    face_rect_t *end;
} nms_tpu_proposal_t;
#pragma pack(pop)
#endif

class image_warpper {
public:
    bm_image* inner;
    int image_num;
    image_warpper(bm_handle_t              handle,
                  int                      num,
                  int                      height,
                  int                      width,
                  bm_image_format_ext      image_format,
                  bm_image_data_format_ext data_type,
                  int *                    stride = nullptr);

    ~image_warpper();
};

bm_status_t bmcv_base64_codec(bm_handle_t     handle,
                    bm_device_mem_t src,
                    bm_device_mem_t dst,
                    unsigned long   len,
                    bool            direction
                    );

layout::plane_layout* bm_image_get_layout(bm_image input_image, int plane_idx);

///////////////////////////////////////////////////
static void str_to_image_format(bm_image_format_ext& format, const char* str)
{
    if(strcmp(str, "FORMAT_YUV420P") == 0)
    {
        format = FORMAT_YUV420P;
    }
    else if(strcmp(str, "FORMAT_YUV422P") == 0)
    {
        format = FORMAT_YUV422P;
    }
    else if(strcmp(str, "FORMAT_YUV444P") == 0)
    {
        format = FORMAT_YUV444P;
    }
    else if(strcmp(str, "FORMAT_NV12") == 0)
    {
        format = FORMAT_NV12;
    }
    else if(strcmp(str, "FORMAT_NV21") == 0)
    {
        format = FORMAT_NV21;
    }
    else if(strcmp(str, "FORMAT_NV16") == 0)
    {
        format = FORMAT_NV16;
    }
    else if(strcmp(str, "FORMAT_NV61") == 0)
    {
        format = FORMAT_NV61;
    }
    else if(strcmp(str, "FORMAT_NV24") == 0)
    {
        format = FORMAT_NV24;
    }
    else if(strcmp(str, "FORMAT_RGB_PLANAR") == 0)
    {
        format = FORMAT_RGB_PLANAR;
    }
    else if(strcmp(str, "FORMAT_BGR_PLANAR") == 0)
    {
        format = FORMAT_BGR_PLANAR;
    }
    else if(strcmp(str, "FORMAT_RGB_PACKED") == 0)
    {
        format = FORMAT_RGB_PACKED;
    }
    else if(strcmp(str, "FORMAT_BGR_PACKED") == 0)
    {
        format = FORMAT_BGR_PACKED;
    }
    else if(strcmp(str, "FORMAT_ARGB_PACKED") == 0)
    {
        format = FORMAT_ARGB_PACKED;
    }
    else if(strcmp(str, "FORMAT_ABGR_PACKED") == 0)
    {
        format = FORMAT_ABGR_PACKED;
    }
    else if(strcmp(str, "FORMAT_RGBP_SEPARATE") == 0)
    {
        format = FORMAT_RGBP_SEPARATE;
    }
    else if(strcmp(str, "FORMAT_BGRP_SEPARATE") == 0)
    {
        format = FORMAT_BGRP_SEPARATE;
    }
    else if(strcmp(str, "FORMAT_GRAY") == 0)
    {
        format = FORMAT_GRAY;
    }
    else if(strcmp(str, "FORMAT_COMPRESSED") == 0)
    {
        format = FORMAT_COMPRESSED;
    }
    else if(strcmp(str, "FORMAT_HSV_PLANAR") == 0)
    {
        format = FORMAT_HSV_PLANAR;
    }
    else
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "Not found such format%s: %s: %d\n",
            filename(__FILE__), __func__, __LINE__);
    }
}

static void data_type_to_str(bm_image_data_format_ext format, char* res)
{
    switch(format)
    {
        case DATA_TYPE_EXT_FLOAT32:
            strcpy(res, "DATA_TYPE_EXT_FLOAT32");
            break;
        case DATA_TYPE_EXT_1N_BYTE:
            strcpy(res, "DATA_TYPE_EXT_1N_BYTE");
            break;
        case DATA_TYPE_EXT_4N_BYTE:
            strcpy(res, "DATA_TYPE_EXT_4N_BYTE");
            break;
        case DATA_TYPE_EXT_1N_BYTE_SIGNED:
            strcpy(res, "DATA_TYPE_EXT_1N_BYTE_SIGNED");
            break;
        case DATA_TYPE_EXT_4N_BYTE_SIGNED:
            strcpy(res, "DATA_TYPE_EXT_4N_BYTE_SIGNED");
            break;
        case DATA_TYPE_EXT_FP16:
            strcpy(res, "DATA_TYPE_EXT_FP16");
            break;
        case DATA_TYPE_EXT_BF16:
            strcpy(res, "DATA_TYPE_EXT_BF16");
            break;
        default:
            bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "Not found such data type %s: %s: %d\n",
                  filename(__FILE__), __func__, __LINE__);
            break;
    }
}

static void str_to_data_type(bm_image_data_format_ext& format, const char* res)
{
    if(strcmp(res, "DATA_TYPE_EXT_FLOAT32") == 0)
    {
        format = DATA_TYPE_EXT_FLOAT32;
    }
    else if(strcmp(res, "DATA_TYPE_EXT_1N_BYTE") == 0)
    {
        format = DATA_TYPE_EXT_1N_BYTE;
    }
    else if(strcmp(res, "DATA_TYPE_EXT_4N_BYTE") == 0)
    {
        format = DATA_TYPE_EXT_4N_BYTE;
    }
    else if(strcmp(res, "DATA_TYPE_EXT_1N_BYTE_SIGNED") == 0)
    {
        format = DATA_TYPE_EXT_1N_BYTE_SIGNED;
    }
    else if(strcmp(res, "DATA_TYPE_EXT_4N_BYTE_SIGNED") == 0)
    {
        format = DATA_TYPE_EXT_4N_BYTE_SIGNED;
    }
    else if(strcmp(res, "DATA_TYPE_EXT_FP16") == 0)
    {
        format = DATA_TYPE_EXT_FP16;
    }
    else if(strcmp(res, "DATA_TYPE_EXT_BF16") == 0)
    {
        format = DATA_TYPE_EXT_BF16;
    }
    else
    {
        bmlib_log(BMCV_LOG_TAG, BMLIB_LOG_WARNING, "Not found such data type %s: %s: %d\n",
                filename(__FILE__), __func__, __LINE__);
    }
}

#endif /* BMCV_INTERNAL_H */

