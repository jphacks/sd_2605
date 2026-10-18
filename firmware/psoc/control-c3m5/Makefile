MTB_TYPE=COMBINED
TARGET=KIT_PSC3M5_EVK
APPNAME=control-c3m5
TOOLCHAIN=GCC_ARM
CONFIG=Debug
CY_GETLIBS_SHARED_PATH=../
CY_GETLIBS_SHARED_NAME=mtb_shared

CY_WIN_HOME=$(subst \,/,$(USERPROFILE))
CY_TOOLS_PATHS ?= $(wildcard \
    $(CY_WIN_HOME)/ModusToolbox/tools_* \
    $(HOME)/ModusToolbox/tools_* \
    /Applications/ModusToolbox/tools_*)
CY_TOOLS_DIR=$(lastword $(sort $(wildcard $(CY_TOOLS_PATHS))))

ifeq ($(CY_TOOLS_DIR),)
$(error Unable to find ModusToolbox tools. Set CY_TOOLS_PATHS to the tools_3.9 directory.)
endif

include $(CY_TOOLS_DIR)/make/start.mk
