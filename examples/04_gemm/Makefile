BUILD_DIR=$(PWD)/build
APP_NAME?=$(notdir $(PWD))

CXXFLAGS+=-I./
APPSRCFILES+=$(PWD)/main.cpp

OPT_APP_OBJ=$(patsubst $(PWD)/%.cpp,$(BUILD_DIR)/%.app.opt.o,$(APPSRCFILES))
LOW_DBG_APP_OBJ=$(patsubst $(PWD)/%.cpp,$(BUILD_DIR)/%.app.low.o,$(APPSRCFILES))
HIGH_DBG_APP_OBJ=$(patsubst $(PWD)/%.cpp,$(BUILD_DIR)/%.app.high.o,$(APPSRCFILES))

include ../../Makefile
