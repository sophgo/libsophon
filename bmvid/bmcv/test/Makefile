##############################################################################
#
#    Copyright (c) 2016-2022 by Bitmain Technologies Inc. All rights reserved.
#
#    The material in this file is confidential and contains trade secrets
#    of Bitmain Technologies Inc. This is proprietary information owned by
#    Bitmain Technologies Inc. No part of this work may be disclosed,
#    reproduced, copied, transmitted, or used in any way for any purpose,
#    without the express written permission of Bitmain Technologies Inc.
#
##############################################################################

TEST_BMCV_DIR = $(BMCV_SRC_DIR)/test

BMCV_DIR ?= $(OUT_DIR)/bmcv

INCLUDE_DIR += -I$(TEST_BMCV_DIR)

#TEST_BMCV_SRCS_CXX = $(wildcard $(TEST_BMCV_DIR)/*.cpp wildcard $(TEST_BMCV_DIR)/bm1684x/*.cpp)
TEST_BMCV_SRCS_CXX = $(TEST_BMCV_DIR)/test_convert_to.cpp  \
                     $(TEST_BMCV_DIR)/test_cv_absdiff.cpp  \
					 $(TEST_BMCV_DIR)/test_cv_add_weighted.cpp \
					 $(TEST_BMCV_DIR)/test_cv_axpy.cpp   \
					 $(TEST_BMCV_DIR)/test_cv_bitwise.cpp  \
					 $(TEST_BMCV_DIR)/test_cv_base64.cpp  \
					 $(TEST_BMCV_DIR)/test_cv_batch_topk.cpp \
					 $(TEST_BMCV_DIR)/test_cv_bgrsplit.cpp  \
					 $(TEST_BMCV_DIR)/test_cv_calc_hist.cpp \
					 $(TEST_BMCV_DIR)/test_cv_canny.cpp  \
					 $(TEST_BMCV_DIR)/test_cv_cmulp.cpp  \
					 $(TEST_BMCV_DIR)/test_cv_copy_to.cpp  \
					 $(TEST_BMCV_DIR)/test_cv_crop.cpp  \
					 $(TEST_BMCV_DIR)/test_cv_dct.cpp  \
					 $(TEST_BMCV_DIR)/test_cv_distance.cpp \
  					 $(TEST_BMCV_DIR)/test_cv_draw_lines.cpp  \
  					 $(TEST_BMCV_DIR)/test_cv_draw_rectangle.cpp \
  					 $(TEST_BMCV_DIR)/test_cv_feature_match.cpp \
  					 $(TEST_BMCV_DIR)/test_cv_fft_1d.cpp \
  					 $(TEST_BMCV_DIR)/test_cv_fft_2d.cpp \
  					 $(TEST_BMCV_DIR)/test_cv_fill_rectangle.cpp \
					 $(TEST_BMCV_DIR)/test_cv_fusion.cpp   \
  					 $(TEST_BMCV_DIR)/test_cv_gaussian_blur.cpp  \
  					 $(TEST_BMCV_DIR)/test_cv_gemm.cpp  \
  					 $(TEST_BMCV_DIR)/test_cv_image_align.cpp  \
  					 $(TEST_BMCV_DIR)/test_cv_image_transpose.cpp   \
  					 $(TEST_BMCV_DIR)/test_cv_img_scale.cpp   \
  					 $(TEST_BMCV_DIR)/test_cv_jpeg.cpp   \
  					 $(TEST_BMCV_DIR)/test_cv_json.cpp   \
  					 $(TEST_BMCV_DIR)/test_cv_laplacian.cpp   \
  					 $(TEST_BMCV_DIR)/test_cv_lkpyramid.cpp   \
					 $(TEST_BMCV_DIR)/test_cv_matmul_t_opt.cpp   \
  					 $(TEST_BMCV_DIR)/test_cv_matmul.cpp  \
  					 $(TEST_BMCV_DIR)/test_cv_min_max.cpp \
  					 $(TEST_BMCV_DIR)/test_cv_morph.cpp   \
  					 $(TEST_BMCV_DIR)/test_cv_multi_crop_resize.cpp  \
  					 $(TEST_BMCV_DIR)/test_cv_nms.cpp   \
  					 $(TEST_BMCV_DIR)/test_cv_put_text.cpp   \
  					 $(TEST_BMCV_DIR)/test_cv_pyramid.cpp   \
  					 $(TEST_BMCV_DIR)/test_cv_sobel.cpp   \
  					 $(TEST_BMCV_DIR)/test_cv_sort.cpp   \
  					 $(TEST_BMCV_DIR)/test_cv_split.cpp  \
  					 $(TEST_BMCV_DIR)/test_cv_storage_convert.cpp   \
  					 $(TEST_BMCV_DIR)/test_cv_threshold.cpp  \
  					 $(TEST_BMCV_DIR)/test_cv_transpose.cpp  \
  					 $(TEST_BMCV_DIR)/test_cv_vpp.cpp  \
  					 $(TEST_BMCV_DIR)/test_cv_vpp_border.cpp  \
  					 $(TEST_BMCV_DIR)/test_cv_vpp_loop.cpp   \
  					 $(TEST_BMCV_DIR)/test_cv_vpp_random.cpp  \
  					 $(TEST_BMCV_DIR)/test_cv_vpp_stitch.cpp  \
  					 $(TEST_BMCV_DIR)/test_cv_warp.cpp  \
					 $(TEST_BMCV_DIR)/test_cv_warp_affine.cpp  \
					 $(TEST_BMCV_DIR)/test_cv_warp_bilinear.cpp  \
					 $(TEST_BMCV_DIR)/test_cv_warp_perspective.cpp  \
                                         $(TEST_BMCV_DIR)/test_cv_warp_perspective_bilinear.cpp  \
					 $(TEST_BMCV_DIR)/test_cv_width_align.cpp  \
					 $(TEST_BMCV_DIR)/test_cv_yuv2hsv.cpp \
					 $(TEST_BMCV_DIR)/test_cv_yuv2rgb.cpp  \
					 $(TEST_BMCV_DIR)/test_faiss_indexflatIP.cpp  \
					 $(TEST_BMCV_DIR)/test_faiss_indexflatL2.cpp  \
					 $(TEST_BMCV_DIR)/test_faiss_indexPQ.cpp  \
					 $(TEST_BMCV_DIR)/test_perf_bmcv.cpp  \
					 $(TEST_BMCV_DIR)/test_perf_vpp.cpp  \
					 $(TEST_BMCV_DIR)/test_resize.cpp  \
					 $(TEST_BMCV_DIR)/test_cv_hm_distance.cpp  \
					 $(TEST_BMCV_DIR)/test_yolov3_detect_out.cpp  \
					 $(TEST_BMCV_DIR)/bm1684x/test_bm1684x_fbd.cpp  \
					 $(TEST_BMCV_DIR)/bm1684x/test_bm1684x_slt.cpp   \
					 $(TEST_BMCV_DIR)/bm1684x/test_bm1684x_vpp.cpp   \
					 $(TEST_BMCV_DIR)/bm1684x/test_bm1684x_vpp_ax+b.cpp   \
					 $(TEST_BMCV_DIR)/bm1684x/test_bm1684x_vpp_basic.cpp  \
					 $(TEST_BMCV_DIR)/bm1684x/test_bm1684x_vpp_border.cpp     \
					 $(TEST_BMCV_DIR)/bm1684x/test_bm1684x_vpp_random.cpp   \
					 $(TEST_BMCV_DIR)/bm1684x/test_bm1684x_vpp_resize.cpp   \
					 $(TEST_BMCV_DIR)/bm1684x/test_bm1684x_vpp_stitch.cpp  \
					 $(TEST_BMCV_DIR)/bm1684x/test_bm1684x_vpp_mosaic.cpp   \
					 $(TEST_BMCV_DIR)/bm1684x/test_bm1684x_vpp_convert.cpp    \
					 $(TEST_BMCV_DIR)/bm1684x/test_bm1684x_vpp_padding.cpp      \
					 $(TEST_BMCV_DIR)/bm1684x/test_bm1684x_vpp_comparison.cpp    \
					 $(TEST_BMCV_DIR)/bm1684x/test_bm1684x_vpp_border_comapre.cpp \
					 $(TEST_BMCV_DIR)/bm1684x/test_bm1684x_vpp_fill_rectangle.cpp  \
					 $(TEST_BMCV_DIR)/bm1684x/test_bm1684x_vpp_postpadding-compare.cpp  \
					 $(TEST_BMCV_DIR)/bm1684x/test_bm1684x_vpp_postpadding-haikang.cpp    \
					 $(TEST_BMCV_DIR)/bm1684x/test_bm1684x_vpp_watermask_superpose.cpp     \
					 $(TEST_BMCV_DIR)/bm1684x/test_bm1684x_vpp_convert_to_comparison.cpp   \
					 $(TEST_BMCV_DIR)/bm1684x/test_bm1684x_vpp_mul_crop_resize_haikang.cpp  \

