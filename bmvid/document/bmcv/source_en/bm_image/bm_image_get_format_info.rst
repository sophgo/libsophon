bm_image_get_format_info
========================

This interface is used to get some information about the bm_image.

**Interface form:**

    .. code-block:: c

        bm_status_t bm_image_get_format_info(
                bm_image *              src,
                bm_image_format_info_t *info
        );


**Input parameters description:**

* bm_image\*  src

  Input parameter. The target bm_image to obtain information.

* bm_image_foramt_info_t \*info

  Output parameter. Save the data structure of the required information and return it to the user. See the data structure description below for detailst.



**Return parameters description:**

* BM_SUCCESS: success

* Other: failed


**Data structure description:**

    .. code-block:: c

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

* int plane_nb

  Number of planes for this image

* bm_device_mem_t plane_data[8]

  Device memory of each plane

* int stride[8];

  Stride value of each plane

* int width;

  Width of the image

* int height;

  Height of the image

* bm_image_format_ext image_format;

  Image format

* bm_image_data_format_ext data_type;

  Storage data type of the image

* bool default_stride;

  Whether the defaulted stride value is used.



