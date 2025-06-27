BUILD_DIR=$(PWD)/build

CXXFLAGS+=-I./
APPSRCFILES+=./main.cpp

OPT_APP_OBJ=$(patsubst %.cpp,$(BUILD_DIR)/%.app.opt.o,$(APPSRCFILES))
LOW_DBG_APP_OBJ=$(patsubst %.cpp,$(BUILD_DIR)/%.app.low.o,$(APPSRCFILES))
HIGH_DBG_APP_OBJ=$(patsubst %.cpp,$(BUILD_DIR)/%.app.high.o,$(APPSRCFILES))

ESP_INCDIR += -I$(ESP_ROOT)/accelerators/stratus_hls/audio_fft_stratus/sw/linux/include

APP_NAME ?= 01_audio_fft_mt

%.app.opt.o: ./main.cpp $(HPP_FILES)
	$(CXX) $(CXXFLAGS) $< -c -o $@

%.app.low.o: ./main.cpp $(HPP_FILES)
	$(CXX) $(CXXFLAGS) -DLOW_VERBOSE $< -c -o $@

%.app.high.o: ./main.cpp $(HPP_FILES)
	$(CXX) $(CXXFLAGS) -DHIGH_VERBOSE $< -c -o $@

include ../../Makefile