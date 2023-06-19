.. BMCV documentation master file, created by
   sphinx-quickstart on Fri May 31 14:55:12 2019.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

BMCV User Guide
=============================

.. figure:: ./logo.png
   :width: 400px
   :height: 400px
   :scale: 50%
   :align: center

| **法律声明**
| 版权所有 © 北京算能科技有限公司 2019。 保留一切权利。
| 非经本公司书面许可，任何单位和个人不得擅自摘抄、复制本文档内容的部分或全部，并不得以任何形式传播。

| **注意**
| 您购买的产品、服务或特性等应受算能公司商业合同和条款的约束，本文档中描述的全部或部分产品、服务或 特性可能不在您的购买或使用范围之内。除非合同另有约定，算能公司对本文档内容不做任何明示或默示的声明或保证。 由于产品版本升级或其他原因，本文档内容会不定期进行更新。除非另有约定，本文档仅作为使用指导，本文档中的所有陈述、信息和建议不构成任何明示或暗示的担保。

| **技术支持**
| 北京算能科技有限公司
| 地址：北京市海淀区丰豪东路9号院中关村集成电路设计园（ICpark）1号楼6F算能
| 邮编：100094
| 网址：www.sophon.ai
| 邮箱：info@sophgo.com

| **发布记录**

.. table::
    :widths: 20 25 55

    ========== ========== ==================
      版本       发布日期    说明
    ---------- ---------- ------------------
     V2.0.0    2019/09/20  创建
     V2.0.1    2019/11/10  新增接口
     V2.1.0    2020/05/01  优化
     V2.2.0    2020/11/01  优化/新增接口
     V2.3.0    2020/03/06  优化/新增接口
     V2.4.0    2021/06/19  优化/新增接口
     V2.5.0    2021/09/26  新增接口
    ========== ========== ==================


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


bm_image device memory 管理
----------------------------
.. toctree::
   :glob:

   memory

BMCV API
-------------------------
.. toctree::
   :glob:

   api/yuv2bgr
   api/warp_affine
   api/warp_perspective
   api/crop
   api/resize
   api/convert_to
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
   api/draw_rectangle
   api/put_text
   api/fill_rectangle
   api/absdiff
   api/add_weighted
   api/threshold
   api/dct
   api/sobel
   api/canny
   api/yuv2hsv
   api/gaussian_blur
   api/transpose
   api/morph
   api/laplacian
   api/lkpyramid
   api/debug_savedata
   api/sort
   api/base64
   api/feature_match_fix8b
   api/gemm
   api/matmul
   api/distance
   api/min_max
   api/fft
   api/calc_hist
   api/nms
   api/nms_ext

PCIe CPU
--------
.. toctree::
   :glob:

   pcie_cpu

Legacy API
----------
.. toctree::
   :glob:

   legacy/common_structures
   legacy/bmcv_img_bgrsplit
   legacy/bmcv_img_crop
   legacy/bmcv_img_scale
   legacy/bmcv_img_transpose
   legacy/bmcv_img_yuv2bgr


