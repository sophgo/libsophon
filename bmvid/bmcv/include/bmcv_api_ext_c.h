#ifndef BMCV_API_EXT_H
#define BMCV_API_EXT_H
#include "bmlib_runtime.h"
#ifdef _WIN32
#ifndef NULL
  #ifdef __cplusplus
    #define NULL 0
  #else
    #define NULL ((void *)0)
  #endif
#endif
#define DECL_EXPORT __declspec(dllexport)
#define DECL_IMPORT __declspec(dllimport)
#else
#define DECL_EXPORT
#define DECL_IMPORT
#endif

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * bmcv api with the new interface.
 */

#ifndef WIN32
/*
 * Face detection frame information
 * x1 y1 x2 y2 is top left and bottom right coordinates
 * score is confidence or score face detection
 */
typedef struct {
    float x1;
    float y1;
    float x2;
    float y2;
    float score;
} __attribute__((packed)) face_rect_t;
#else
#pragma pack(push, 1)
/*
 * Face detection frame information
 * x1 y1 x2 y2 is top left and bottom right coordinates
 * score is confidence or score face detection
 */
typedef struct {
    float x1;
    float y1;
    float x2;
    float y2;
    float score;
} face_rect_t;
#pragma pack(pop)
#endif

#define MIN_PROPOSAL_NUM (1)
#define MAX_PROPOSAL_NUM (40000) //(65535)

#ifndef WIN32
/*
 * The nms_proposal_t structure is used to store the results of the face detection algorithm
 * size is number used of fact_rect array
 * face_rect is detected facial rectangle information
 * capacity is maxinum of array element
 * begin and end are array first used position with last used position
 */
typedef struct nms_proposal {
    int          size;
    face_rect_t  face_rect[MAX_PROPOSAL_NUM];
    int          capacity;
    face_rect_t *begin;
    face_rect_t *end;
} __attribute__((packed)) nms_proposal_t;
#else
#pragma pack(push, 1)
/*
 * The nms_proposal_t structure is used to store the results of the face detection algorithm
 * size is number used of fact_rect array
 * face_rect is detected facial rectangle information
 * capacity is maxinum of array element
 * begin and end are array first used position with last used position
 */
typedef struct nms_proposal {
    int          size;
    face_rect_t  face_rect[MAX_PROPOSAL_NUM];
    int          capacity;
    face_rect_t *begin;
    face_rect_t *end;
} nms_proposal_t;
#pragma pack(pop)
#endif

#define MAX_RECT_NUM (8 * 1024)
#ifndef WIN32
/*
 * fact_rect is array of detected facial rectangle
 * size is number used of fact_rect array
 * capacity is maxinum of array element
 * begin and end are array first used position with last used position
 */
typedef struct {
    face_rect_t  face_rect[MAX_RECT_NUM];
    int          size;
    int          capacity;
    face_rect_t *begin;
    face_rect_t *end;
} __attribute__((packed)) m_proposal_t;
#else
#pragma pack(push, 1)
/*
 * fact_rect is array of detected facial rectangle
 * size is number used of fact_rect array
 * capacity is maxinum of array element
 * begin and end are array first used position with last used position
 */
typedef struct {
    face_rect_t  face_rect[MAX_RECT_NUM];
    int          size;
    int          capacity;
    face_rect_t *begin;
    face_rect_t *end;
} m_proposal_t;
#pragma pack(pop)
#endif

typedef enum {
    LINEAR_WEIGHTING = 0,
    GAUSSIAN_WEIGHTING,
    MAX_WEIGHTING_TYPE
} weighting_method_e;

/*
 * Apply ip of the memory heap
 * BMCV_HEAP_ANY is any ip
 */
typedef enum bmcv_heap_id_ {
    BMCV_HEAP0_ID = 0,
    BMCV_HEAP1_ID = 1,
    BMCV_HEAP_ANY
} bmcv_heap_id;

// BMCV_IMAGE_FOR_IN and BMCV_IMAGE_FOR_OUT may be deprecated in future version.
// We recommend not use this.
#define BMCV_IMAGE_FOR_IN BMCV_HEAP1_ID
#define BMCV_IMAGE_FOR_OUT BMCV_HEAP0_ID

typedef enum bm_image_data_format_ext_ {
    DATA_TYPE_EXT_FLOAT32,
    DATA_TYPE_EXT_1N_BYTE,
    DATA_TYPE_EXT_4N_BYTE,
    DATA_TYPE_EXT_1N_BYTE_SIGNED,
    DATA_TYPE_EXT_4N_BYTE_SIGNED,
    DATA_TYPE_EXT_FP16,
    DATA_TYPE_EXT_BF16,
} bm_image_data_format_ext;

/*
* The image format represents the type of storage of the data,in enumeration order
* 1.YUV420 with 3 planes; 2.YUV422 with 3 planes; 3.YUV444 with 3 planes; 4.YUV420 Y plane and UV interleaving storage
* 5.YUV420 Y plane and VU interleaving storage; 6.YUV422 Y plane and UV interleaving storage; 7.YUV422 Y plane and VU interleaving storage
* 8.YUV444 Y plane UV interleaving storage; 9.RGB three planes; 10.BGR three planes; 11.RGB interleaved with a plane; 12.BGR interleaved with a plane
* 13.RGB 3 planes; 14.BGR 3 planes; 15.only Y plane; 16.compression format four planes with Y table Y data, cbcr table cbcr data
* 17.HSV 3 planes,H~[0,180]; 18.ARGB interleaved with a plane; 19.ARGB interleaved with a plane; 20.YUV staggered with a plane
* 21.YVU staggered with a plane; 22. YUYV staggered with a plane; 23.YVYU staggered with a plane; 24.UYVY staggered with a plane
* 25.VYUY interleaved with a plane; 26.RGBY with 4 planes; 27.HSV with a plane, H~[0,180]; 28.HSV with a plane, H~[0,255]
* 29.sensor data format,Each pixel about one color channel; 30.red-green mode with 8-bit depth;
*/
typedef enum bm_image_format_ext_ {
    FORMAT_YUV420P,
    FORMAT_YUV422P,
    FORMAT_YUV444P,
    FORMAT_NV12,
    FORMAT_NV21,
    FORMAT_NV16,
    FORMAT_NV61,
    FORMAT_NV24,
    FORMAT_RGB_PLANAR,
    FORMAT_BGR_PLANAR,
    FORMAT_RGB_PACKED,
    FORMAT_BGR_PACKED,
    FORMAT_RGBP_SEPARATE,
    FORMAT_BGRP_SEPARATE,
    FORMAT_GRAY,
    FORMAT_COMPRESSED,
    FORMAT_HSV_PLANAR,
    FORMAT_ARGB_PACKED,
    FORMAT_ABGR_PACKED,
    FORMAT_YUV444_PACKED,
    FORMAT_YVU444_PACKED,
    FORMAT_YUV422_YUYV,
    FORMAT_YUV422_YVYU,
    FORMAT_YUV422_UYVY,
    FORMAT_YUV422_VYUY,
    FORMAT_RGBYP_PLANAR,
    FORMAT_HSV180_PACKED,
    FORMAT_HSV256_PACKED,
    FORMAT_BAYER,
    FORMAT_BAYER_RG8,
    FORMAT_ARGB4444_PACKED,
    FORMAT_ABGR4444_PACKED,
    FORMAT_ARGB1555_PACKED,
    FORMAT_ABGR1555_PACKED,
} bm_image_format_ext;

/*
* resize algorithm
* 1.nearest neighbor interpolation model; 2.bilinear interpolation scaling mode
* 3.bicubic interpolation mode; 4.area interpolation scaling mode
*/
typedef enum bmcv_resize_algorithm_ {
    BMCV_INTER_NEAREST = 0,
    BMCV_INTER_LINEAR  = 1,
    BMCV_INTER_BICUBIC = 2
} bmcv_resize_algorithm;

/*
* non-maximum suppression type
* 1.HARD_NMS is hard non-maximum suppression; 2.SOFT_NMS is soft non-maximum suppression
* 3.ADAPTIVE_NMS is adaptive non-maximum suppression; 4.SSD_NMS is Single Shot Multibox Detector with NMS algorithm
* 5.NMS types are used as loop boundaries
*/
typedef enum bm_cv_nms_alg_ {
    HARD_NMS = 0,
    SOFT_NMS,
    ADAPTIVE_NMS,
    SSD_NMS,
    MAX_NMS_TYPE
} bm_cv_nms_alg_e;

struct bm_image_private;

/*
* Create a struct type corresponding to bm_image,Members are as follows:
* 1.image width; 2.image height; 3.Color format of the image; 4.Data format of the image
* 5.Private data of the image
*/
typedef struct bm_image_s {
    int                      width;
    int                      height;
    bm_image_format_ext      image_format;
    bm_image_data_format_ext data_type;
    struct bm_image_private *       image_private;
}bm_image;

/*
* each crop output image information
* 1.top left horizontal coordinate; 2.top left ordinate
* 3.width of the crop area; 4.height of the crop area
*/
typedef struct bmcv_rect {
    int start_x;
    int start_y;
    int crop_w;
    int crop_h;
} bmcv_rect_t;

#ifndef WIN32
typedef struct yolov7_info{
  int scale;
  int *orig_image_shape;
  int model_h;
  int model_w;
} __attribute__((packed)) yolov7_info_t;
#else
#pragma pack(push, 1)
typedef struct yolov7_info{
  int scale;
  int *orig_image_shape;
  int model_h;
  int model_w;
} yolov7_info_t;
#pragma pack(pop)
#endif

typedef struct
{
	//! Coordinates of the detected interest point
	float x, y;
	//! Detected scale
	float scale;
	//! Orientation measured anti-clockwise from +ve x-axis
	float orientation;
	//! Sign of laplacian for fast matching purposes
	unsigned char laplacian;
	//! Array of descriptor components
	float descriptor[64]; //float descriptor[128];
	float dx, dy;
	//! Used to store cluster index
	int clusterIndex;
} Ipoint;

typedef struct
{
	int width;
	int	height;
	int	step;
	int	filter;
	float *responses;
	unsigned char *laplacian;
} ResponseLayer;

typedef struct
{
	float *img;
	int i_width;
	int i_height;
	Ipoint *ipts;
	int iptssize;
	ResponseLayer responseMap[12];
	int octaves;
	int init_sample;
	float thresh;
	int interestPtsLen;
} FastHessian;

typedef struct
{
    float *integralImg;
    FastHessian *hessians;
} SURFDescriptor;

/*
* Copy the information of the image
* 1.start_x and start_y are copy the starting coordinate information to the output image
* 2.padding_r padding_g and padding_b are the channel filled value When the input image is smaller than the output image.
* 3.if need not paddding, setting if_padding is 0
*/
typedef struct bmcv_copy_to_atrr_s {
    int           start_x;
    int           start_y;
    unsigned char padding_r;
    unsigned char padding_g;
    unsigned char padding_b;
    int           if_padding;
} bmcv_copy_to_atrr_t;

typedef struct bmcv_padding_atrr_s {
    unsigned int  dst_crop_stx;
    unsigned int  dst_crop_sty;
    unsigned int  dst_crop_w;
    unsigned int  dst_crop_h;
    unsigned char padding_r;
    unsigned char padding_g;
    unsigned char padding_b;
    int           if_memset;
} bmcv_padding_atrr_t;

