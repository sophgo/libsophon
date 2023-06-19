#
# Copyright (c) 2016-2017, ARM Limited and Contributors. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

STDLIB_PATH := $(ROOT)/common/stdlib/

STDLIB_SRCS	:=	\
		$(STDLIB_PATH)/mem.c      \
		$(STDLIB_PATH)/subr_prf.c \
		$(STDLIB_PATH)/printf.c

C_SRC += $(STDLIB_SRCS)
