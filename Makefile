CC=riscv64-unknown-linux-gnu-gcc
CXX=riscv64-unknown-linux-gnu-g++
LD=riscv64-unknown-linux-gnu-g++

ROOT_DIR=$(ESP_ROOT)/soft/ariane/virtual-acc-app
LIB_DIR=$(ROOT_DIR)/lib
INCLUDE_DIR=$(ROOT_DIR)/include
ACCEL_DIR=$(ROOT_DIR)/accel_def

CFLAGS+=-Wall -fPIC -O3
CXXFLAGS+=-std=c++17 -Wall -fPIC -Wno-overloaded-virtual
LD_LIBS+=-lpthread -pthread

HPP_FILES := $(shell find -L $(ROOT_DIR) -name '*.h')

CXXFLAGS+=-I$(ROOT_DIR)/include/common
CXXFLAGS+=-I$(ROOT_DIR)/include/hpthread
CXXFLAGS+=-I$(ROOT_DIR)/include/vam
CXXFLAGS+=-I$(ROOT_DIR)/include/nn

LIB_FILES+=$(LIB_DIR)/hpthread/hpthread.cpp
LIB_FILES+=$(LIB_DIR)/hpthread/hpthread_intf.cpp
LIB_FILES+=$(LIB_DIR)/vam/vam_backend.cpp
LIB_FILES+=$(LIB_DIR)/nn/nn_module.cpp
LIB_FILES+=$(LIB_DIR)/nn/sw_kernels.cpp
LIB_FILES+=$(LIB_DIR)/nn/nn_frontend.cpp
LIB_FILES+=$(LIB_DIR)/nn/nn_graph.cpp
LIB_FILES+=$(LIB_DIR)/nn/nn_intf.cpp
LIB_FILES+=$(LIB_DIR)/nn/nn_helper.cpp

include $(ACCEL_DIR)/Makefile

OPT_LIB_OBJ=$(patsubst $(LIB_DIR)/%.cpp,$(BUILD_DIR)/%.lib.opt.o,$(LIB_FILES))
LOW_DBG_LIB_OBJ=$(patsubst $(LIB_DIR)/%.cpp,$(BUILD_DIR)/%.lib.low.o,$(LIB_FILES))
HIGH_DBG_LIB_OBJ=$(patsubst $(LIB_DIR)/%.cpp,$(BUILD_DIR)/%.lib.high.o,$(LIB_FILES))

OPT_ACCEL_OBJ=$(patsubst $(ACCEL_DIR)/%.cpp,$(BUILD_DIR)/%.accel.opt.o,$(ACCEL_FILES))
LOW_DBG_ACCEL_OBJ=$(patsubst $(ACCEL_DIR)/%.cpp,$(BUILD_DIR)/%.accel.low.o,$(ACCEL_FILES))
HIGH_DBG_ACCEL_OBJ=$(patsubst $(ACCEL_DIR)/%.cpp,$(BUILD_DIR)/%.accel.high.o,$(ACCEL_FILES))

CROSS_COMPILE ?= riscv64-unknown-linux-gnu-

ESP_BUILD_DRIVERS = $(BUILD_DIR)/esp-build/drivers

ESP_DRIVERS ?= $(ESP_ROOT)/soft/common/drivers
ESP_DRV_LINUX = $(ESP_DRIVERS)/linux

ESP_INCDIR += -I${ESP_DRIVERS}/common/include
ESP_INCDIR += -I${ESP_DRIVERS}/linux/include
ESP_INCDIR += -I${ESP_DRIVERS}/common/include/utils

ESP_LD_LIBS += -L$(ESP_BUILD_DRIVERS)/contig_alloc
ESP_LD_LIBS += -L$(ESP_BUILD_DRIVERS)/test
ESP_LD_LIBS += -L$(ESP_BUILD_DRIVERS)/libesp
ESP_LD_LIBS += -L$(ESP_BUILD_DRIVERS)/utils

ESP_LD_FLAGS += -lrt
ESP_LD_FLAGS += -lesp
ESP_LD_FLAGS += -ltest
ESP_LD_FLAGS += -lcontig
ESP_LD_FLAGS += -lutils

CXXFLAGS += $(ESP_INCDIR) $(ESP_LD_LIBS)
LD_LIBS += $(ESP_LD_FLAGS)

ESP_EXE_DIR = $(ESP_ROOT)/socs/xilinx-vcu118-xcvu9p-backup/soft-build/ariane/sysroot/applications/test

NPROCS = $(shell nproc || printf 1)
MAKEFLAGS += -j$(NPROCS)

APP_NAME ?= APP_NAME

.PHONY: clean build

all: build $(BUILD_DIR)/$(APP_NAME)/opt.exe $(BUILD_DIR)/$(APP_NAME)/low.dbg.exe $(BUILD_DIR)/$(APP_NAME)/high.dbg.exe
	cp -rf $(BUILD_DIR)/$(APP_NAME) ${ESP_EXE_DIR}
	@echo ""
	@echo "===================================================="
	@echo "  SUCCESS! Executables have been copied to: ";
	@echo "  OPT:${ESP_EXE_DIR}/$(APP_NAME)/opt.exe"
	@echo "  LOW DBG:${ESP_EXE_DIR}/$(APP_NAME)/low.opt.exe"
	@echo "  HIGH DBG:${ESP_EXE_DIR}/$(APP_NAME)/high.dbg.exe"
	@echo "===================================================="
	@echo ""