/*
* A data structure that holds the required information,Members are as follows:
* 1.number plane of image; 2.per plane device memory; 3.per plane stride; 4.image width; 5.image height
* 6.image format; 7.data type of the image;
*/
typedef struct bm_image_format_info {
    int                      plane_nb;
    bm_device_mem_t          plane_data[8];
    int                      stride[8];
    int                      width;
    int                      height;
    bm_image_format_ext      image_format;
    bm_image_data_format_ext data_type;
    bool                     default_stride;
} bm_image_format_info_t;

/*
* The starting coordinate information corresponding to the line point
*/
typedef struct {
    int x;
    int y;
} bmcv_point_t;

/*
* The starting coordinate information corresponding to the line point
*/
typedef struct {
    float x;
    float y;
} bmcv_point2f_t;

typedef struct {
    int type;   // 1: maxCount   2: eps   3: both
    int max_count;
    double epsilon;
} bmcv_term_criteria_t;

/*
* the value of drawing line with each channel value, with r g b channels
*/
typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} bmcv_color_t;

typedef struct {
    int csc_coe00;
    int csc_coe01;
    int csc_coe02;
    int csc_add0;
    int csc_coe10;
    int csc_coe11;
    int csc_coe12;
    int csc_add1;
    int csc_coe20;
    int csc_coe21;
    int csc_coe22;
    int csc_add2;
} csc_matrix_t;

/*
* Define the color space conversion control mode. in enum order
* 1.YUV2RGB image used BT601; 2.YUV2RGB video used BT601; 3.RGB2YUV image used BT601; 4.YUV2RGB image used BT709
* 5.RGB2YUV image used BT709; 6.RGB2YUV video used BT601; 7.YUV2RGB video used BT709; 8.RGB2YUV video used BT709
* 9.custom color space conversion matrix can be used;
* 10.determine if the input is in range
*/
typedef enum csc_type {
    CSC_YCbCr2RGB_BT601 = 0,
    CSC_YPbPr2RGB_BT601,
    CSC_RGB2YCbCr_BT601,
    CSC_YCbCr2RGB_BT709,
    CSC_RGB2YCbCr_BT709,
    CSC_RGB2YPbPr_BT601,
    CSC_YPbPr2RGB_BT709,
    CSC_RGB2YPbPr_BT709,
    CSC_USER_DEFINED_MATRIX = 1000,
    CSC_MAX_ENUM
} csc_type_t;

/*
* Threshold type
* 1.BM_THRESH_BINARY is if pixel > threshold, pixel=maximum; otherwise set to 0;
* 2.BM_THRESH_BINARY_INV if pixel > threshold, pixel=0; otherwise set to maximum;
* 3.BM_THRESH_TRUNC if pixel > threshold, pixel=threshold, otherwise keep
* 4.BM_THRESH_TOZERO_INV if pixel > threshold, pixel=0, otherwise keep
* 5.BM_THRESH_TYPE_MAX is the number of valid threshold types in the enumeration
*/
typedef enum {
    BM_THRESH_BINARY = 0,
    BM_THRESH_BINARY_INV,
    BM_THRESH_TRUNC,
    BM_THRESH_TOZERO,
    BM_THRESH_TOZERO_INV,
    BM_THRESH_TYPE_MAX
} bm_thresh_type_t;

typedef enum {
    BM_MORPH_RECT,
    BM_MORPH_CROSS,
    BM_MORPH_ELLIPSE
} bmcv_morph_shape_t;

DECL_EXPORT const char *bm_get_bmcv_version();

/**
 * @brief Update the bm_image structure with image information.
 *
 * This function updates the bm_image structure with the provided image information,
 * including width, height, image format, data type, memory_layout and handle. It performs error
 * checks on the image format and size, as well as the stride, before updating the
 * image_private handle with the given handle.
 *
 * @param handle The handle associated with the image.
 * @param img_h The height of the image.
 * @param img_w The width of the image.
 * @param image_format The image format.
 * @param data_type The data type of the image.
 * @param res Pointer to the bm_image structure to be updated.
 * @param stride Pointer to the stride value.
 * @return BM_SUCCESS if the update is successful, otherwise an error code.
 */
DECL_EXPORT bm_status_t bm_image_update(bm_handle_t handle,
                            int img_h,
                            int img_w,
                            bm_image_format_ext image_format,
                            bm_image_data_format_ext data_type,
                            bm_image *res,
                            int *stride);

/**
 * @brief Get the size of struct bm_image_private in bytes.
 *
 * This function calculates and returns the size in bytes of the struct bm_image_private.
 *
 * @return The size of struct bm_image_private in bytes.
 */

DECL_EXPORT size_t bmcv_get_private_size(void);

/** bm_image_create
 * @brief Create and fill bm_image structure
 * @param [in] handle   The bm handle which return by bm_dev_request
 * @param [in] img_h    The height or rows of the creating image
 * @param [in] img_w    The width or cols of the creating image.
 * @param [in] image_format  The image_format of the creating image,
 *  please choose one from bm_image_format_ext enum.
 * @param [in] data_type The data_type of the creating image,
 *  be caution that not all combinations between image_format and data_type
 *  are supported.
 * @param [in] stride    the stride array for planes, each
 * number in array means corresponding plane pitch stride in bytes. The plane
 * size is determinated by image_format. If this array is null, we may use
 * default value.
 * @param [in] bm_private   A pointer to the bm_image_private structure to
 * be initialized.
 * @param [out] res       The filled bm_image structure.
 * For example, we need create a 480x480 NV12 format image, we know that NV12
 * format has 2 planes, we need pitch stride is 256 aligned(just for example) so
 * the pitch stride for the first plane is 512, so as the same for the second
 * plane.
 * The call may as following
 * bm_image res;
 *  int stride[] = {512, 512};
 *  bm_image_create_private(handle, 480, 480, FORMAT_NV12, DATA_TYPE_EXT_1N_BYTE, &res,
 * stride, bm_private); If bm_image_create return BM_SUCCESS, res is created successfully.
 */
DECL_EXPORT bm_status_t bmcv_image_surf_response(
        bm_handle_t handle,
        float* img_data,
        SURFDescriptor *surf,
        int width,
        int height,
        int layer_num);

DECL_EXPORT bm_status_t bm_image_create_private(bm_handle_t              handle,
                            int                      img_h,
                            int                      img_w,
                            bm_image_format_ext      image_format,
                            bm_image_data_format_ext data_type,
                            bm_image *               res,
                            int *                    stride,
                            void*                    bm_private);
/** bm_image_create
 * @brief Create and fill bm_image structure
 * @param [in] handle                     The bm handle which return by
 * bm_dev_request.
 * @param [in] img_h                      The height or rows of the creating
 * image.
 * @param [in] img_w                     The width or cols of the creating
 * image.
 * @param [in] image_format      The image_format of the creating image,
 *  please choose one from bm_image_format_ext enum.
 * @param [in] data_type               The data_type of the creating image,
 *  be caution that not all combinations between image_format and data_type
 *  are supported.
 * @param [in] stride                        the stride array for planes, each
 * number in array means corresponding plane pitch stride in bytes. The plane
 * size is determinated by image_format. If this array is null, we may use
 * default value.
 *  @param [out] image                   The filled bm_image structure.
 *  For example, we need create a 480x480 NV12 format image, we know that NV12
 * format has 2 planes, we need pitch stride is 256 aligned(just for example) so
 * the pitch stride for the first plane is 512, so as the same for the second
 * plane.
 * The call may as following
 * bm_image res;
 *  int stride[] = {512, 512};
 *  bm_image_create(handle, 480, 480, FORMAT_NV12, DATA_TYPE_EXT_1N_BYTE, &res,
 * stride); If bm_image_create return BM_SUCCESS, res is created successfully.
 */
DECL_EXPORT bm_status_t bm_image_create(bm_handle_t              handle,
                            int                      img_h,
                            int                      img_w,
                            bm_image_format_ext      image_format,
                            bm_image_data_format_ext data_type,
                            bm_image *        image,
                            int *                    stride);

/** bm_image_destroy
 * @brief Destroy bm_image and free the corresponding system memory and device
 * memory.
 * @param [in] image                     The bm_image structure ready to
 * destroy. If bm_image_destroy return BM_SUCCESS, image is destroy successfully
 * and the corresponding system memory and device memory are freed.
 */
DECL_EXPORT bm_status_t bm_image_destroy(bm_image image);

/** bm_image_get_handle
 * @brief return the device handle, this handle is exactly the first parameter
 * when bm_image_create called.
 * @param [in] image                                   The bm_image structure
 *  @param [return] bm_handle_t          The device handle where bm_image bind
 * to. If image is not created by bm_image_create, this function would return
 * NULL.
 */
DECL_EXPORT bm_handle_t bm_image_get_handle(bm_image *image);

/** bm_image_write_to_bmp
 * @brief dump this bm_image to .bmp file.
 * @param [in] image                 The bm_image structure you would like to
 * dump
 *  @param [in] filename           path and filename for the creating bmp file,
 * it's better end with ".bmp" If bm_image_write_to_bmp return BM_SUCCESS, a
 * .bmp file is create in the path filename point to.
 */
DECL_EXPORT bm_status_t bm_image_write_to_bmp(bm_image    image,
                                             const char *filename);

/*
 * The API copies the host data to the device memory corresponding to the bm_image structure
 */
DECL_EXPORT bm_status_t bm_image_copy_host_to_device(bm_image image,
                                                    void *   buffers[]);
/*
 * The API copies the device data to the host memory corresponding to buffers
 */
DECL_EXPORT bm_status_t bm_image_copy_device_to_host(bm_image image,
                                                     void *   buffers[]);

/*
 * If the user wants to manage the device memory themselves
 * or if the device memory is generated by an external component (VPU/VPP, etc.)
 * the following API can be called to associate the device memory with bm_image
 */
DECL_EXPORT bm_status_t bm_image_attach(bm_image image, bm_device_mem_t *device_memory);
/*
 * This API is used to disassociate device memory from bm_image
 */
DECL_EXPORT bm_status_t bm_image_detach(bm_image);
/*
 * This interface is used to determine whether the target has attached storage space
 * The memory requested by bm_image calling bm_image_alloc_dev_mem is automatically managed internally.
 * It is automatically released when bm_image_destroy, bm_image_detach,
 * or bm_image_attach other device memory is called. In contrast,
 * if bm_image_attach a device memory, that memory will be managed by the caller.
 * bm_image_destroy, bm_image_detach, or call bm_image_attach to other device memory will not be released,
 * and the caller needs to release it manually.
 */
DECL_EXPORT bool        bm_image_is_attached(bm_image);
/*
 * This interface is used to obtain the number of planes of the target bm_image
 */
DECL_EXPORT int         bm_image_get_plane_num(bm_image);
/*
 * This interface is used to obtain the stride information of the target bm_image
 */
DECL_EXPORT bm_status_t bm_image_get_stride(bm_image image, int *stride);
/* This interface is used to get some information about bm_image */
DECL_EXPORT bm_status_t bm_image_get_format_info(bm_image *            image,
                                     struct bm_image_format_info *info);
/* The API applies for internal management memory for bm_image object.
  The requested device memory size is the sum of the device memory size required by each plane.
  The plane_byte_size calculation method is described in bm_image_copy_host_to_device,
  or confirmed by calling the bm_image_get_byte_size API.*/
DECL_EXPORT bm_status_t bm_image_alloc_dev_mem(bm_image image,
                                              int      heap_id);
DECL_EXPORT bm_status_t bm_image_alloc_dev_mem_heap_mask(bm_image image, int heap_mask);
/* Gets the size of each plane byte of the bm_image object */
DECL_EXPORT bm_status_t bm_image_get_byte_size(bm_image image, int *size);

/* Gets the device memory of each plane of the bm_image object */
DECL_EXPORT bm_status_t bm_image_get_device_mem(bm_image image, bm_device_mem_t *mem);

