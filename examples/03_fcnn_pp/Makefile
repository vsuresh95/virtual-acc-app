BUILD_DIR=$(PWD)/build
APP_NAME?=$(notdir $(PWD))

CFLAGS+=-I./
CFLAGS+=-DPINGPONG_EN
# CFLAGS+=-DDO_CHAIN
APPSRCFILES+=$(PWD)/main.c

OPT_APP_OBJ=$(patsubst $(PWD)/%.c,$(BUILD_DIR)/%.app.opt.o,$(APPSRCFILES))
LOW_DBG_APP_OBJ=$(patsubst $(PWD)/%.c,$(BUILD_DIR)/%.app.low.o,$(APPSRCFILES))
HIGH_DBG_APP_OBJ=$(patsubst $(PWD)/%.c,$(BUILD_DIR)/%.app.high.o,$(APPSRCFILES))

include ../../Makefile
