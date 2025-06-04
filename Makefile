CC=riscv64-unknown-linux-gnu-gcc
CXX=riscv64-unknown-linux-gnu-g++
LD=riscv64-unknown-linux-gnu-g++

CFLAGS=-Wall -fPIC -I./include -O3
CXXFLAGS=-std=c++17 -Wall -fPIC -I./include -I./portaudio/include -Wno-overloaded-virtual
LD_LIBS=-lpthread -pthread

ifdef VERBOSE
CXXFLAGS += -DVERBOSE
endif

HPP_FILES := $(shell find -L . -name '*.hpp')
HPP_FILES := $(patsubst ./%,%,$(HPP_FILES))

SRCFILES+=vam-module.cpp
SRCFILES+=vam-df-graph.cpp
SRCFILES+=vam-hpthread.cpp
SRCFILES+=vam-mem-helper.cpp

COMMON_OBJ=$(patsubst %.cpp,%.common.o,$(SRCFILES))

# AUDIO CHANGES
CXXFLAGS+=-I./audio
AUDIOSRCFILES+=audio-worker.cpp
AUDIOSRCFILES+=audio-app.cpp
AUDIOSRCFILES+=audio-sw-func.cpp

AUDIO_OBJ=$(patsubst %.cpp,%.audio.o,$(AUDIOSRCFILES))

CROSS_COMPILE ?= riscv64-unknown-linux-gnu-

ESP_BUILD_DRIVERS = ${PWD}/esp-build/drivers

ESP_DRIVERS ?= $(ESP_ROOT)/soft/common/drivers
ESP_DRV_LINUX = $(ESP_DRIVERS)/linux

ESP_INCDIR += -I${ESP_DRIVERS}/common/include
ESP_INCDIR += -I${ESP_DRIVERS}/linux/include

ESP_LD_LIBS += -L$(ESP_BUILD_DRIVERS)/contig_alloc
ESP_LD_LIBS += -L$(ESP_BUILD_DRIVERS)/test
ESP_LD_LIBS += -L$(ESP_BUILD_DRIVERS)/libesp

ESP_LD_FLAGS += -lrt
ESP_LD_FLAGS += -lesp
ESP_LD_FLAGS += -ltest
ESP_LD_FLAGS += -lcontig

ESP_INCDIR += -I$(ESP_ROOT)/accelerators/stratus_hls/audio_fft_stratus/sw/linux/include
ESP_INCDIR += -I$(ESP_ROOT)/accelerators/stratus_hls/audio_fir_stratus/sw/linux/include
ESP_INCDIR += -I$(ESP_ROOT)/accelerators/stratus_hls/audio_ffi_stratus/sw/linux/include

CXXFLAGS += $(ESP_INCDIR) $(ESP_LD_LIBS)
LD_LIBS += $(ESP_LD_FLAGS)

PP_EXE_FLAGS=-DPIPELINE_FFI

ESP_EXE_DIR = $(ESP_ROOT)/socs/xilinx-vcu118-xcvu9p/soft-build/ariane/sysroot/applications/test/

.PHONY: clean

all: virtual-app.exe virtual-app-pipeline.exe

virtual-app.exe: $(HPP_FILES) $(COMMON_OBJ) $(AUDIO_OBJ) esp-libs
	$(LD) $(CXXFLAGS) $(filter-out $(HPP_FILES) esp-libs,$^) -o $@ $(LD_LIBS)
	cp $@ ${ESP_EXE_DIR}

virtual-app-pipeline.exe: $(HPP_FILES) $(COMMON_OBJ) $(AUDIO_OBJ) esp-libs
	$(LD) $(CXXFLAGS) $(PP_EXE_FLAGS) $(filter-out $(HPP_FILES) esp-libs,$^) -o $@ $(LD_LIBS)
	cp $@ ${ESP_EXE_DIR}

%.common.o: source/%.cpp $(HPP_FILES)
	$(CXX) $(CXXFLAGS) $< -c -o $@

%.audio.o: audio/%.cpp $(HPP_FILES)
	$(CXX) $(CXXFLAGS) $< -c -o $@

clean: esp-build-distclean
	rm -rf *.o *.exe

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
	@ln -sf $(ESP_DRV_LINUX)/../common $(ESP_BUILD_DRIVERS)/../common

esp-build-distclean:
	$(QUIET_CLEAN)$(RM) -rf esp-build

esp-libs: esp-build
	CROSS_COMPILE=$(CROSS_COMPILE) DRIVERS=$(ESP_DRV_LINUX) $(MAKE) -C $(ESP_BUILD_DRIVERS)/contig_alloc/ libcontig.a
	cd $(ESP_BUILD_DRIVERS)/test; CROSS_COMPILE=$(CROSS_COMPILE) BUILD_PATH=$$PWD $(MAKE) -C $(ESP_DRV_LINUX)/test
	cd $(ESP_BUILD_DRIVERS)/libesp; CROSS_COMPILE=$(CROSS_COMPILE) BUILD_PATH=$$PWD $(MAKE) -C $(ESP_DRV_LINUX)/libesp
	cd $(ESP_BUILD_DRIVERS)/utils; CROSS_COMPILE=$(CROSS_COMPILE) BUILD_PATH=$$PWD $(MAKE) -C $(ESP_DRV_LINUX)/utils