/* Allocate contiguous memory for multiple images */
DECL_EXPORT bm_status_t bm_image_alloc_contiguous_mem(int       image_num,
                                          bm_image *images,
                                          int       heap_id);
/* Allocates contiguous memory on the specified heap for multiple images */
DECL_EXPORT bm_status_t bm_image_alloc_contiguous_mem_heap_mask(int       image_num,
                                                    bm_image *images,
                                                    int       heap_mask);
/* Frees contiguous memory in multiple images assigned by bm_image_alloc_contiguous_mem */
DECL_EXPORT bm_status_t bm_image_free_contiguous_mem(int image_num, bm_image *images);
/* attach a contiguous memory to multiple images */
DECL_EXPORT bm_status_t bm_image_attach_contiguous_mem(int             image_num,
                                           bm_image *      images,
                                           bm_device_mem_t dmem);
/* detach a contiguous block of memory from multiple images */
DECL_EXPORT bm_status_t bm_image_dettach_contiguous_mem(int image_num, bm_image *images);

/* Obtain device memory information of consecutive memory from successive images of multiple memory */
DECL_EXPORT bm_status_t bm_image_get_contiguous_device_mem(int              image_num,
                                               bm_image *       images,
                                               bm_device_mem_t *mem);

DECL_EXPORT bm_status_t bmcv_image_yuv2bgr_ext(bm_handle_t handle,
                                   int         image_num,
                                   bm_image *  input,
                                   bm_image *  output);

DECL_EXPORT bm_status_t bmcv_image_yuv2hsv(bm_handle_t handle,
                               bmcv_rect_t rect,
                               bm_image    input,
                               bm_image    output);

/*
* Interface parameter description:
* image_num is input or output image; input_ is input bm_image; output_ is output image pointer;
*/
DECL_EXPORT bm_status_t bmcv_image_storage_convert(bm_handle_t handle,
                                       int         image_num,
                                       bm_image *  input,
                                       bm_image *  output);

/*
* Interface parameter description:
* image_num is image number; input_ is input image pointer; output_ is output image pointer;
* csc_type is csc type
*/
DECL_EXPORT bm_status_t bmcv_image_storage_convert_with_csctype(bm_handle_t handle,
                                                    int         image_num,
                                                    bm_image *  input,
                                                    bm_image *  output,
                                                    csc_type_t  csc_type);

/*
* The interface implements a copy of an image to the corresponding memory area of the destination image
* copy_to_attr is attribute configuration; input is input bm_image; output is output bm_image
*/
DECL_EXPORT bm_status_t bmcv_image_copy_to(bm_handle_t         handle,
                               bmcv_copy_to_atrr_t copy_to_attr,
                               bm_image            input,
                               bm_image            output);
DECL_EXPORT bm_status_t bmcv_image_crop(bm_handle_t         handle,
                            int                 crop_num,
                            bmcv_rect_t *       rects,
                            bm_image            input,
                            bm_image *          output);
DECL_EXPORT bm_status_t bmcv_image_split(bm_handle_t         handle,
                             bm_image            input,
                             bm_image *          output);

/*
* Affine transformation matrix data
*/
typedef struct bmcv_affine_matrix_s {
    float m[6];
} bmcv_affine_matrix;

/*
* 1.matrix is the affine transformation matrix struct pointer
* 2.matrix_num is affine transformation matrix numbers
*/
typedef struct bmcv_affine_image_matrix_s {
    bmcv_affine_matrix *matrix;
    int                 matrix_num;
} bmcv_affine_image_matrix;

/*
* Perspective transformation matrix data
*/
typedef struct bmcv_perspective_matrix_s {
    float m[9];
} bmcv_perspective_matrix;

/*
* 1.matrix is perspective transformation matrix pointers
* 2.matrix_num is perspective transformation matrix numbers
*/
typedef struct bmcv_perspective_image_matrix_s {
    bmcv_perspective_matrix *matrix;
    int                      matrix_num;
} bmcv_perspective_image_matrix;

/*
* x and y array stores the coordinate information of the points
*/
typedef struct bmcv_perspective_coordinate_s {
    int x[4];
    int y[4];
} bmcv_perspective_coordinate;

/*
* 1.coordinate is the coordinate point after perspective transformation
* 2.coordinate_num is the number of coordinate points in a coordinate point group
*/
typedef struct bmcv_perspective_image_coordinate_s {
    bmcv_perspective_coordinate *coordinate;
    int                         coordinate_num;
} bmcv_perspective_image_coordinate;

/*
* Specific properties and parameters required to perform an image scaling operation
* 1.start_x start_y is starting coordinates of the scaling operation
* 2.in_width in_height is src or roi image width and height
* 3.out_width out_height id output image width and height
*/
typedef struct bmcv_resize_s {
    int start_x;
    int start_y;
    int in_width;
    int in_height;
    int out_width;
    int out_height;
} bmcv_resize_t;

/*
* resize image need the attr infor
* 1.resize_img_attr is pointer to the struct of type bmcv_resize_t
* 2.roi_num is num of interesting areas; 3.stretch_fit is if the image should be stretched to fit the target size
* 3.padding_b padding_g padding_r are each channel padding with the value
* 4.interpolation is witch interpolation method will be used
*/
typedef struct bmcv_resize_image_s {
    bmcv_resize_t *resize_img_attr;
    int            roi_num;
    unsigned char  stretch_fit;
    unsigned char  padding_b;
    unsigned char  padding_g;
    unsigned char  padding_r;
    unsigned int   interpolation;
} bmcv_resize_image;

/*
* each image config param
* 1.channel 0 linear transformation coefficient. 2.channel 0 linear transformation offset
* 3.channel 1 linear transformation coefficient. 4.channel 1 linear transformation offset
* 5.channel 2 linear transformation coefficient. 6.channel 2 linear transformation offset
*/
typedef struct bmcv_convert_to_attr_s {
    float alpha_0;
    float beta_0;
    float alpha_1;
    float beta_1;
    float alpha_2;
    float beta_2;
} bmcv_convert_to_attr;

/**
 * @brief Do warp affine operation with the transform matrix.
 *        For 1N mode, only support 4 images.
 *        For 4N mode, only support 1 images.
 * @param [in] handle        The bm handle which return by bm_dev_request.
 * @param [in] image_num    The really input image number, should be less than
 *or equal to 4.
 * @param [in] matrix        The input transform matrix and matrix number for
 *each image.
 * @param [in] input        The input bm image, could be 1N or 4N.
 *                for each image. And do any operation if matrix[n] is nullptr.
 * @param [out]            The output image, could be 1N or 4N.
 *                If setting to 1N, the output image number should have summary
 *of matrix_num[n]. If setting to 4N, the output image number should have
 *summary of ROUNDUP(matrix_num[n], 4)/4
 */
DECL_EXPORT bm_status_t bmcv_image_warp_affine(
        bm_handle_t              handle,
        int                      image_num,
        bmcv_affine_image_matrix matrix[4],
        bm_image *               input,
        bm_image *               output,
        int                      use_bilinear);
DECL_EXPORT bm_status_t bmcv_image_warp_affine_padding(
        bm_handle_t              handle,
        int                      image_num,
        bmcv_affine_image_matrix matrix[4],
        bm_image *               input,
        bm_image *               output,
        int                      use_bilinear);
/**
 * Image affine transformation can realize rotation, translation, scaling and other operations
 * image_num is image num;matrix is transformation matrix data structure corresponding to each image
 * input is src image;output is dst image;use_bilinear is if to use bilinear for interpolation
*/
DECL_EXPORT bm_status_t bmcv_image_warp_affine_similar_to_opencv(
        bm_handle_t              handle,
        int                      image_num,
        bmcv_affine_image_matrix matrix[4],
        bm_image *               input,
        bm_image *               output,
        int                      use_bilinear);
DECL_EXPORT bm_status_t bmcv_image_warp_affine_similar_to_opencv_padding(
        bm_handle_t              handle,
        int                      image_num,
        bmcv_affine_image_matrix matrix[4],
        bm_image *               input,
        bm_image *               output,
        int                      use_bilinear);
/**
 * The interface realizes transmission transformation of image
 * image_num is image num;matrix is transformation matrix data structure corresponding to each image
 * input is src image;output is dst image;use_bilinear is if to use bilinear for interpolation
*/
DECL_EXPORT bm_status_t bmcv_image_warp_perspective(
        bm_handle_t                   handle,
        int                           image_num,
        bmcv_perspective_image_matrix matrix[4],
        bm_image *                    input,
        bm_image *                    output,
        int                           use_bilinear);

/**
 * The interface realizes transmission transformation of image
 * image_num is image num;matrix is transformation matrix data structure corresponding to each image
 * input is src image;output is dst image;use_bilinear is if to use bilinear for interpolation
*/
DECL_EXPORT bm_status_t bmcv_image_warp_perspective_with_coordinate(
        bm_handle_t                       handle,
        int                               image_num,
        bmcv_perspective_image_coordinate coordinate[4],
        bm_image *                        input,
        bm_image *                        output,
        int                               use_bilinear);

/**
 * The interface realizes transmission transformation of image
 * image_num is image num;matrix is transformation matrix data structure corresponding to each image
 * input is src image;output is dst image;use_bilinear is if to use bilinear for interpolation
*/
DECL_EXPORT bm_status_t bmcv_image_warp_perspective_similar_to_opencv(
    bm_handle_t                       handle,
    int                               image_num,
    bmcv_perspective_image_matrix     matrix[4],
    bm_image *                        input,
    bm_image *                        output,
    int                               use_bilinear);

/*
 * The interface realizes remapping transformation of image
 * based on OpenCV remap operator:
 * for each dst pixel (x, y), it looks up src coordinates
 * (mapx[x,y], mapy[x,y]) and fetches the value with the
 * specified interpolation.
 * input                    is src image;
 * output                   is dst image;
 * mapx_data_global_addr    is device memory address for x-coordinate map (float32);
 * mapy_data_global_addr    is device memory address for y-coordinate map (float32);
 * interpolation_mode       is interpolation method: 0 - nearest; 1 - bilinear.
 */
DECL_EXPORT bm_status_t bmcv_image_remap(
    bm_handle_t handle,
    bm_image input,
    bm_image output,
    bm_device_mem_t mapx_data_global_addr,
    bm_device_mem_t mapy_data_global_addr,
    int interpolation_mode);

/**
 * Image size changes, such as zoom in, zoom out, matting and other functions.
 * input_num is src num;resize_attr is resize parameter for each image
 * input is src image;output is resize dst image
*/
DECL_EXPORT bm_status_t bmcv_image_resize(
        bm_handle_t          handle,
        int                  input_num,
        bmcv_resize_image    resize_attr[],
        bm_image *           input,
        bm_image *           output);

/*
* Hamming distance of each element in two vectors
* input1 is Address information for vector 1 data; intput2 is Address information for vector 2 data
* output is output Indicates the address of the vector data;bits_len is length of each vector element
* input1_num is number of data in vector 1;input2_num is number of data in vector 2;
*/
DECL_EXPORT bm_status_t bmcv_hamming_distance(bm_handle_t handle,
                                  bm_device_mem_t input1,
                                  bm_device_mem_t input2,
                                  bm_device_mem_t output,
                                  int bits_len,
                                  int input1_num,
                                  int input2_num);

/**
 * input_num is src image number
 * input is src image sturct pointer
 * output is dst image struct pointer
*/
DECL_EXPORT bm_status_t bmcv_image_yuv_resize(
                              bm_handle_t       handle,
                              int               input_num,
                              bm_image *        input,
                              bm_image *        output);

