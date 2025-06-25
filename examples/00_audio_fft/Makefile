BUILD_DIR=$(PWD)/build

CXXFLAGS+=-I./
APPSRCFILES+=./main.cpp

APP_OBJ=$(patsubst %.cpp,$(BUILD_DIR)/%.app.o,$(APPSRCFILES))
DBG_APP_OBJ=$(patsubst %.cpp,$(BUILD_DIR)/%.app.dbg.o,$(APPSRCFILES))

ESP_INCDIR += -I$(ESP_ROOT)/accelerators/stratus_hls/audio_fft_stratus/sw/linux/include

APP_NAME = 00_audio_fft

%.app.o: ./main.cpp $(HPP_FILES)
	$(CXX) $(CXXFLAGS) $< -c -o $@

%.app.dbg.o: ./main.cpp $(HPP_FILES)
	$(CXX) $(CXXFLAGS) -DVERBOSE $< -c -o $@

include ../../Makefile 