bmcv_image_draw_lines
======================

The function of drawing polygons can be implemented by drawing one or more lines on an image, it also can specify the color and width of lines.


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

        bm_status_t bmcv_image_draw_lines(
                bm_handle_t handle,
                bm_image img,
                const bmcv_point_t* start,
                const bmcv_point_t* end,
                int line_num,
                bmcv_color_t color,
                int thickness);


**Parameter Description:**

* bm_handle_t handle

  Input parameter. Handle of bm_handle.

* bm_image img

  Input/output parameter. The bm_image of the image to be processed. The creation of bm_image requires an external call to bmcv_image_create. Image memory can use bm_image_alloc_dev_mem or bm_image_copy_host_to_device to create new memory, or use bmcv_image_attach to attach existing memory.

* const bmcv_point_t* start

  Input parameter. The coordinate pointer of the starting point of the line segment. The data length pointed to is determined by line_num. The upper left corner of the image is the origin, extending to the right in the x-direction and downward in the y-direction.

* const bmcv_point_t* end

  Input parameter. The coordinate pointer of the end point of the line segment. The data length pointed to is determined by line_num. The upper left corner of the image is the origin, extending to the right in the x-direction and downward in the y-direction.

* int line_num

  Input parameter. The number of lines to be drawn.

* bmcv_color_t color

  Input parameter. The color of the drawn line, which is the value of RGB three channels respectively.

* int thickness

  Input parameter. The width of the drawn line, which is recommended to be set as even numbers for YUV format images.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Format Support:**

The interface currently supports the following image_format:

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
        bmcv_point_t start = {0, 0};
        bmcv_point_t end = {100, 100};
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
        if (BM_SUCCESS != bmcv_image_draw_lines(handle, img, &start, &end, 1, color, thickness)) {
            std::cout << "bmcv draw lines error !!!" << std::endl;
            bm_image_destroy(img);
            bm_dev_free(handle);
            return;
        }
        bm_image_copy_device_to_host(img, (void **)&(data_ptr.get()));
        bm_image_destroy(img);
        bm_dev_free(handle);