/*
* The interface realize the linear change of image pixels;Parameters are as follows
* handle; input_num is input image num,if input_num > 1，it needs continuous data storage
* convert_to_attr is config parameters for each image; input is input bm_image; output is output bm_image
*/
DECL_EXPORT bm_status_t bmcv_image_convert_to(
        bm_handle_t          handle,
        int                  input_num,
        bmcv_convert_to_attr convert_to_attr,
        bm_image *           input,
        bm_image *           output);

/* input image Width align for output image */
DECL_EXPORT bm_status_t bmcv_width_align(
        bm_handle_t handle,
        bm_image    input,
        bm_image    output);

/*
* JPEG encoding process for multiple BM_images
* image_num is input image num;src is input image;p_jpeg_data is output image;
* out_size is size of the output picture (in bytes);quality_factor is quality factor of the encoded image
* bs_in_device is pcie use
*/
DECL_EXPORT bm_status_t bmcv_image_jpeg_enc(
        bm_handle_t handle,
        int         image_num,
        bm_image *  src,
        void **     p_jpeg_data,
        size_t *    out_size,
        int         quality_factor,
        int         bs_in_device);
/*
* JPEG decoding of multiple images
* p_jpeg_data is pointer to picture data to be decoded;in_size is size (in byte) of each image to be decoded;
* image_num is input image num,num < 4;dst is pointer to bm_image
*/
DECL_EXPORT bm_status_t bmcv_image_jpeg_dec(
        bm_handle_t handle,
        void **     p_jpeg_data,
        size_t *    in_size,
        int         image_num,
        bm_image *  dst,
        int         bs_in_device);

/**
 * Eliminate network calculations to get too many object boxes and find the best object box.
 * input_proposal_addr is Enter the address of the object box data;
 * proposal_size is Number of object frames;nms_threshold is Filter the threshold of the object frame
 * output_proposal_addr is Output the address of the object enclosure data
*/
DECL_EXPORT bm_status_t bmcv_nms(
        bm_handle_t     handle,
        bm_device_mem_t input_proposal_addr,
        int             proposal_size,
        float           nms_threshold,
        bm_device_mem_t output_proposal_addr);

/**
 * bmcv_nms Specifies the generalized form of an interface that supports Hard_NMS/Soft_NMS/Adaptive_NMS/SSD_NMS
 * input_proposal_addr is Enter the address of the object box data;
 * proposal_size is Number of object frames;nms_threshold is Filter the threshold of the object frame
 * output_proposal_addr is Output the address of the object enclosure data
 * topk is Ports reserved for possible future expansion
 * score_threshold is Lowest score threshold;nms_alg is The choice of different NMS algorithms
 * sigma is Parameter of the Gaussian re-score function if Soft_NMS or Adaptive_NMS is used
 * weighting_method is Linear and Gaussian weights;densities is Adaptive-NMS density value
 * eta is SSD-NMS coefficient is used to adjust the iou threshold
*/
DECL_EXPORT bm_status_t bmcv_nms_ext(
        bm_handle_t     handle,
        bm_device_mem_t input_proposal_addr,
        int             proposal_size,
        float           nms_threshold,
        bm_device_mem_t output_proposal_addr,
        int             topk,
        float           score_threshold,
        int             nms_alg,
        float           sigma,
        int             weighting_method,
        float         * densities,
        float           eta);

#define BMCV_YOLOV3_DETECT_OUT_MAX_NUM 200
DECL_EXPORT bm_status_t bmcv_nms_yolov3(bm_handle_t      handle,
        //input
        int              input_num,
        bm_device_mem_t  bottom[3],
        int              batch_num,
        int              hw[3][2],
        int              num_classes,
        int              num_boxes,
        int              mask_group_size,
        float            nms_threshold,
        float            confidence_threshold,
        int              keep_top_k,
        float            bias[18],
        float            anchor_scale[3],
        float            mask[9],
        bm_device_mem_t  output,
        int              yolov5_flag,
        int              len_per_batch);

DECL_EXPORT bm_status_t bmcv_nms_yolo(
        bm_handle_t handle,
        int input_num,
        bm_device_mem_t bottom[3],
        int batch_num,
        int hw_shape[3][2],
        int num_classes,
        int num_boxes,
        int mask_group_size,
        float nms_threshold,
        float confidence_threshold,
        int keep_top_k,
        float bias[18],
        float anchor_scale[3],
        float mask[9],
        bm_device_mem_t output,
        int yolov5_flag,
        int len_per_batch,
        void *ext);

/*
* Interface parameter description:
* image is which you want to draw a rectangular box
* rect_num is number of rectangles;rects is rectangle box object pointer;
* line_width is rectangular frame line width; r,g, and b are the rectangular box colors
*/
DECL_EXPORT bm_status_t bmcv_image_draw_rectangle(
        bm_handle_t   handle,
        bm_image      image,
        int           rect_num,
        bmcv_rect_t * rect,
        int           line_width,
        unsigned char r,
        unsigned char g,
        unsigned char b);
/*
* The interface image is filled with one or more rectangles
* image needs to be drawn as a rectangular box;rect_num is number of rectangles
* rects is an output pointer containing a rectangular starting point and width and height
* Rectangle color of r,g,b
*/
DECL_EXPORT bm_status_t bmcv_image_fill_rectangle(
        bm_handle_t   handle,
        bm_image      image,
        int           rect_num,
        bmcv_rect_t * rect,
        unsigned char r,
        unsigned char g,
        unsigned char b);

/*
* Sorting of floating-point data (ascending/descending)
* src_index_addr is enter the address of the index corresponding to the data
* src_data_addr is address of the input data to be sorted
* dst_index_addr is address of the sorted output data index
* dst_data_addr is address of the sorted output data
* data_cnt is amount of input data to be sorted;sort_cnt is number that needs to be sorted
* order is Ascending or descending;index_enable is Whether to enable index;
* auto_index isWhether to enable the automatic index generation function;
*/
DECL_EXPORT bm_status_t bmcv_sort(
        bm_handle_t     handle,
        bm_device_mem_t src_index_addr,
        bm_device_mem_t src_data_addr,
        int             data_cnt,
        bm_device_mem_t dst_index_addr,
        bm_device_mem_t dst_data_addr,
        int             sort_cnt,
        int             order,
        bool            index_enable,
        bool            auto_index);
/**
 * @brief: calculate topk for each db, return BM_SUCCESS if succeed.
 * @param handle           [in]: the device handle.
 * @param src_data_addr    [in]: device addr information of input_data.
 * @param src_index_addr   [in]: device addr information of input_index, set it if src_index_valid.
 * @param dst_data_addr    [out]: device addr information of output_data
 * @param dst_index_addr   [in]: device addr information of output_index.
 * @param buffer_addr      [in]: device addr information of buffer.
 *        if (max(per_batch_cnt) <= 10000 || (max(per_batch_cnt) <=200000 && topk <=64))
 *          buffer_size = (batch * k + max_cnt) * sizeof(float) * 2
 *        else
 *          buffer_size = ceil(max(per_per_batch_cnt)/4000000) * 3*sizeof(float)*(k>10000? 2*k : 10000)
 * @param src_index_valid  [in]: if true, use src_index, otherwise gen index auto.
 * @param k                [in]: k value
 * @param batch            [in]: batch numeber
 * @param per_batch_cnt    [in]: data_number of per_batch
 * @param src_batch_stride [in]: distance between two batches
 * @param descending       [in]: descending or ascending.
 */
DECL_EXPORT bm_status_t bmcv_batch_topk(
        bm_handle_t     handle,
        bm_device_mem_t src_data_addr,
        bm_device_mem_t src_index_addr,
        bm_device_mem_t dst_data_addr,
        bm_device_mem_t dst_index_addr,
        bm_device_mem_t buffer_addr,
        bool            src_index_valid,
        int             k,
        int             batch,
        int *           per_batch_cnt,
        bool            same_batch_cnt,
        int             src_batch_stride,
        bool            descending);

/**
 * network get feature points(float)and compares them with feature points(float) in database
 * input_data_global_addr is address of the feature point data store to be compared
 * db_data_global_addr is address of the database's feature point data store
 * db_feature_global_addr is database feature_size indicates the reciprocal address of the direction mode
 * output_similarity_global_addr is compare the result to the maximum address
 * output_index_global_addr is serial number address in database
 * batch_size is batch size;feature_size is each data feature points;db_size is number of feature points
*/
DECL_EXPORT bm_status_t bmcv_feature_match_normalized(
        bm_handle_t     handle,
        bm_device_mem_t input_data_global_addr,
        bm_device_mem_t db_data_global_addr,
        bm_device_mem_t db_feature_global_addr,
        bm_device_mem_t output_similarity_global_addr,
        bm_device_mem_t output_index_global_addr,
        int             batch_size,
        int             feature_size,
        int             db_size);

/**
 * network get feature points(int)and compares them with feature points(int) in database
 * input_data_global_addr is address of the feature point data store to be compared
 * db_data_global_addr is address of the database's feature point data store
 * output_sorted_similarity_global_addr is comparison results Maximum address (descending)
 * output_sorted_index_global_addr is serial number address in database
 * batch_size is batch size; feature_size is number of feature points
 * db_size is number of feature points;sort_cnt is Number of output results;
 * rshiftbits is Result displacement number;
*/
DECL_EXPORT bm_status_t bmcv_feature_match(
        bm_handle_t     handle,
        bm_device_mem_t input_data_global_addr,
        bm_device_mem_t db_data_global_addr,
        bm_device_mem_t output_sorted_similarity_global_addr,
        bm_device_mem_t output_sorted_index_global_addr,
        int             batch_size,
        int             feature_size,
        int             db_size,
        int             sort_cnt,
        int             rshiftbits);

/**
 * Implement base64 encoding task
 * src is input data;dst is output data;len is lenth of input and output
*/
DECL_EXPORT bm_status_t bmcv_base64_enc(
        bm_handle_t     handle,
        bm_device_mem_t src,
        bm_device_mem_t dst,
        unsigned long   len[2]);

/**
 * Implement base64 decoding task
 * src is input data;dst is output data;len is lenth of input and output
*/
DECL_EXPORT bm_status_t bmcv_base64_dec(
        bm_handle_t     handle,
        bm_device_mem_t src,
        bm_device_mem_t dst,
        unsigned long   len[2]);

/**
 * @brief: calculate inner product distance between query vectors and database vectors, output the top K IP-values and the corresponding indices, return BM_SUCCESS if succeed.
 * @param handle                               [in]: the device handle.
 * @param input_data_global_addr               [in]: device addr information of the query matrix.
 * @param db_data_global_addr                  [in]: device addr information of the database matrix.
 * @param buffer_global_addr                   [in]: inner product values stored in the buffer.
 * @param output_sorted_similarity_global_addr [out]: the IP-values matrix.
 * @param output_sorted_index_global_addr      [out]: the result indices matrix.
 * @param vec_dims          [in]: vector dimension.
 * @param query_vecs_num    [in]: the num of query vectors.
 * @param database_vecs_num [in]: the num of database vectors.
 * @param sort_cnt          [in]: get top sort_cnt values.
 * @param is_transpose      [in]: db_matrix 0: NO_TRNAS; 1: TRANS.
 * @param input_dtype       [in]: DT_FP32 / DT_INT8.
 * @param output_dtype      [in]: DT_FP32 / DT_INT32.
 */
