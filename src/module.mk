# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

BINARY_NAME := aver-updater

PC_DEPS = libbrillo-$(BASE_VER) libchrome-$(BASE_VER) libarchive
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))
LDLIBS += $(PC_LIBS)

CPPFLAGS += $(PC_CFLAGS)

all: CXX_BINARY($(BINARY_NAME))

CXX_BINARY($(BINARY_NAME)): \
	src/main.o \
	src/utilities.o \
	src/usb_device.o \
	src/model_one_device.o \
	src/model_two_device.o \
	src/target_device.o \

clean: CLEAN(CXX_BINARY($(BINARY_NAME)))
