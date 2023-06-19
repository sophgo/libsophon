bmcv_image_put_text
===================

The functions of writing (English) on an image and specifying the color, size and width of words are supported.


**Interface form:**

    .. code-block:: c

        typedef struct {
            int x;
            int y;
        } bmcv_point_t;

        typedef struct {
            unsigned char r;
            unsigned char g;
            unsigned char b;
        } bmcv_color_t;

        bm_status_t bmcv_image_put_text(
                bm_handle_t handle,
                bm_image image,
                const char* text,
                bmcv_point_t org,
                bmcv_color_t color,
                float fontScale,
                int thickness);


**Parameter Description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle.

* bm_image image

  Input / output parameter. The bm_image of image to be processed. The creation of bm_image requires an external call to bmcv_image_create. Image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to create new memory, or use bmcv_image_attach to attach existing memory.

* const char* text

  Input parameter. The text content to be written. Currently only supports English.

* bmcv_point_t org

  Input parameter. The coordinate position of the lower left corner of the first character. The upper left corner of the image is the origin, extending to the right in the x-direction and downward in the y-direction.

* bmcv_color_t color

  Input parameter. The color of the drawn line, which is the value of RGB three channels respectively.

* float fontScale

  Input parameter. Font scale.

* int thickness

  Input parameter. The width of the drawn line, which is recommended to be set to an even number for YUV format images.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Format support:**

The interface currently supports the following images_format:

+-----+------------------------+
| num | image_format           |
+=====+========================+
| 1   | FORMAT_GRAY            |
+-----+------------------------+
| 2   | FORMAT_YUV420P         |
+-----+------------------------+
| 3   | FORMAT_YUV422P         |
+-----+------------------------+
| 4   | FORMAT_YUV444P         |
+-----+------------------------+
| 5   | FORMAT_NV12            |
+-----+------------------------+
| 6   | FORMAT_NV21            |
+-----+------------------------+
| 7   | FORMAT_NV16            |
+-----+------------------------+
| 8   | FORMAT_NV61            |
+-----+------------------------+

The following data_type is currently supported:

+-----+--------------------------------+
| num | data_type                      |
+=====+================================+
| 1   | DATA_TYPE_EXT_1N_BYTE          |
+-----+--------------------------------+


**Code example:**

    .. code-block:: c


        int channel   = 1;
        int width     = 1920;
        int height    = 1080;
        int dev_id    = 0;
        int thickness = 4
        float fontScale = 4;
        char text[20] = "hello world";
        bmcv_point_t org = {100, 100};
        bmcv_color_t color = {255, 0, 0};
        bm_handle_t handle;
        bm_status_t dev_ret = bm_dev_request(&handle, dev_id);
        std::shared_ptr<unsigned char> data_ptr(
                new unsigned char[channel * width * height],
                std::default_delete<unsigned char[]>());
        for (int i = 0; i < channel * width * height; i++) {
            data_ptr.get()[i] = rand() % 255;
        }
        // calculate res
        bm_image img;
        bm_image_create(handle,
                        height,
                        width,
                        FORMAT_GRAY,
                        DATA_TYPE_EXT_1N_BYTE,
                        &img);
        bm_image_alloc_dev_mem(img);
        bm_image_copy_host_to_device(img, (void **)&(data_ptr.get()));
        if (BM_SUCCESS != bmcv_image_put_text(handle, img, text, org, color, fontScale, thickness)) {
            std::cout << "bmcv put text error !!!" << std::endl;
            bm_image_destroy(img);
            bm_dev_free(handle);
            return;
        }
        bm_image_copy_device_to_host(img, (void **)&(data_ptr.get()));
        bm_image_destroy(img);
        bm_dev_free(handle);