build:
	@mkdir -p $(BUILD_DIR)/hpthread
	@mkdir -p $(BUILD_DIR)/vam
	@mkdir -p $(BUILD_DIR)/nn
	@mkdir -p $(BUILD_DIR)/audio_fft
	@mkdir -p $(BUILD_DIR)/audio_fir
	@mkdir -p $(BUILD_DIR)/gemm
	@mkdir -p $(BUILD_DIR)/$(APP_NAME)

$(BUILD_DIR)/$(APP_NAME)/opt.exe: $(HPP_FILES) $(OPT_LIB_OBJ) $(OPT_ACCEL_OBJ) $(OPT_APP_OBJ) esp-libs
	$(LD) $(CXXFLAGS) $(filter-out $(HPP_FILES) esp-libs,$^) -o $@ $(LD_LIBS)

$(BUILD_DIR)/$(APP_NAME)/low.dbg.exe: $(HPP_FILES) $(LOW_DBG_LIB_OBJ) $(LOW_DBG_ACCEL_OBJ) $(LOW_DBG_APP_OBJ) esp-libs
	$(LD) $(CXXFLAGS) -DLOW_VERBOSE $(filter-out $(HPP_FILES) esp-libs,$^) -o $@ $(LD_LIBS)

$(BUILD_DIR)/$(APP_NAME)/high.dbg.exe: $(HPP_FILES) $(HIGH_DBG_LIB_OBJ) $(HIGH_DBG_ACCEL_OBJ) $(HIGH_DBG_APP_OBJ) esp-libs
	$(LD) $(CXXFLAGS) -DHIGH_VERBOSE $(filter-out $(HPP_FILES) esp-libs,$^) -o $@ $(LD_LIBS)

$(BUILD_DIR)/%.lib.opt.o: $(LIB_DIR)/%.cpp $(HPP_FILES)
	$(CXX) $(CXXFLAGS) $< -c -o $@

$(BUILD_DIR)/%.lib.low.o: $(LIB_DIR)/%.cpp $(HPP_FILES)
	$(CXX) $(CXXFLAGS) -DLOW_VERBOSE $< -c -o $@

$(BUILD_DIR)/%.lib.high.o: $(LIB_DIR)/%.cpp $(HPP_FILES)
	$(CXX) $(CXXFLAGS) -DHIGH_VERBOSE $< -c -o $@

$(BUILD_DIR)/%.accel.opt.o: $(ACCEL_DIR)/%.cpp $(HPP_FILES)
	$(CXX) $(CXXFLAGS) $< -c -o $@

$(BUILD_DIR)/%.accel.low.o: $(ACCEL_DIR)/%.cpp $(HPP_FILES)
	$(CXX) $(CXXFLAGS) -DLOW_VERBOSE $< -c -o $@

$(BUILD_DIR)/%.accel.high.o: $(ACCEL_DIR)/%.cpp $(HPP_FILES)
	$(CXX) $(CXXFLAGS) -DHIGH_VERBOSE $< -c -o $@

clean: esp-build-distclean
	rm -rf $(BUILD_DIR)

esp-build:
	@mkdir -p $(ESP_BUILD_DRIVERS)/contig_alloc
	@mkdir -p $(ESP_BUILD_DRIVERS)/esp
	@mkdir -p $(ESP_BUILD_DRIVERS)/esp_cache
	@mkdir -p $(ESP_BUILD_DRIVERS)/libesp
	@mkdir -p $(ESP_BUILD_DRIVERS)/probe
	@mkdir -p $(ESP_BUILD_DRIVERS)/test
	@mkdir -p $(ESP_BUILD_DRIVERS)/utils/baremetal
	@mkdir -p $(ESP_BUILD_DRIVERS)/utils/linux
	@ln -sf $(ESP_DRV_LINUX)/contig_alloc/* $(ESP_BUILD_DRIVERS)/contig_alloc
	@ln -sf $(ESP_DRV_LINUX)/esp/* $(ESP_BUILD_DRIVERS)/esp
	@ln -sf $(ESP_DRV_LINUX)/esp_cache/* $(ESP_BUILD_DRIVERS)/esp_cache
	@ln -sf $(ESP_DRV_LINUX)/driver.mk $(ESP_BUILD_DRIVERS)
	@ln -sf $(ESP_DRV_LINUX)/include $(ESP_BUILD_DRIVERS)
	@ln -sf $(ESP_DRV_LINUX)/../common $(ESP_BUILD_DRIVERS)/../

esp-build-distclean:
	$(QUIET_CLEAN)$(RM) -rf $(BUILD_DIR)/esp-build

esp-libs: esp-build
	CROSS_COMPILE=$(CROSS_COMPILE) DRIVERS=$(ESP_DRV_LINUX) $(MAKE) -C $(ESP_BUILD_DRIVERS)/contig_alloc/ libcontig.a
	cd $(ESP_BUILD_DRIVERS)/test; CROSS_COMPILE=$(CROSS_COMPILE) BUILD_PATH=$$PWD $(MAKE) -C $(ESP_DRV_LINUX)/test
	cd $(ESP_BUILD_DRIVERS)/libesp; CROSS_COMPILE=$(CROSS_COMPILE) BUILD_PATH=$$PWD $(MAKE) -C $(ESP_DRV_LINUX)/libesp
	cd $(ESP_BUILD_DRIVERS)/utils; CROSS_COMPILE=$(CROSS_COMPILE) BUILD_PATH=$$PWD $(MAKE) -C $(ESP_DRV_LINUX)/utils

