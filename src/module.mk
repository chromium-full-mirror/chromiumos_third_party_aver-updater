# Copyright 2017 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

BINARY_NAME := aver-updater

PC_DEPS = libbrillo libchrome libarchive
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

UNITTEST_LIBS := -lgtest -pthread -lpthread

CXX_BINARY(utilities-unittest): LDLIBS += $(UNITTEST_LIBS)
CXX_BINARY(utilities-unittest): \
	src/utilities.o \
	src/utilities_unittest.o \
	src/testrunner.o

tests: TEST(CXX_BINARY(utilities-unittest))
clean: CLEAN(CXX_BINARY(utilities-unittest))
