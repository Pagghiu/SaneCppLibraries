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

space := $(null) $(null)
TOOL_SOURCE_DIR_ESCAPED = $(subst $(space),\$(space),$(TOOL_SOURCE_DIR))
TOOL_OUTPUT_DIR_ESCAPED = $(subst $(space),\$(space),$(TOOL_OUTPUT_DIR))
CURDIR_ESCAPED = $(subst $(space),\$(space),$(CURDIR))

ifndef TOOL
$(error "TOOL is required")
endif

TARGET_OS := $(shell uname)

CLANG_DETECTED := $(shell $(CXX) --version 2>&1 | grep -q clang && echo "yes")

.PHONY: clean all
all: SCTool_BUILD
build: SCTool_BUILD
clean: SCTool_CLEAN

# Force a clean when makefile is modified
$(TOOL_OUTPUT_DIR_ESCAPED)/_Intermediates/Makefile.$(TARGET_OS).$(CONFIG).touched: Makefile
	@mkdir -p $(TOOL_OUTPUT_DIR_ESCAPED)/_Intermediates
	@touch "$@"
	@echo " " >> "$@" # touch doesn't set proper modification date on hgfs (VMWare)
	@$(MAKE) clean

# Implicitly evaluate the makefile rebuild force clean during parsing
-include $(TOOL_OUTPUT_DIR_ESCAPED)/_Intermediates/Makefile.$(TARGET_OS).$(CONFIG).touched

SCTool_TARGET_NAME := $(TOOL)

SCTool_TARGET_DIR := $(TOOL_OUTPUT_DIR_ESCAPED)/$(TARGET_OS)
SCTool_INTERMEDIATE_DIR := $(TOOL_OUTPUT_DIR_ESCAPED)/_Intermediates/$(TARGET_OS)
SCTool_TARGET_DIR_UNESCAPED := $(TOOL_OUTPUT_DIR)/$(TARGET_OS)
SCTool_INTERMEDIATE_DIR_UNESCAPED := $(TOOL_OUTPUT_DIR)/_Intermediates/$(TARGET_OS)

ifeq ($(CONFIG),Debug)
SCTool_CONFIG_FLAGS := -D_DEBUG=1 -g -ggdb -O0
else
SCTool_CONFIG_FLAGS := -DNDEBUG=1 -O3
endif

SCTool_COMMON_FLAGS := "-I../../.."

SCTool_CPPFLAGS := $(CFLAGS) $(SCTool_COMMON_FLAGS) $(SCTool_CONFIG_FLAGS) $(CPPFLAGS)
SCTool_CXXFLAGS := $(CFLAGS) $(SCTool_COMMON_FLAGS) $(SCTool_CONFIG_FLAGS) $(CXXFLAGS)

SCTool_CXXFLAGS += -std=c++14 -fstrict-aliasing -fvisibility=hidden -fvisibility-inlines-hidden -fno-rtti -fno-exceptions -MMD -pthread
ifeq ($(CLANG_DETECTED),yes)
SCTool_CXXFLAGS += -nostdinc++ 
else
SCTool_CXXFLAGS += -DSC_COMPILER_ENABLE_STD_CPP=1 # Only GCC 13+ supports nostdlib++
endif

SCTool_FRAMEWORKS := -framework CoreFoundation -framework CoreServices
SCTool_LIBRARIES := -ldl -lpthread

ifeq ($(TARGET_OS),Linux)
     SCTool_OS_LDFLAGS := -rdynamic
else 
     SCTool_OS_LDFLAGS := $(SCTool_FRAMEWORKS)
endif

ifeq ($(CLANG_DETECTED),yes)
SCTool_OS_LDFLAGS += -nostdlib++
else
SCTool_OS_LDFLAGS +=
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
	@echo Creating \"$(SCTool_INTERMEDIATE_DIR_UNESCAPED)\"
	@mkdir -p $(SCTool_INTERMEDIATE_DIR)

$(SCTool_TARGET_DIR):
	@echo Creating \"$(SCTool_TARGET_DIR_UNESCAPED)\"
	@mkdir -p $(SCTool_TARGET_DIR)

SCTool_BUILD: $(SCTool_TARGET_DIR)/$(SCTool_TARGET_NAME)

$(SCTool_TARGET_DIR)/$(SCTool_TARGET_NAME): $(SCTool_OBJECT_FILES) | $(SCTool_TARGET_DIR)
	@echo Linking "$(SCTool_TARGET_NAME)"
	@$(CXX) -o $(SCTool_TARGET_DIR)/$(SCTool_TARGET_NAME) $(SCTool_OBJECT_FILES) $(SCTool_LDFLAGS)

$(SCTool_INTERMEDIATE_DIR)/Tools.o: $(CURDIR_ESCAPED)/../../../Tools/Tools.cpp | $(SCTool_INTERMEDIATE_DIR)
	@echo "Compiling Tools.cpp"
	@$(CXX) $(SCTool_CXXFLAGS) -o "$@" -c "$(subst \$(space),$(space),$<)"

$(SCTool_INTERMEDIATE_DIR)/$(TOOL).o: $(TOOL_SOURCE_DIR_ESCAPED)/$(TOOL).cpp | $(SCTool_INTERMEDIATE_DIR)
	@echo "Compiling $(TOOL).cpp"
	@$(CXX) $(SCTool_CXXFLAGS) -o "$@" -c "$(subst \$(space),$(space),$<)"

