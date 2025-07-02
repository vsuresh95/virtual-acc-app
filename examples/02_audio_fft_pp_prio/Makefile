BUILD_DIR=$(PWD)/build
APP_NAME?=$(notdir $(PWD))

CXXFLAGS+=-I./
APPSRCFILES+=./main.cpp

OPT_APP_OBJ=$(patsubst %.cpp,$(BUILD_DIR)/%.app.opt.o,$(APPSRCFILES))
LOW_DBG_APP_OBJ=$(patsubst %.cpp,$(BUILD_DIR)/%.app.low.o,$(APPSRCFILES))
HIGH_DBG_APP_OBJ=$(patsubst %.cpp,$(BUILD_DIR)/%.app.high.o,$(APPSRCFILES))

%.app.opt.o: ./main.cpp $(HPP_FILES)
	$(CXX) $(CXXFLAGS) $< -c -o $@

%.app.low.o: ./main.cpp $(HPP_FILES)
	$(CXX) $(CXXFLAGS) -DLOW_VERBOSE $< -c -o $@

%.app.high.o: ./main.cpp $(HPP_FILES)
	$(CXX) $(CXXFLAGS) -DHIGH_VERBOSE $< -c -o $@

include ../../Makefile