bmcv_distance
=============

Calculate the Euclidean distance between multiple points and a specific point in multidimensional space. The coordinates of the former are stored in continuous device memory, while the coordinates of a specific point are passed in through parameters. The coordinate value is of float type.


The format of the interface is as follows:

    .. code-block:: c

        bm_status_t bmcv_distance(
                bm_handle_t handle,
                bm_device_mem_t input,
                bm_device_mem_t output,
                int dim,
                const float *pnt,
                int len);


**Input parameter description:**

* bm_handle_t handle

  Input parameter. The handle of bm_handle

* bm_device_mem_t input

  Input parameter. Device space for storing len point coordinates. Its size is len*dim*sizeof (float).

* bm_device_mem_t output

  Output parameter. Device space for storing len distances. Its size is len*sizeof (float).

* int dim

  Input parameter. Space dimension size.

* const float \*pnt

  Input parameter. The coordinate of a specific point, with the length of dim.

* int len

  Input parameter. Number of coordinates to be calculated.



**Return value description:**

* BM_SUCCESS: success

* Other: failed



**Sample code**


    .. code-block:: c

        int L = 1024 * 1024;
        int dim = 3;
        float pnt[8] = {0};
        for (int i = 0; i < dim; ++i)
            pnt[i] = (rand() % 2 ? 1.f : -1.f) * (rand() % 100 + (rand() % 100) * 0.01);
        float *XHost = new float[L * dim];
        for (int i = 0; i < L * dim; ++i)
            XHost[i] = (rand() % 2 ? 1.f : -1.f) * (rand() % 100 + (rand() % 100) * 0.01);
        float *YHost = new float[L];
        bm_handle_t handle = nullptr;
        bm_dev_request(&handle, 0);
        bm_device_mem_t XDev, YDev;
        bm_malloc_device_byte(handle, &XDev, L * dim * 4);
        bm_malloc_device_byte(handle, &YDev, L * 4);
        bm_memcpy_s2d(handle, XDev, XHost);
        bmcv_distance(handle,
                      XDev,
                      YDev,
                      dim,
                      pnt,
                      L));
        bm_memcpy_d2s(handle, YHost, YDev));
        delete [] XHost;
        delete [] YHost;
        bm_free_device(handle, XDev);
        bm_free_device(handle, YDev);
        bm_dev_free(handle);