DECL_EXPORT bm_status_t bmcv_faiss_indexflatIP(
        bm_handle_t     handle,
        bm_device_mem_t input_data_global_addr,
        bm_device_mem_t db_data_global_addr,
        bm_device_mem_t buffer_global_addr,
        bm_device_mem_t output_sorted_similarity_global_addr,
        bm_device_mem_t output_sorted_index_global_addr,
        int             vec_dims,
        int             query_vecs_num,
        int             database_vecs_num,
        int             sort_cnt,
        int             is_transpose,
        int             input_dtype,
        int             output_dtype);

/**
 * @brief: calculate squared L2 distance between query vectors and database vectors, output the top K L2sqr-values and the corresponding indices, return BM_SUCCESS if succeed.
 * @param handle                               [in]: the device handle.
 * @param input_data_global_addr               [in]: device addr information of the query matrix.
 * @param db_data_global_addr                  [in]: device addr information of the database matrix.
 * @param query_L2norm_global_addr             [in]: device addr information of the query norm_L2sqr vector.
 * @param db_L2norm_global_addr                [in]: device addr information of the database norm_L2sqr vector.
 * @param buffer_global_addr                   [in]: squared L2 values stored in the buffer.
 * @param output_sorted_similarity_global_addr [out]: the L2sqr-values matrix.
 * @param output_sorted_index_global_addr      [out]: the result indices matrix.
 * @param vec_dims          [in]: vector dimension.
 * @param query_vecs_num    [in]: the num of query vectors.
 * @param database_vecs_num [in]: the num of database vectors.
 * @param sort_cnt          [in]: get top sort_cnt values.
 * @param is_transpose      [in]: db_matrix 0: NO_TRNAS; 1: TRANS.
 * @param input_dtype       [in]: DT_FP32.
 * @param output_dtype      [in]: DT_FP32.
 */
DECL_EXPORT bm_status_t bmcv_faiss_indexflatL2(
        bm_handle_t     handle,
        bm_device_mem_t input_data_global_addr,
        bm_device_mem_t db_data_global_addr,
        bm_device_mem_t query_L2norm_global_addr,
        bm_device_mem_t db_L2norm_global_addr,
        bm_device_mem_t buffer_global_addr,
        bm_device_mem_t output_sorted_similarity_global_addr,
        bm_device_mem_t output_sorted_index_global_addr,
        int             vec_dims,
        int             query_vecs_num,
        int             database_vecs_num,
        int             sort_cnt,
        int             is_transpose,
        int             input_dtype,
        int             output_dtype);

/**
 * @brief: PQ Asymmetric Distance Computation, output the topK distance and label of x and q(ny), return BM_SUCCESS if succeed.
 * @param handle                         [in]: the device handle.
 * @param centroids_input_dev            [in]: device addr information of the centroids.
 * @param nxquery_input_dev              [in]: device addr information of the query.
 * @param nycodes_input_dev,             [in]: PQcodes of database.
 * @param distance_output_dev            [out]: output topK distance
 * @param index_output_dev               [out]: output topK label
 * @param vec_dims              [in]: vector dimension.
 * @param slice_num             [in]: the num of sliced vector.
 * @param centroids_num         [in]: the num of centroids num.
 * @param database_num          [in]: the num of database vectors.
 * @param query_num             [in]: the num of query vectors.
 * @param sort_cnt              [in]: get top sort_cnt values.
 * @param IP_metric             [in]: metrics 0:L2_matric; 1:IP_matric.
 */
DECL_EXPORT bm_status_t bmcv_faiss_indexPQ_ADC(
        bm_handle_t     handle,
        bm_device_mem_t centroids_input_dev,
        bm_device_mem_t nxquery_input_dev,
        bm_device_mem_t nycodes_input_dev,
        bm_device_mem_t distance_output_dev,
        bm_device_mem_t index_output_dev,
        int             vec_dims,
        int             slice_num,
        int             centroids_num,
        int             database_num,
        int             query_num,
        int             sort_cnt,
        int             IP_metric);

/**
 * @brief: PQ Asymmetric Distance Computation, output the topK distance and label of x and q(ny), return BM_SUCCESS if succeed.
 * @param handle                         [in]: the device handle.
 * @param centroids_input_dev            [in]: device addr information of the centroids.
 * @param nxquery_input_dev              [in]: device addr information of the query.
 * @param nycodes_input_dev,             [in]: PQcodes of database.
 * @param distance_output_dev            [out]: output topK distance
 * @param index_output_dev               [out]: output topK label
 * @param vec_dims              [in]: vector dimension.
 * @param slice_num             [in]: the num of sliced vector.
 * @param centroids_num         [in]: the num of centroids num.
 * @param database_num          [in]: the num of database vectors.
 * @param query_num             [in]: the num of query vectors.
 * @param sort_cnt              [in]: get top sort_cnt values.
 * @param IP_metric             [in]: metrics 0:L2_matric; 1:IP_matric.
 * @param in_dtype              [in]: input data type, support DT_FP32/DTfp16.
 * @param out_dtype             [in]: output data type, support DT_FP32/DTfp16.
 */
DECL_EXPORT bm_status_t bmcv_faiss_indexPQ_ADC_ext(
        bm_handle_t     handle,
        bm_device_mem_t centroids_input_dev,
        bm_device_mem_t nxquery_input_dev,
        bm_device_mem_t nycodes_input_dev,
        bm_device_mem_t distance_output_dev,
        bm_device_mem_t index_output_dev,
        int             vec_dims,
        int             slice_num,
        int             centroids_num,
        int             database_num,
        int             query_num,
        int             sort_cnt,
        int             IP_metric,
        int             in_dtype,
        int             out_dtype);

/**
 * @brief: PQ Symmetric Distance Computation, output the topK distance and label of q(x) and q(ny), return BM_SUCCESS if succeed.
 * @param handle                         [in]: the device handle.
 * @param sdc_table_input_dev            [in]: device addr information of the sdc_table.
 * @param nxcodes_input_dev,             [in]: PQcodes of query.
 * @param nycodes_input_dev,             [in]: PQcodes of database.
 * @param distance_output_dev            [out]: output topK distance.
 * @param index_output_dev               [out]: output topK label.
 * @param slice_num             [in]: the num of sliced vector.
 * @param centroids_num         [in]: the num of centroids num.
 * @param database_num          [in]: the num of database vectors.
 * @param query_num             [in]: the num of query vectors.
 * @param sort_cnt              [in]: get top sort_cnt values.
 * @param IP_metric             [in]: metrics 0:L2_matric; 1:IP_matric.
 */
DECL_EXPORT bm_status_t bmcv_faiss_indexPQ_SDC(
        bm_handle_t     handle,
        bm_device_mem_t sdc_table_input_dev,
        bm_device_mem_t nxcodes_input_dev,
        bm_device_mem_t nycodes_input_dev,
        bm_device_mem_t distance_output_dev,
        bm_device_mem_t index_output_dev,
        int             slice_num,
        int             centroids_num,
        int             database_num,
        int             query_num,
        int             sort_cnt,
        int             IP_metric);

/**
 * @brief: PQ Symmetric Distance Computation, output the topK distance and label of q(x) and q(ny), return BM_SUCCESS if succeed.
 * @param handle                         [in]: the device handle.
 * @param sdc_table_input_dev            [in]: device addr information of the sdc_table.
 * @param nxcodes_input_dev,             [in]: PQcodes of query.
 * @param nycodes_input_dev,             [in]: PQcodes of database.
 * @param distance_output_dev            [out]: output topK distance.
 * @param index_output_dev               [out]: output topK label.
 * @param slice_num             [in]: the num of sliced vector.
 * @param centroids_num         [in]: the num of centroids num.
 * @param database_num          [in]: the num of database vectors.
 * @param query_num             [in]: the num of query vectors.
 * @param sort_cnt              [in]: get top sort_cnt values.
 * @param IP_metric             [in]: metrics 0:L2_matric; 1:IP_matric.
 * @param in_dtype              [in]: input data type, support DT_FP32/DTfp16.
 * @param out_dtype             [in]: output data type, support DT_FP32/DTfp16.
 */
DECL_EXPORT bm_status_t bmcv_faiss_indexPQ_SDC_ext(
        bm_handle_t     handle,
        bm_device_mem_t sdc_table_input_dev,
        bm_device_mem_t nxcodes_input_dev,
        bm_device_mem_t nycodes_input_dev,
        bm_device_mem_t distance_output_dev,
        bm_device_mem_t index_output_dev,
        int             slice_num,
        int             centroids_num,
        int             database_num,
        int             query_num,
        int             sort_cnt,
        int             IP_metric,
        int             in_dtype,
        int             out_dtype);

/**
 * @brief: encode D-dims vectors into m*int8 PQcodes , return BM_SUCCESS if succeed.
 * @param handle                         [in]: the device handle.
 * @param vector_input_dev               [in]: device addr information of the D-dims vectors
 * @param centroids_input_dev            [in]: device addr information of the centroids.
 * @param buffer_table_dev,              [in]: distance table stored in the buffer,size=nv*m*ksub*dtype.
 * @param codes_output_dev               [out]: output PQcodes, size = nv * m * int8.
 * @param encode_vecs_num   [in]: the num of input vectors.
 * @param vec_dims          [in]: vector dimension.
 * @param slice_num         [in]: the num of sliced vector.
 * @param centroids_num     [in]: the num of centroids.
 */
DECL_EXPORT bm_status_t bmcv_faiss_indexPQ_encode(
        bm_handle_t     handle,
        bm_device_mem_t vector_input_dev,
        bm_device_mem_t centroids_input_dev,
        bm_device_mem_t buffer_table_dev,
        bm_device_mem_t codes_output_dev,
        int             encode_vec_num,
        int             vec_dims,
        int             slice_num,
        int             centroids_num,
        int             IP_metric);

/**
 * @brief: encode D-dims vectors into m*int8 PQcodes , return BM_SUCCESS if succeed.
 * @param handle                         [in]: the device handle.
 * @param vector_input_dev               [in]: device addr information of the D-dims vectors
 * @param centroids_input_dev            [in]: device addr information of the centroids.
 * @param buffer_table_dev,              [in]: distance table stored in the buffer,size=nv*m*ksub*dtype.
 * @param codes_output_dev               [out]: output PQcodes, size = nv * m * int8.
 * @param encode_vecs_num   [in]: the num of input vectors.
 * @param vec_dims          [in]: vector dimension.
 * @param slice_num         [in]: the num of sliced vector.
 * @param centroids_num     [in]: the num of centroids.
 * @param in_dtype              [in]: input data type, support DT_FP32/DTfp16.
 * @param out_dtype             [in]: output data type, support DT_FP32/DTfp16.
 */
DECL_EXPORT bm_status_t bmcv_faiss_indexPQ_encode_ext(
        bm_handle_t     handle,
        bm_device_mem_t vector_input_dev,
        bm_device_mem_t centroids_input_dev,
        bm_device_mem_t buffer_table_dev,
        bm_device_mem_t codes_output_dev,
        int             encode_vec_num,
        int             vec_dims,
        int             slice_num,
        int             centroids_num,
        int             IP_metric,
        int             input_dtype,
        int             output_dtype);

DECL_EXPORT bm_status_t bmcv_debug_savedata(bm_image image, const char *name);

/**
 * Image width and height transpose
 * input is src image;output is dst image
*/
DECL_EXPORT bm_status_t bmcv_image_transpose(bm_handle_t handle,
                                 bm_image input,
                                 bm_image output);