TEST_BMCV_CXX_EXE1 = $(patsubst %.cpp,%,$(TEST_BMCV_SRCS_CXX))
TEST_BMCV_CXX_EXE2 = $(patsubst $(BMVID_ROOT)/%,%,$(TEST_BMCV_CXX_EXE1))
TEST_BMCV_CXX_EXE ?= $(addprefix $(OUT_DIR)/,$(TEST_BMCV_CXX_EXE2))
TEST_BMCV_CXX_EXE_TEMP = $(addprefix $(OUT_DIR)/,$(TEST_BMCV_CXX_EXE2))

TEST_BMCV_OBJ_DIR = $(sort $(dir $(TEST_BMCV_CXX_EXE)))
INSTALL_EXES += $(TEST_BMCV_CXX_EXE)
DAILY_TEST_EXES += $(TEST_BMCV_CXX_EXE)

BMLIB_DYN =  $(BMVID_ROOT)/3rdparty/libbmcv/lib/$(PRODUCTFORM)/libbmlib.so

$(TEST_BMCV_OBJ_DIR):
	@$(MKDIR) $@

INCLUDE_DIR += -I$(BMVID_ROOT)/bmcv/src/bm1684x/
INCLUDE_DIR += -I$(BMVID_ROOT)/bmcv/src/

