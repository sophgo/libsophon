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

SHELL           := /bin/bash
BMODEL_ROOT     = $(shell pwd)
NETCOMPILER_TOP  ?= $(BMODEL_ROOT)/..
export           BMODEL_ROOT

##############################################################################
# Include common definitions.
##############################################################################

include Makefile.def

OUT_DIR       ?= $(BMODEL_ROOT)/build
OBJ_DIR       ?= $(BMODEL_ROOT)/obj
INSTALL_PATH  ?= $(NETCOMPILER_TOP)/out/install_bmodel

########MODEL_TOOL.bin##################
MODEL_TOOL    ?= $(OUT_DIR)/bm_model.bin
SRCS_CXX      += $(wildcard src/*.cpp)
SRCS_CXX      += $(wildcard tools/*.cpp)
SRCS_CXX      += $(wildcard tools/flatbuffers/*.cpp)
SRCS_CXX      += $(wildcard tools/md5/*.cpp)
OBJS_CXX       = $(addprefix $(OBJ_DIR)/, $(notdir $(SRCS_CXX:.cpp=.o)))
DEPS_CXX       = $(OBJS_CXX:.o=.d)

########model to source###############
FLATC         := ./model/flatc
MODEL_FILE     = model/model.fbs
MODEL_HEADER   = include/model_generated.h

INCLUDE_DIR = -I./include

CXXFLAGS    += $(INCLUDE_DIR)
CXXFLAGS    += '-DVER="3.1.0"'

all: $(OBJ_DIR) $(OUT_DIR) $(MODEL_TOOL)

.PHONY : clean install

$(OBJ_DIR):
	$(MKDIR) $@

$(OUT_DIR):
	$(MKDIR) $@

-include $(DEPS_CXX)

#because it will fail in some environment
#so need generate file manually
#$(MODEL_HEADER): $(MODEL_HEADER)
#	./gen_model_header.sh

$(OBJ_DIR)/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -MMD -c -o $@ $<

$(OBJ_DIR)/%.o: tools/%.cpp
	$(CXX) $(CXXFLAGS) -MMD -c -o $@ $<

$(OBJ_DIR)/%.o: tools/flatbuffers/%.cpp
	$(CXX) $(CXXFLAGS) -MMD -c -o $@ $<

$(OBJ_DIR)/%.o: tools/md5/%.cpp
	$(CXX) $(CXXFLAGS) -MMD -c -o $@ $<

$(MODEL_TOOL) : $(OBJS_CXX)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS_CXX)

clean:
	$(RM) $(OBJ_DIR) $(OUT_DIR)
	$(RM) $(INSTALL_PATH)/obj/*
	$(RM) $(INSTALL_PATH)/include/*
	$(RM) $(INSTALL_PATH)/tools/*

install:
	$(MKDIR)  $(INSTALL_PATH)
	$(MKDIR)  $(INSTALL_PATH)/include
	$(MKDIR)  $(INSTALL_PATH)/obj
	$(MKDIR)  $(INSTALL_PATH)/tools
	$(CP) -u $(BMODEL_ROOT)/include/* $(INSTALL_PATH)/include
	$(CP) -u $(OBJ_DIR)/bmodel.o $(INSTALL_PATH)/obj/bmodel.o
	$(CP) -u $(MODEL_TOOL) $(INSTALL_PATH)/tools
#	$(CP) -u $(MODEL_HEADER) $(INSTALL_PATH)/include