/**
 * Realization of 8-bit data type matrix multiplication calculation
 * @param [in] M is A C rows @param [in] N is B C columns
 * @param [in] K A B rows
 * @param [] A B C is Save its device address
 * A_sign B_sign is Matrix sign;rshift_bit si right shift of matrix product
 * result_type is Matrix data type;is_B_trans is bool; alpha is a*b coefficient;
 * beta is offest
*/
DECL_EXPORT bm_status_t bmcv_matmul(
        bm_handle_t      handle,
        int              M,
        int              N,
        int              K,
        bm_device_mem_t  A,
        bm_device_mem_t  B,
        bm_device_mem_t  C,
        int              A_sign,  // 1: signed 0: unsigned
        int              B_sign,
        int              rshift_bit,
        int              result_type,  // 0:8bit 1:int16 2:fp32
        bool             is_B_trans,
        float            alpha,
        float            beta);

DECL_EXPORT bm_status_t bmcv_matmul_transpose_opt(
        bm_handle_t      handle,
        int              M,
        int              N,
        int              K,
        bm_device_mem_t  A,
        bm_device_mem_t  B,
        bm_device_mem_t  C,
        int              A_sign,  // 1: signed 0: unsigned
        int              B_sign);

/**
 * is_A_trans is_B_trans is transpose or not
 * M is A C Y line number;N is B C Y columns;K is A columns and B lines
 * alpha is Multiplicative coefficient;A is A data addr;B is B data addr;
 * beta is Multiplicative coefficient;C is C data addr;Y is Y data addr;
 * input_dtype is data types of A, B, and C;output_dtype is data type of Y
*/
DECL_EXPORT bm_status_t bmcv_gemm_ext(
        bm_handle_t      handle,
        bool                is_A_trans,
        bool                is_B_trans,
        int                 M,
        int                 N,
        int                 K,
        float               alpha,
        bm_device_mem_t     A,
        bm_device_mem_t     B,
        float               beta,
        bm_device_mem_t     C,
        bm_device_mem_t     Y,
        bm_image_data_format_ext in_dtype,
        bm_image_data_format_ext out_dtype);

/**
 * Edge detection Sobel operator
 * input is src image;output is dst image;
 * dx is difference order in the x direction;dy is difference order in the y direction
 * ksize is size of the Sobel nucleus;scale is resulting difference is multiplied by the coefficient
 * delta is offset
*/
DECL_EXPORT bm_status_t bmcv_image_sobel(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        int dx,
        int dy,
        int ksize,
        float scale,
        float delta);

/*
* input is input image; output is output image;ksize is size of the Laplacian nucleus,1 or 3;
*/
DECL_EXPORT bm_status_t  bmcv_image_laplacian(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        unsigned int ksize);

/*
* The interface does F = A * X + Y, A is a constant and size is n * c, and F, X, and Y are matrices of size n * c * h * w.
* tensor_A is Memory address of the device of A;tensor_X is Device memory address of matrix X;
* tensor_Y is Device memory address of matrix Y;tensor_F is Device memory address of matrix F;
* input_n is size of n; input_c is size of c; input_h is size of h; input is size of w;
*/
DECL_EXPORT bm_status_t  bmcv_image_axpy(
        bm_handle_t handle,
        bm_device_mem_t tensor_A,
        bm_device_mem_t tensor_X,
        bm_device_mem_t tensor_Y,
        bm_device_mem_t tensor_F,
        int input_n,
        int input_c,
        int input_h,
        int input_w);

DECL_EXPORT bm_status_t bmcv_image_fusion(
        bm_handle_t handle,
        bm_image input1,
        bm_image input2,
        bm_image output,
        unsigned char thresh,
        unsigned char max_value,
        bm_thresh_type_t type,
        int kw,
        int kh,
        bm_device_mem_t kmem);

/**
 * histogram
 * @param [in] input input data
 * @param [in] output output data
 * C H W is channels width height channels
 * dims is dimensionality; histSizes is each channel counts the number of copies of the histogram
*/
DECL_EXPORT bm_status_t bmcv_calc_hist(bm_handle_t handle,
                           bm_device_mem_t input,
                           bm_device_mem_t output,
                           int C,
                           int H,
                           int W,
                           const int *channels,
                           int dims,
                           const int *histSizes,
                           const float *ranges,
                           int inputDtype);

/**
 * Histogram with weights
 * input output is data;weight is each element in the channel when calculating the histogram
 * C H W is channels width height;channels is list of channels that need to be computed for the histogram
 * dims is < 3;histSizes is each channel counts the number of copies of the histogram
 * ranges is Scope of channel participation statistics;inputDtype is Type of input data
*/
DECL_EXPORT bm_status_t bmcv_calc_hist_with_weight(bm_handle_t handle,
                                       bm_device_mem_t input,
                                       bm_device_mem_t output,
                                       const float *weight,
                                       int C,
                                       int H,
                                       int W,
                                       const int *channels,
                                       int dims,
                                       const int *histSizes,
                                       const float *ranges,
                                       int inputDtype);

/**
 * Histogram equalization improves contrast
 * input is input image device addr; output is output image device addr;
 * H W is image width height
*/
DECL_EXPORT bm_status_t bmcv_hist_balance(
        bm_handle_t handle,
        bm_device_mem_t input,
        bm_device_mem_t output,
        int H,
        int W);

/**
 * Compute graph Laplacian matrix
 * input is similarity matrix M device addr; output is laplacian matrix M device addr;
 * row & col is matrix's row & col
*/
DECL_EXPORT bm_status_t bmcv_lap_matrix(
        bm_handle_t handle,
        bm_device_mem_t input,
        bm_device_mem_t output,
        int row,
        int col);

/*
* The Euclidean distance between multiple points and a particular point in dimensional space
* input and output are device space that holds len point coordinates
* dim is spatial dimension size;pnt is coordinates of a particular point;len is number of coordinates to be found
*/
DECL_EXPORT bm_status_t bmcv_distance(bm_handle_t handle,
                          bm_device_mem_t input,
                          bm_device_mem_t output,
                          int dim,
                          const float *pnt,
                          int len);

DECL_EXPORT bm_status_t bmcv_distance_ext(bm_handle_t handle,
                          bm_device_mem_t input,
                          bm_device_mem_t output,
                          int dim,
                          const void * pnt,
                          int len,
                          int dtyte);

/*
* Create, execute, and destroy.
* batch is batch num;len is lenth of len;forward is Whether it is a forward transform
* plan is handle that needs to be used during the execution phase
*/
DECL_EXPORT bm_status_t bmcv_fft_1d_create_plan(bm_handle_t handle,
                                    int batch,
                                    int len,
                                    bool forward,
                                    void *plan);

/**
 * Two-dimensional M*N FFT operation
 * M is The size of the first dimension;N is The size of the second dimension
 * forward is Whether it is a forward transform;
 * plan is handle that needs to be used during the execution phase
*/
DECL_EXPORT bm_status_t bmcv_fft_2d_create_plan(bm_handle_t handle,
                                    int M,
                                    int N,
                                    bool forward,
                                    void *plan);

/**
 * The created plan can then begin the actual execution phase
 * inputReal is Enter the real part of the data address;
 * inputImag is Enter the address of the imaginary part of the data
 * outputReal is Enter the real part of the data address;
 * outputImag is Enter the address of the imaginary part of the data
 * plan is handle that needs to be used during the execution phase
*/
DECL_EXPORT bm_status_t bmcv_fft_execute(bm_handle_t handle,
                             bm_device_mem_t inputReal,
                             bm_device_mem_t inputImag,
                             bm_device_mem_t outputReal,
                             bm_device_mem_t outputImag,
                             const void *plan);

/**
 * The created plan can then begin the actual execution phase
 * inputReal is Enter the real part of the data address;
 * inputImag is Enter the address of the imaginary part of the data
 * outputReal is Enter the real part of the data address;
 * outputImag is Enter the address of the imaginary part of the data
 * plan is handle that needs to be used during the execution phase
*/
DECL_EXPORT bm_status_t bmcv_fft_execute_real_input(
                             bm_handle_t handle,
                             bm_device_mem_t inputReal,
                             bm_device_mem_t outputReal,
                             bm_device_mem_t outputImag,
                             const void *plan);

DECL_EXPORT void bmcv_fft_destroy_plan(bm_handle_t handle, void *plan);

/**
 * XRHost is Enter the real part of the data address
 * XIHost is Enter the address of the imaginary part of the data
 * YRHost is Enter the real part of the data address;
 * YIHost is Enter the address of the imaginary part of the data
 * batch is the num of signal
 * L is the length of signal
 * realInput is the input signal whether real or complex
 * pad_mode is reflect or constant
 * n_fft is the stft do fft length
 * win_mode is hanning or hamming
 * normalize is normalize or not
**/
DECL_EXPORT bm_status_t bmcv_stft(
                            bm_handle_t handle,
                            float* XRHost, float* XIHost,
                            float* YRHost, float* YIHost,
                            int batch, int L,
                            bool realInput, int pad_mode,
                            int n_fft, int win_mode, int hop_len,
                            bool normalize);

/**
 * XRHost is Enter the real part of the data address
 * XIHost is Enter the address of the imaginary part of the data
 * YRHost is Enter the real part of the data address;
 * YIHost is Enter the address of the imaginary part of the data
 * batch is the num of signal
 * L is the length of signal
 * realInput is the input signal whether real or complex
 * pad_mode is reflect or constant
 * n_fft is the stft do fft length
 * win_mode is hanning or hamming
 * normalize is normalize or not
**/
DECL_EXPORT bm_status_t bmcv_istft(bm_handle_t handle,
                            float* XRHost, float* XIHost,
                            float* YRHost, float* YIHost,
                            int batch, int L,
                            bool realInput, int pad_mode,
                            int n_fft, int win_mode, int hop_len,
                            bool normalize);

// mode = 0 for min only, 1 for max only, 2 for both
DECL_EXPORT bm_status_t bmcv_min_max(bm_handle_t handle,
                         bm_device_mem_t input,
                         float *minVal,
                         float *maxVal,
                         int len);

/*
* Complex multiplication operation
* inputReal is enter the data address of the real part;inputImag is enter the data address of the virtual part
* pointImag is Enter the data address of the virtual part of 2; pointReal is enter the data address of the real part of 2
* outputReal is Output the data address of the real part;outputImag is Output the data address of the virtual part
* batch is batch quantity;len is batch plural number
*/
DECL_EXPORT bm_status_t bmcv_cmulp(bm_handle_t handle,
                       bm_device_mem_t inputReal,
                       bm_device_mem_t inputImag,
                       bm_device_mem_t pointReal,
                       bm_device_mem_t pointImag,
                       bm_device_mem_t outputReal,
                       bm_device_mem_t outputImag,
                       int batch,
                       int len);

/*
* A weighted fusion of two images of the same size
* input1 is input image1;input2 is input image2
* alpha is weight of the 1st image;beta is weight of the 2nd image
* gamma is offset after fusion;output is output bm_image
*/
DECL_EXPORT bm_status_t bmcv_image_add_weighted(
        bm_handle_t handle,
        bm_image input1,
        float alpha,
        bm_image input2,
        float beta,
        float gamma,
        bm_image output);

/*
* Image pixel values are bitwise and manipulated
* input1 input2 is 1st and 2nd image;output is output image;
*/
DECL_EXPORT bm_status_t bmcv_image_bitwise_and(
        bm_handle_t handle,
        bm_image input1,
        bm_image input2,
        bm_image output);

/*
* Pixel values are bitwise or manipulated
* input1 input2 is 1st and 2nd image;output is output image;
*/
DECL_EXPORT bm_status_t bmcv_image_bitwise_or(
        bm_handle_t handle,
        bm_image input1,
        bm_image input2,
        bm_image output);

