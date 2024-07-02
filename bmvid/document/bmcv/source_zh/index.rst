.. BMCV documentation master file, created by
   sphinx-quickstart on Fri May 31 14:55:12 2019.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

BMCV 开发参考手册
===============================


声明
-------------------------------
.. toctree::
   :maxdepth: 4

   0_disclaimer

BMCV 介绍
-------------------------
.. toctree::
   :glob:

   bmcv

bm_image 介绍
-------------------------
.. toctree::
   :glob:

   bm_image/bm_image
   bm_image/bm_image_create
   bm_image/bm_image_destroy
   bm_image/bm_image_copy_host_to_device
   bm_image/bm_image_copy_device_to_host
   bm_image/bm_image_attach
   bm_image/bm_image_detach
   bm_image/bm_image_alloc_dev_mem
   bm_image/bm_image_alloc_dev_mem_heap_mask
   bm_image/bm_image_get_byte_size
   bm_image/bm_image_get_device_mem
   bm_image/bm_image_alloc_contiguous_mem
   bm_image/bm_image_alloc_contiguous_mem_heap_mask
   bm_image/bm_image_free_contiguous_mem
   bm_image/bm_image_attach_contiguous_mem
   bm_image/bm_image_dettach_contiguous_mem
   bm_image/bm_image_get_contiguous_device_mem
   bm_image/bm_image_get_format_info
   bm_image/bm_image_get_stride
   bm_image/bm_image_get_plane_num
   bm_image/bm_image_is_attached
   bm_image/bm_image_get_handle
   bm_image/bm_image_write_to_bmp
   bm_image/bmcv_calc_cbcr_addr


bm_image device memory 管理
----------------------------
.. toctree::
   :glob:

   memory

BMCV API
-------------------------
.. toctree::
   :glob:

   api/api_introduct
   api/bmcv_hist_balance
   api/yuv2bgr
   api/warp_affine
   api/warp_perspective
   api/watermask_superpose
   api/crop
   api/resize
   api/convert_to
   api/csc_convert_to
   api/storage_convert
   api/vpp_basic
   api/vpp_convert
   api/vpp_convert_padding
   api/vpp_stitch
   api/vpp_csc_matrix_convert
   api/jpeg_encode
   api/jpeg_decode
   api/copy_to
   api/draw_lines
   api/draw_point
   api/draw_rectangle
   api/put_text
   api/fill_rectangle
   api/absdiff
   api/bitwise_and
   api/bitwise_or
   api/bitwise_xor
   api/add_weighted
   api/threshold
   api/dct
   api/sobel
   api/canny
   api/yuv2hsv
   api/gaussian_blur
   api/transpose
   api/morph
   api/mosaic
   api/laplacian
   api/lkpyramid
   api/debug_savedata
   api/sort
   api/base64
   api/feature_match_fix8b
   api/gemm
   api/gemm_ext
   api/matmul
   api/distance
   api/min_max
   api/fft
   api/calc_hist
   api/nms
   api/nms_ext
   api/yolo_nms
   api/cmulp
   api/faiss_indexflatIP
   api/faiss_indexflatL2
   api/batch_topk
   api/hm_distance
   api/axpy
   api/pyramid
   api/bayer2rgb
   api/as_strided
   api/quantify

PCIe CPU
--------
.. toctree::
   :glob:

   pcie_cpu