ifeq ($(USING_CMODEL),1)
INCLUDE_DIR += -I$(BMVID_ROOT)/bmcv/include

TEST_BMCV_LDFLAGS = $(LDFLAGS) -L$(BMCV_DIR)/lib/cmodel/ \
 -l:libbmcv.a -lbmlib \
 -L$(BMVID_ROOT)/3rdparty/libbmcv/lib/pcie/pcie_cmodel/ \
-ldl

$(TEST_BMCV_CXX_EXE): $(OUT_DIR)/%:$(BMVID_ROOT)/%.cpp $(BMLIB_RUNTIME_STATIC) | $(TEST_BMCV_OBJ_DIR)
	@echo cc -o $@ $<
	@$(CXX) $(INCLUDE_DIR) $(CXXFLAGS) -Wextra -o $@ $< $(TEST_BMCV_LDFLAGS) -std=c++0x -pthread

else
TEST_BMCV_STATIC = -std=c++0x -pthread
# INCLUDE_DIR +=
INCLUDE_DIR += -I$(BMVID_ROOT)/bmcv/include \
			   -I$(BMVID_ROOT)/vpp/driver/include/bm1684 \
			   -I$(BMVID_ROOT)/vpp/driver/include/ \
			   -I$(BMVID_ROOT)/jpeg/driver/bmjpuapi/inc \
			   -I$(BMVID_ROOT)/allocator/ion/inc

TEST_BMCV_LDFLAGS = $(LDFLAGS) -L$(JPU_DIR)/lib/ -L$(BMION_DIR)/lib/ \
					-L$(OUT_DIR)/lib/ -L$(BMCV_DIR)/lib/ \
					-L$(BMVID_ROOT)/3rdparty/libbmcv/lib/$(PRODUCTFORM) \
					-lbmjpuapi -lbmjpulite -lbmion -lbmcv -lbmlib -ldl\

TEST_MISC_CXX = $(TEST_BMCV_DIR)/test_misc.cpp $(TEST_BMCV_DIR)/md5.cpp

$(TEST_BMCV_CXX_EXE): $(OUT_DIR)/%:$(BMVID_ROOT)/%.cpp $(BMLIB_DYN) | $(TEST_BMCV_OBJ_DIR)
	@echo cc -o $@ $<
	@$(CXX) $(INCLUDE_DIR)  $(CXXFLAGS) -o $@ $< $(TEST_MISC_CXX) $(TEST_BMCV_LDFLAGS) $(TEST_BMCV_STATIC)
endif

test_bmcv: $(TEST_BMCV_CXX_EXE) $(BMCV_CPU_LIB)
	@echo $(BMCV_CPU_LIB) "this is bmcpu lib"
	@cp -rf $(BMCV_SRC_DIR)/test/image $(OUT_DIR)/bmcv/test/

clean_test:
	@echo rm -rf $(OUT_DIR)/bmcv/test/
	@rm -rf $(OUT_DIR)/bmcv/test/
	@rm -rf $(TEST_BMCV_OBJ_DIR)

BM_TEST_BMCV_DEP = $(patsubst %.cpp,%.d,$(TEST_BMCV_SRCS_CXX))
-include $(BM_TEST_BMCV_DEP)