/*
* The pixel value is operated on a bit-by-bit basis
* input1 input2 is 1st and 2nd image;output is output image;
*/
DECL_EXPORT bm_status_t bmcv_image_bitwise_xor(
        bm_handle_t handle,
        bm_image input1,
        bm_image input2,
        bm_image output);

/*
* Two pictures of the same size correspond to the pixel values and take the absolute value
* input1 intput2 is the input image; output is output image;
*/
DECL_EXPORT bm_status_t bmcv_image_absdiff(
        bm_handle_t handle,
        bm_image input1,
        bm_image input2,
        bm_image output);

/*
* input is enter the matrix data address;output is output matrix data address;
* input_row is enter the number of matrix rows;input_col is enter the number of matrix columns
* output_row is output the number of matrix rows;output_col is output the number of matrix columns
* row_stride is output matrix row step size;col_stride is output matrix column step size
*/
DECL_EXPORT bm_status_t bmcv_as_strided(
        bm_handle_t handle,
        bm_device_mem_t input,
        bm_device_mem_t output,
        int input_row,
        int input_col,
        int output_row,
        int output_col,
        int row_stride,
        int col_stride);

/**
 * Convert bayerBG8 or bayerRG8 format images to rgb plannar format
 * @param [in] convd_kernel   convolution kernel value
 * @param [in] input          input image
 * @param [in] output         output image
*/
DECL_EXPORT bm_status_t bmcv_image_bayer2rgb(
        bm_handle_t handle,
        unsigned char* convd_kernel,
        bm_image input,
        bm_image output);

/*
* Image pixel threshold operation
* input is input image; output is output image;thresh is Pixel threshold,range [0,255]
* max_value is maximum pixel value after thresholding;type is thresholding type,range[0,4]
*/
DECL_EXPORT bm_status_t bmcv_image_threshold(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        unsigned char thresh,
        unsigned char max_value,
        bm_thresh_type_t type);

/*
* Convert float data to int
* input is input image; output is output image
*/
DECL_EXPORT bm_status_t bmcv_image_quantify(
        bm_handle_t handle,
        bm_image input,
        bm_image output);


DECL_EXPORT bm_status_t bmcv_image_rotate(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        int rotation_angle);

DECL_EXPORT bm_status_t bmcv_cos_similarity(
    bm_handle_t handle,
    bm_device_mem_t input_data_global_addr,
    bm_device_mem_t normalized_data_global_addr,
    bm_device_mem_t output_data_global_addr,
    int vec_num,
    int vec_dims);

DECL_EXPORT bm_status_t bmcv_matrix_prune(
    bm_handle_t handle,
    bm_device_mem_t input_data_global_addr,
    bm_device_mem_t output_data_global_addr,
    bm_device_mem_t sort_index_global_addr,
    int matrix_dims,
    float p);

DECL_EXPORT bm_status_t bmcv_image_overlay(
        bm_handle_t      handle,
        bm_image         image,
        int              overlay_num,
        bmcv_rect_t*     overlay_info,
        bm_image*        overlay_image);
/*
* The image is operated by Gaussian filtering
* input is input image; output is output image;kw is kernel width size;
* kh is kernel height size;sigmaX is Gaussian kernel standard deviation in the X direction
* sigmaY is Gaussian kernel standard deviation in the Y direction
*/
DECL_EXPORT bm_status_t bmcv_image_gaussian_blur(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        int kw,
        int kh,
        float sigmaX,
        float sigmaY);

DECL_EXPORT bm_status_t bmcv_image_assigned_area_blur(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        int ksize,
        int assigned_area_num,
        float center_weight_scale,
        bmcv_rect_t *assigned_area);

/**
 * only 1684
 * input output is src and dst;threshold1 threshold2 is 1st and 2nd threshold;
 * aperture_size is only 3;l2gradient is Whether to use L2 norm to find the image gradient
*/
DECL_EXPORT bm_status_t bmcv_image_canny(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        float threshold1,
        float threshold2,
        int aperture_size,
        bool l2gradient);

/*
* Draw one or more line segments on an image
* img is bm_image to be processed
* start is information of starting of line;end is information of ending of line;
* line_num is number of lines drawn;color is line with color;thickness is line width
*/
DECL_EXPORT bm_status_t bmcv_image_draw_lines(
        bm_handle_t handle,
        bm_image img,
        const bmcv_point_t* start,
        const bmcv_point_t* end,
        int line_num,
        bmcv_color_t color,
        int thickness);

/*
* The function of writing on an image
* image is input image;text is Text to be written, in English;
* org is coordinates at the bottom left of the first character;
* color is draw the color of the line
* fontScale is font size; thickness is draw the width of the line
*/
DECL_EXPORT bm_status_t bmcv_image_put_text(
        bm_handle_t handle,
        bm_image image,
        const char* text,
        bmcv_point_t org,
        bmcv_color_t color,
        float fontScale,
        int thickness);

/**
 * only 1684
 * shape is shape of Kernel;kw is kernel width;kh is kernel height
*/
DECL_EXPORT bm_device_mem_t bmcv_get_structuring_element(
        bm_handle_t handle,
        bmcv_morph_shape_t shape,
        int kw,
        int kh
        );

/**
 * Corrosion and expansion operations are currently supported
 * src is input image;dst is output image;kw is windows w;
 * kh is windows h;kmem is device memory
*/
DECL_EXPORT bm_status_t bmcv_image_erode(
        bm_handle_t handle,
        bm_image src,
        bm_image dst,
        int kw,
        int kh,
        bm_device_mem_t kmem
        );

/**
 * Corrosion and expansion operations are currently supported
 * src is input image;dst is output image;kw is windows w;
 * kh is windows h;kmem is device memory
*/
DECL_EXPORT bm_status_t bmcv_image_dilate(
        bm_handle_t handle,
        bm_image src,
        bm_image dst,
        int kw,
        int kh,
        bm_device_mem_t kmem
        );

/**
 * Realize downsampling in image Gaussian pyramid operation
 * input is src image;output is dst image
*/
DECL_EXPORT bm_status_t bmcv_image_pyramid_down(
        bm_handle_t handle,
        bm_image input,
        bm_image output);

/**
 * 1684x only
 * plan is handle required by the execution phase
 * width is width of the image to be processed
 * height is height of the image to be processed
 * winW is algorithm deals with the width of the window
 * winH is algorithm deals with the height of the window
 * Pyramid handles height is maxLevel
*/
DECL_EXPORT bm_status_t bmcv_image_lkpyramid_create_plan(
        bm_handle_t handle,
        void* plan,
        int width,
        int height,
        int winW,
        int winH,
        int maxLevel);

DECL_EXPORT bm_status_t bmcv_image_lkpyramid_execute(
        bm_handle_t handle,
        void* plan,
        bm_image prevImg,
        bm_image nextImg,
        int ptsNum,
        bmcv_point2f_t* prevPts,
        bmcv_point2f_t* nextPts,
        bool* status,
        bmcv_term_criteria_t criteria);

/**
 * The created handle needs to be destroyed when the execution is complete
*/
DECL_EXPORT void bmcv_image_lkpyramid_destroy_plan(
        bm_handle_t handle,
        void* plan);

/**
 * @brief bmcv_dct_coeff
 * @param handle
 * @param H
 * @param W
 * @param hcoeff_output: HxH
 * @param wcoeff_output: WxW
 * @param is_inversed: 0-dct, 1-idct
 * @return
 */
DECL_EXPORT bm_status_t bmcv_dct_coeff(
        bm_handle_t handle,
        int H,
        int W,
        bm_device_mem_t hcoeff_output,
        bm_device_mem_t wcoeff_output,
        bool is_inversed
        );

/**
 * @brief bmcv_image_dct_with_coeff which can reuse dct coeff for optimization, output=hcoeff[HxH]*input[HxW]*wcoeff[WxW]
 * @param handle
 * @param input: image
 * @param hcoeff: output from bmcv_dct_coeff
 * @param wcoeff: output from bmcv_dct_coeff
 * @param output: dct output
 * @return
 */
DECL_EXPORT bm_status_t bmcv_image_dct_with_coeff(
        bm_handle_t handle,
        bm_image input,
        bm_device_mem_t hcoeff,
        bm_device_mem_t wcoeff,
        bm_image output
        );

/**
 * @brief bmcv_image_dct: recalculate dct coeff every time, that will cost more time
 * @param handle
 * @param input
 * @param output
 * @return
 */
DECL_EXPORT bm_status_t bmcv_image_dct(
        bm_handle_t handle,
        bm_image input,
        bm_image output,
        bool is_inversed
        );

DECL_EXPORT bm_status_t bmcv_open_cpu_process(bm_handle_t handle);
DECL_EXPORT bm_status_t bmcv_close_cpu_process(bm_handle_t handle);

//#ifndef USING_CMODEL
/*
* The interface realize the combination of crop, color-space-convert, resize, padding and any number of functions for multiple pictures
* in_img_num is input bm_image num; input is input bm_image pointer; output is output image pointer;crop_num_vec is crop quantity per input picture
* crop_rect is output bm_image object information to the crop parameter on the input image
* padding_attr is all crop image infor and padding infor pointer; algorithm is resize algorithm; csc_type is color space convert Specifies the parameter type
* matrix is that CSC_USER_DEFINED_MATRIX is selected, the coefficient matrix needs to be passed.
*/
DECL_EXPORT bm_status_t bmcv_image_vpp_basic(bm_handle_t           handle,
                                 int                   in_img_num,
                                 bm_image*             input,
                                 bm_image*             output,
                                 int*                  crop_num_vec,
                                 bmcv_rect_t*          crop_rect,
                                 bmcv_padding_atrr_t*  padding_attr,
                                 bmcv_resize_algorithm algorithm,
                                 csc_type_t            csc_type,
                                 csc_matrix_t*         matrix);

/*
* Interface parameter description:
* output_num is output bm_image num, it is same num with crop num of src image
* input is input bm_image; output is output bm_image; padding_attr is bmcv_rect_t pointer
* algorithm is resize algorithm
*/
DECL_EXPORT bm_status_t bmcv_image_vpp_convert(
    bm_handle_t           handle,
    int                   output_num,
    bm_image              input,
    bm_image *            output,
    bmcv_rect_t *         crop_rect,
    bmcv_resize_algorithm algorithm);

/*
* Interface parameter description:
* output_num is output num;input is input bm_image;output is output bm_image pointer;csc is gamut conversion enumeration type
* matrix is color gamut conversion custom matrix;algorithm is resize algorithm selection;
* crop_rect is crop infor in src bm_image
*/
DECL_EXPORT bm_status_t bmcv_image_vpp_csc_matrix_convert(bm_handle_t           handle,
                                              int                   output_num,
                                              bm_image              input,
                                              bm_image *            output,
                                              csc_type_t            csc,
                                              csc_matrix_t *        matrix,
                                              bmcv_resize_algorithm algorithm,
                                              bmcv_rect_t *         crop_rect);

/*
* Interface parameter description:
* output_num is output bm_image num, it is same num with crop num of src image
* input is input bm_image; output is output bm_image; padding_attr is bmcv_rect_t pointer
* algorithm is resize algorithm
*/
DECL_EXPORT bm_status_t bmcv_image_vpp_convert_padding(
    bm_handle_t           handle,
    int                   output_num,
    bm_image              input,
    bm_image *            output,
    bmcv_padding_atrr_t * padding_attr,
    bmcv_rect_t *         crop_rect,
    bmcv_resize_algorithm algorithm);

