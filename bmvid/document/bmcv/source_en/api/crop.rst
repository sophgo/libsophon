bmcv_image_crop
===============


The interface can crop out several small images from an original image.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_image_crop(
                bm_handle_t         handle,
                int                 crop_num,
                bmcv_rect_t*        rects,
                bm_image            input,
                bm_image*           output
        );


**Parameter Description:**

* bm_handle_t handle

  Input parameter. bm_handle handle.

* int crop_num

  Input parameter. The number of crop small images is required, which is the length of the content pointed to by the pointer rects and the number of output bm_image.

* bmcv_rect_t\* rects

  Input parameter. It refers to the information related to the crop, including the starting coordinates, crop width and height. For details, please refer to the data type description below. The pointer points to the information of several crop boxes, and the number of boxes is determined by crop_num.

* bm_image input

  Input parameter. The creation of input bm_image requires the external calling of bmcv_image _create. Image memory can use bm_image_alloc_dev_mem or use bm_image_copy_host_to_device to applicate memory, or use bmcv_image_attach to attach existing memory.

* bm_image\* output

  Output parameter. The pointer of output bm_image whose number is crop_num. The creation of bm_image requires the external calling of bm_image_create. New image memory can be opened through bm_image_alloc_dev_mem. Users can also use bmcv_image_attach to attach the existing memory. If users do not actively allocate, the memory will be allocated automatically within the API.


**Return parameter description:**

* BM_SUCCESS: success

* Other: failed


**Data type description:**


    .. code-block:: c

        typedef struct bmcv_rect {
            int start_x;
            int start_y;
            int crop_w;
            int crop_h;
        } bmcv_rect_t;



* start_x describes the starting horizontal coordinate of where the crop image is located in the original image. It starts at 0 from left to right and takes values in the range [0, width).

* start_y describes the starting vertical coordinate of where the crop image is located in the original image. It starts at 0 from top to bottom and takes values in the range [0, height).

* crop_w describes the width of the crop image, that is, the width of the corresponding output image.

* crop_h describes the height of the crop image, that is, the height corresponding to the output image.


**Supported format**

Crop currently supports the following image_format:

+-----+------------------------+
| num | image_format           |
+=====+========================+
|  1  | FORMAT_BGR_PACKED      |
+-----+------------------------+
|  2  | FORMAT_BGR_PLANAR      |
+-----+------------------------+
|  3  | FORMAT_RGB_PACKED      |
+-----+------------------------+
|  4  | FORMAT_RGB_PLANAR      |
+-----+------------------------+
|  5  | FORMAT_GRAY            |
+-----+------------------------+


bm1684 crop currently supports the following data_type

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
|  1  | DATA_TYPE_EXT_FLOAT32          |
+-----+--------------------------------+
|  2  | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+
|  3  | DATA_TYPE_EXT_1N_BYTE_SIGNED   |
+-----+--------------------------------+

bm1684x crop currently supports the following data_type:

+-----+-------------------------------+
| num | data_type                     |
+=====+===============================+
|  1  | DATA_TYPE_EXT_1N_BYTE         |
+-----+-------------------------------+


**Note**

1. Before calling bmcv_image_crop(), you must ensure that the input image memory has been applied.

2. Data_type and image_format of input must be the same.

3. To avoid memory overruns, start_x + crop_w must be less than or equal to the width of the input image; start_y + crop_h must be less than or equal to the height of the input image.



**Code example:**

    .. code-block:: c


        int channel   = 3;
        int in_w      = 400;
        int in_h      = 400;
        int out_w     = 800;
        int out_h     = 800;
        int    dev_id = 0;
        bm_handle_t handle;
        bm_status_t dev_ret = bm_dev_request(&handle, dev_id);
        std::shared_ptr<unsigned char> src_ptr(
                new unsigned char[channel * in_w * in_h],
                std::default_delete<unsigned char[]>());
        std::shared_ptr<unsigned char> res_ptr(
                new unsigned char[channel * out_w * out_h],
                std::default_delete<unsigned char[]>());
        unsigned char * src_data = src_ptr.get();
        unsigned char * res_data = res_ptr.get();
        for (int i = 0; i < channel * in_w * in_h; i++) {
            src_data[i] = rand() % 255;
        }
        // calculate res
        bmcv_rect_t crop_attr;
        crop_attr.start_x   = 0;
        crop_attr.start_y   = 0;
        crop_attr.crop_w    = 50;
        crop_attr.crop_h    = 50;
        bm_image input, output;
        bm_image_create(handle,
                in_h,
                in_w,
                FORMAT_RGB_PLANAR,
                DATA_TYPE_EXT_1N_BYTE,
                &input);
        bm_image_alloc_dev_mem(input);
        bm_image_copy_host_to_device(input, (void **)&src_data);
        bm_image_create(handle,
                out_h,
                out_w,
                FORMAT_RGB_PLANAR,
                DATA_TYPE_EXT_1N_BYTE,
                &output);
        bm_image_alloc_dev_mem(output);
        if (BM_SUCCESS != bmcv_image_crop(handle, 1, &crop_attr, input, &output)) {
            std::cout << "bmcv_copy_to error !!!" << std::endl;
            bm_image_destroy(input);
            bm_image_destroy(output);
            bm_dev_free(handle);
            exit(-1);
        }
        bm_image_copy_device_to_host(output, (void **)&res_data);
        bm_image_destroy(input);
        bm_image_destroy(output);
        bm_dev_free(handle);


