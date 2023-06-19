bm_image device memory management
============================


The bm_image structure needs to be associated with relevant device memory. Only when there is the data you need in the device memory can the subsequent bmcv API be called. Whether calling bm_image_alloc_dev_mem internally or calling bm_image_attach associated with external memory, users can connect the bm_image object with the device memory.


Users can call the following API to judge whether the bm_image object has been connected to the device memory:

    .. code-block:: c

        bool bm_image_is_attached(
                bm_image image
        );


**Incoming parameters description:**

* bm_image image

  Input parameter. The bm_image object to be judged.


**Return parameters description:**

1.  If the bm_image object is not created, a false will be returned;

2. Whether the returning bm_image object of the function is associated with a piece of device memory. If it is associated, it will return true; otherwise, it will return false.


**Note:**

1. In general, calling the bmcv API requires the incoming bm_image object to be associated with device memory. Otherwise, it will return failure. If the output bm_image object is not associated with device memory, it will internally call the bm_image_alloc_dev_mem function and apply internal memory.

2. The applied memory when bm_image calls bm_image_alloc_dev_mem is automatically managed internally. The memory will be automatically released without the management of the caller when calling bm_image_destroy, bm_image_detach, bm_image_attach and other device memory. Conversely, if the bm_image_attach is connected with a device memory, it means that the memory will be managed by the caller. The memory will not be automatically released when calling bm_image_destroy, bm_image_detach, bm_image_attach or other device memory. It needs to be manually released by the caller.

3. At present, device memory is divided into three memory spaces: heap0, heap1 and heap2. The difference among the three is whether the hardware VPP module of the chip has reading permission. Therefore, if an API needs to be implemented by specifying the hardware VPP module, the input bm_image of the API must be guaranteed to be saved in heap1 or heap2.

   +------------------+------------------+------------------+
   |    heap id       |   bm1684 VPP     |   bm1684x VPP    |
   +==================+==================+==================+
   |    heap0         |     Unreadable   |     readable     |
   +------------------+------------------+------------------+
   |    heap1         |     readable     |     readable     |
   +------------------+------------------+------------------+
   |    heap2         |     readable     |     readable     |
   +------------------+------------------+------------------+