/*
* Using the crop function with hardware resources to do image stitching,src crop + csc + resize + dst crop at one time can be compleled
* input_num is input image num;input is input image pointer;output is output image;dst_crop_rect is coordinates,width and height in dst image
* src_crop_rect is coordinates,width and height of dst in src image;algorithm is resize algorithm;
*/
DECL_EXPORT bm_status_t bmcv_image_vpp_stitch(
    bm_handle_t          handle,
    int                  input_num,
    bm_image*            input,
    bm_image             output,
    bmcv_rect_t*         dst_crop_rect,
    bmcv_rect_t*         src_crop_rect,
    bmcv_resize_algorithm algorithm);
//#endif

/**
 * Legacy functions
 */

typedef bmcv_affine_image_matrix bmcv_warp_image_matrix;
typedef bmcv_affine_matrix bmcv_warp_matrix;

#ifdef __linux__
/**
 * Image affine transformation can realize rotation, translation, scaling and other operations
 * image_num is image num;matrix is transformation matrix data structure corresponding to each image
 * input is src image;output is dst image;
*/
bm_status_t bmcv_image_warp(
        bm_handle_t            handle,
        int                    image_num,
        bmcv_warp_image_matrix matrix[4],
        bm_image *             input,
        bm_image *output) __attribute__((deprecated));
bm_status_t bm_image_dev_mem_alloc(bm_image image,
                                   int heap_id) __attribute__((deprecated));
#endif

/**
 * The interface realizes transmission transformation of image
 * image_num is image num;matrix is transformation matrix data structure corresponding to each image
 * input is src image;output is dst image;use_bilinear is if to use bilinear for interpolation
*/
DECL_EXPORT bm_status_t bmcv_image_warp_perspective_similar_to_opencv(
    bm_handle_t                       handle,
    int                               image_num,
    bmcv_perspective_image_matrix     matrix[4],
    bm_image *                        input,
    bm_image *                        output,
    int                               use_bilinear);

DECL_EXPORT bm_status_t bm1684x_vpp_fill_rectangle(
  bm_handle_t          handle,
  int                  input_num,
  bm_image *           input,
  bm_image *           output,
  bmcv_rect_t*         input_crop_rect,
  unsigned char        r,
  unsigned char        g,
  unsigned char        b);

DECL_EXPORT bm_status_t bm1684x_vpp_cmodel_csc_resize_convert_to(
  bm_handle_t             handle,
  int                     frame_number,
  bm_image*               input,
  bm_image*               output,
  bmcv_rect_t*            input_crop_rect,
  bmcv_padding_atrr_t*    padding_attr,
  bmcv_resize_algorithm   algorithm,
  csc_type_t              csc_type,
  csc_matrix_t*           matrix,
  bmcv_convert_to_attr*   convert_to_attr);

DECL_EXPORT bm_status_t bm1684x_vpp_cmodel_border(
  bm_handle_t             handle,
  int                     rect_num,
  bm_image*               input,
  bm_image*               output,
  bmcv_rect_t*            rect,
  int                     line_width,
  unsigned char           r,
  unsigned char           g,
  unsigned char           b);

/*
* The interface image is printed with one or more mosaics
* mosaic_num is Mosaic num; input is need to Mosaic the bm_image
* mosaic_rect is contains Pointers to the starting point and width and height of each Mosaic
* is_expand is column expansion or not
*/
DECL_EXPORT bm_status_t bmcv_image_mosaic(
  bm_handle_t           handle,
  int                   mosaic_num,
  bm_image              input,
  bmcv_rect_t *         mosaic_rect,
  int                   is_expand);

/**
 * Used to overlay one or more watermarks on an image
 * image is src;bitmap_mem is Watermark data pointer;bitmap_num is Watermark num
 * bitmap_type is Watermark type;pitch is Watermark width;rects is Watermark position pointer
 * color is Watermark color
*/
DECL_EXPORT bm_status_t bmcv_image_watermark_superpose(
  bm_handle_t           handle,
  bm_image *            image,
  bm_device_mem_t *     bitmap_mem,
  int                   bitmap_num,
  int                   bitmap_type,
  int                   pitch,
  bmcv_rect_t *         rects,
  bmcv_color_t          color);

/**
 * Used to overlay one or more watermarks on an image
 * image is src;bitmap_mem is Watermark data pointer;bitmap_num is Watermark num
 * bitmap_type is Watermark type;pitch is Watermark width;rects is Watermark position pointer
 * color is Watermark color
*/
DECL_EXPORT bm_status_t bmcv_image_watermark_repeat_superpose(
  bm_handle_t           handle,
  bm_image              image,
  bm_device_mem_t       bitmap_mem,
  int                   bitmap_num,
  int                   bitmap_type,
  int                   pitch,
  bmcv_rect_t *         rects,
  bmcv_color_t          color);

DECL_EXPORT bm_status_t bmcv_gen_text_watermark(
  bm_handle_t           handle,
  const wchar_t*        hexcode,
  bmcv_color_t          color,
  float                 fontscale,
  bm_image_format_ext   format,
  bm_image*             output);

/*
* This interface realize the combination of crop, color-space-convert, resize, padding, convert_to for multiple pictures
* img_num is bm_image num; input is input bm_image pointer;output is output bm_image pointer; crop_num_vec is each input image with crop num
* crop_rect is output image crop attr in input_image; padding_attr is crop infor in dst image and padding value;
* algorithm is resize algorithm selection; csc_type is color space convert type; matrix is CSC_USER_DEFINED_MATRIX with coefficient matrix
* convert_to_attr is linear transformation coefficient;
*/
DECL_EXPORT bm_status_t bmcv_image_csc_convert_to(
  bm_handle_t             handle,
  int                     img_num,
  bm_image*               input,
  bm_image*               output,
  int*                    crop_num_vec,
  bmcv_rect_t*            crop_rect,
  bmcv_padding_atrr_t*    padding_attr,
  bmcv_resize_algorithm   algorithm,
  csc_type_t              csc_type,
  csc_matrix_t*           matrix,
  bmcv_convert_to_attr*   convert_to_attr);

DECL_EXPORT bm_status_t bmcv_image_vpp_basic_v2(
  bm_handle_t             handle,
  int                     img_num,
  bm_image*               input,
  bm_image*               output,
  int*                    crop_num_vec,
  bmcv_rect_t*            crop_rect,
  bmcv_padding_atrr_t*    padding_attr,
  bmcv_resize_algorithm   algorithm,
  csc_type_t              csc_type,
  csc_matrix_t*           matrix,
  bmcv_convert_to_attr*   convert_to_attr);

/**
 * This interface is used to fill one or more points on an image。
 * image is bm_image on which users need to draw a point.
 * point_num is number of points boxes;coord is Pointer position pointer
 * length is length of the point; r g b is component of the color.
*/
DECL_EXPORT bm_status_t bmcv_image_draw_point(
  bm_handle_t   handle,
  bm_image      image,
  int           point_num,
  bmcv_point_t *coord,
  int           length,
  unsigned char r,
  unsigned char g,
  unsigned char b);

/**
 * Compute qr householder decomposition
 * arm_select_egin_vector is postprocessed eignvector
 * num_spks_output        is dynamic num_spks
 * input_A                is host input
 * arm_sorted_eign_value  is eignvector before postprocess
 * n is length of input square matrix
 * user_num_spks is num of user-given cols of postprocessed eignvector
 * max_num_spks  is num of user-given max cols of postprocessed eignvector
 * min_num_spks  is num of user-given min cols of postprocessed eignvector
*/
DECL_EXPORT void bmcv_qr_cpu(
    float* arm_select_egin_vector,
    int*   num_spks_output,
    float* input_A,
    float* arm_sorted_eign_value,
    int n,
    int user_num_spks,
    int max_num_spks,
    int min_num_spks);

/**
 * Compute KNN kmeans
 * centroids_global_addr is distance of KNN
 * labels_global_addr    is  result labels of KNN
 * input_global_addr     is  input matrix
 * weight_global_addr    is  random weight
 * buffer_global_addr    is  commmon buffer
 * Shape_Input  is input matrix shape
 * Shape_Weight is weight matrix shape
 * dims_Input   is input matrix dims
 * dims_Weight  is weight matrix dims
 * n_feat       is w-dim of input/weight matrix
 * num_iter     is KNN iteration
 * k  nums of KNN clusters
 * buffer_coeff, buffer_max_cnt means buffer_size =  buffer_coeff * buffer_max_cnt
 * dtype is data type
*/
DECL_EXPORT bm_status_t bmcv_knn(
    bm_handle_t handle,
    //output
    bm_device_mem_t centroids_global_addr,//[m_code,  n_feat]
    bm_device_mem_t labels_global_addr,   //[m_obs]
    //input nxn
    bm_device_mem_t input_global_addr,    //[m_obs,   n_feat]
    bm_device_mem_t weight_global_addr,   //[m_code,  n_feat]
    //buffer 2 * max(m_obs, m_code) * 4
    bm_device_mem_t buffer_global_addr,
    int* Shape_Input,
    int* Shape_Weight,
    int  dims_Input,
    int  dims_Weight,
    int  n_feat,
    int  k,
    int  num_iter,
    int  buffer_coeff,
    unsigned int buffer_max_cnt,
    int  dtype);

bm_status_t bmcv_cluster(bm_handle_t     handle,
                         bm_device_mem_t input,
                         bm_device_mem_t output,
                         int             row,
                         int             col,
                         float           p,
                         int             min_num_spks,
                         int             max_num_spks,
                         int             num_spks,
                         int             weight_mode_KNN,
                         int             num_iter_KNN);

DECL_EXPORT bm_status_t bmcv_knn2(
        bm_handle_t handle,
        bm_device_mem_t ref_data_addr,
        bm_device_mem_t test_data_addr,
        bm_device_mem_t distance_addr,
        bm_device_mem_t indices_addr,
        int n_test,
        int n_ref,
        int n_feat,
        int k);

DECL_EXPORT bm_status_t bmcv_knn_match(
        bm_handle_t handle,
        bm_device_mem_t ref_addr,
        bm_device_mem_t test_addr,
        bm_device_mem_t distance_addr,
        bm_device_mem_t good_match_addr,
        bm_device_mem_t match_index_addr,
        int n_ref,
        int n_ref_feat,
        int n_test_feat,
        int n_descriptor,
        float ratio_thresh);


DECL_EXPORT bm_status_t bmcv_matrix_log(
  bm_handle_t handle,
  bm_image src,
  bm_image dst);

/**
 * The data in the address requested by bm_image is cleared before use
*/
DECL_EXPORT bm_status_t bm_image_zeros(bm_image image);

/* Read information from a file into the bm_image data structure */
DECL_EXPORT void bm_read_bin(bm_image src, const char *input_name);
/* Insert the data from the bm_image structure into the file */
DECL_EXPORT void bm_write_bin(bm_image dst, const char *output_name);

DECL_EXPORT void bmcv_print_version();

DECL_EXPORT unsigned long long bmcv_calc_cbcr_addr(unsigned long long y_addr, unsigned int y_stride, unsigned int frame_height);

typedef struct {
        unsigned short  manti : 10;
        unsigned short  exp : 5;
        unsigned short  sign : 1;
} fp16;

typedef struct {
        unsigned int manti : 23;
        unsigned int exp : 8;
        unsigned int sign : 1;
} fp32;

union fp16_data {
        unsigned short idata;
        fp16 ndata;
};

union fp32_data {
        unsigned int idata;
        int idataSign;
        float fdata;
        fp32 ndata;
};

DECL_EXPORT float fp16tofp32(fp16 h);

DECL_EXPORT fp16 fp32tofp16 (float A, int round_method);

#if defined(__cplusplus)
}
#endif

#endif /* BMCV_API_EXT_H */
