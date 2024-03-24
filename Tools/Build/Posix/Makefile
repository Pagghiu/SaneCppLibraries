# Copyright (c) Stefano Cristiano
# SPDX-License-Identifier: MIT
ifndef CONFIG
 CONFIG=Debug
endif

ifndef TOOL_SOURCE_DIR
$(error "TOOL_SOURCE_DIR is required")
endif

ifndef TOOL_OUTPUT_DIR
$(error "TOOL_OUTPUT_DIR is required")
endif

ifndef TOOL
$(error "TOOL is required")
endif

TARGET_OS := $(shell uname)

.PHONY: clean all
all: SCTool_BUILD
clean: SCTool_CLEAN

# Force a clean when makefile is modified
$(TOOL_OUTPUT_DIR)/_Intermediates/Makefile.$(TARGET_OS).$(CONFIG).touched: Makefile
	@mkdir -p "$(TOOL_OUTPUT_DIR)/_Intermediates"
	@touch $@
	@$(MAKE) clean

# Implicitly evaluate the makefile rebuild force clean during parsing
-include $(TOOL_OUTPUT_DIR)/_Intermediates/Makefile.$(TARGET_OS).$(CONFIG).touched

SCTool_TARGET_NAME := $(TOOL)

SCTool_TARGET_DIR := $(TOOL_OUTPUT_DIR)/$(TARGET_OS)
SCTool_INTERMEDIATE_DIR := $(TOOL_OUTPUT_DIR)/_Intermediates/$(TARGET_OS)

ifeq ($(CONFIG),Debug)
SCTool_CONFIG_FLAGS := -D_DEBUG=1 -g -ggdb -O0
else
SCTool_CONFIG_FLAGS := -DNDEBUG=1 -O3
endif

SCTool_COMMON_FLAGS := "-I../../.." -DSC_COMPILER_ENABLE_STD_CPP=1
SCTool_CPPFLAGS := $(SCTool_COMMON_FLAGS) $(SCTool_CONFIG_FLAGS) $(CPPFLAGS)
SCTool_CXXFLAGS := $(SCTool_CPPFLAGS) -std=c++14 -fstrict-aliasing -fvisibility=hidden -fvisibility-inlines-hidden -fno-rtti -nostdinc++ -fno-exceptions $(CXXFLAGS)

SCTool_FRAMEWORKS := -framework CoreFoundation -framework CoreServices
SCTool_LIBRARIES := -ldl -lpthread

ifeq ($(TARGET_OS),Linux)
     SCTool_OS_LDFLAGS := -rdynamic
else 
     SCTool_OS_LDFLAGS := $(SCTool_FRAMEWORKS)
endif

SCTool_LDFLAGS := $(SCTool_TARGET_FLAGS) $(SCTool_LIBRARIES) $(SCTool_OS_LDFLAGS) $(LDFLAGS)
SCTool_CLEAN:
	@echo Cleaning $(TOOL)
	@rm -rf $(SCTool_TARGET_DIR)/$(TARGET) $(SCTool_INTERMEDIATE_DIR)

SCTool_OBJECT_FILES := \
$(SCTool_INTERMEDIATE_DIR)/Tools.o \
$(SCTool_INTERMEDIATE_DIR)/$(TOOL).o

# Rebuild object files when an header dependency changes
-include $(SCTool_OBJECT_FILES:.o=.d)

$(SCTool_INTERMEDIATE_DIR):
	@echo Creating "$(SCTool_INTERMEDIATE_DIR)"
	@mkdir -p $@

$(SCTool_TARGET_DIR):
	@echo Creating "$(SCTool_TARGET_DIR)"
	@mkdir -p $@

SCTool_BUILD: $(SCTool_TARGET_DIR)/$(SCTool_TARGET_NAME)

$(SCTool_TARGET_DIR)/$(SCTool_TARGET_NAME): $(SCTool_OBJECT_FILES) | $(SCTool_TARGET_DIR)
	@echo Linking "$(SCTool_TARGET_NAME)"
	@$(CXX) -o $(SCTool_TARGET_DIR)/$(SCTool_TARGET_NAME) $(SCTool_OBJECT_FILES) $(SCTool_LDFLAGS)

$(SCTool_INTERMEDIATE_DIR)/Tools.o: $(CURDIR)/../../../Tools/Tools.cpp | $(SCTool_INTERMEDIATE_DIR)
	@echo "Compiling Tools.cpp"
	@$(CXX) $(SCTool_CXXFLAGS) -o "$@" -MMD -pthread -c "$<"

$(SCTool_INTERMEDIATE_DIR)/$(TOOL).o: $(TOOL_SOURCE_DIR)/$(TOOL).cpp | $(SCTool_INTERMEDIATE_DIR)
	@echo "Compiling $(TOOL).cpp"
	@$(CXX) $(SCTool_CXXFLAGS) -o "$@" -MMD -pthread -c "$<"
