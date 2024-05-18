# Copyright (c) Stefano Cristiano
# SPDX-License-Identifier: MIT

!ifndef CONFIG
CONFIG=Debug
!endif

!ifndef TOOL_SOURCE_DIR
!error "TOOL_SOURCE_DIR is required"
!endif

!ifndef TOOL_OUTPUT_DIR
!error "TOOL_OUTPUT_DIR is required"
!endif

!ifndef TOOL
!error "TOOL is required"
!endif

!if "$(OS)" == "Windows_NT"
TARGET_OS = Windows
!else
TARGET_OS = $(error Unsupported OS)
!endif

.SUFFIXES:
.SUFFIXES: .cpp .obj

# Make the main target depend on the sentinel file
all: $(TOOL_OUTPUT_DIR)\_Intermediates\Makefile.$(TARGET_OS).$(CONFIG).touched

# Default target
all: SCTool_BUILD
build: SCTool_BUILD
clean: SCTool_CLEAN

SCTool_TARGET_NAME = $(TOOL).exe

SCTool_TARGET_DIR = $(TOOL_OUTPUT_DIR)\$(TARGET_OS)
SCTool_INTERMEDIATE_DIR = $(TOOL_OUTPUT_DIR)\_Intermediates\$(TARGET_OS)

# Force a clean when makefile is modified
$(TOOL_OUTPUT_DIR)\_Intermediates\Makefile.$(TARGET_OS).$(CONFIG).touched: $(SCTool_INTERMEDIATE_DIR) Makefile
    @copy NUL "$(TOOL_OUTPUT_DIR)\_Intermediates\Makefile.$(TARGET_OS).$(CONFIG).touched" > NUL
	@ $(MAKE) /nologo clean

# Implicitly evaluate the makefile rebuild force clean during parsing
!if exist($(TOOL_OUTPUT_DIR)\_Intermediates\Makefile.$(TARGET_OS).$(CONFIG).touched)
!include $(TOOL_OUTPUT_DIR)\_Intermediates\Makefile.$(TARGET_OS).$(CONFIG).touched
!endif

!if "$(CONFIG)"=="Debug"
SCTool_CONFIG_FLAGS = /D_DEBUG /Zi
!else
SCTool_CONFIG_FLAGS = /DNDEBUG /O2
!endif

SCTool_COMMON_FLAGS = /I"../../.." /I"."
SCTool_CPPFLAGS = $(SCTool_COMMON_FLAGS) $(SCTool_CONFIG_FLAGS) $(CPPFLAGS)
SCTool_CXXFLAGS = $(SCTool_CPPFLAGS) /nologo /std:c++14 /permissive- /EHsc $(CXXFLAGS)

SCTool_LIBRARIES = Advapi32.lib Shell32.lib

SCTool_LDFLAGS = $(SCTool_LIBRARIES) $(LDFLAGS)

SCTool_CLEAN:
    @echo Cleaning $(TOOL)
    @if exist "$(SCTool_TARGET_DIR)\$(TOOL).*" del /q "$(SCTool_TARGET_DIR)\$(TOOL).*"
    @if exist "$(SCTool_INTERMEDIATE_DIR)\*.pdb" del /q "$(SCTool_INTERMEDIATE_DIR)\*.pdb"
    @if exist "$(SCTool_INTERMEDIATE_DIR)\*.obj" del /q "$(SCTool_INTERMEDIATE_DIR)\*.obj"
    @if exist "$(SCTool_INTERMEDIATE_DIR)\*.d" del /q "$(SCTool_INTERMEDIATE_DIR)\*.d"

SCTool_OBJECT_FILES = $(SCTool_INTERMEDIATE_DIR)\Tools.obj $(SCTool_INTERMEDIATE_DIR)\$(TOOL).obj
SCTool_DEPENDENCY_FILES=$(SCTool_OBJECT_FILES:.obj=.d)

# Rules for compiling source files

$(SCTool_INTERMEDIATE_DIR)\Tools.obj: $(SCTool_INTERMEDIATE_DIR) $(MAKEDIR)\..\..\..\Tools\Tools.cpp
    @cl.exe $(SCTool_CXXFLAGS) /c /sourceDependencies $(SCTool_INTERMEDIATE_DIR)\Tools.json /Fd"$(SCTool_INTERMEDIATE_DIR)\$(TOOL).pdb" /Fo$@ /MD $(MAKEDIR)\..\..\..\Tools\Tools.cpp

$(SCTool_INTERMEDIATE_DIR)\Tools.d: $(SCTool_INTERMEDIATE_DIR)\Tools.obj
    @echo Generating dependencies for Tools.cpp
    @powershell.exe -ExecutionPolicy Bypass -File jsonToDependencies.ps1 -jsonFilePath $(SCTool_INTERMEDIATE_DIR)\Tools.json -objFile $(SCTool_INTERMEDIATE_DIR)\Tools.obj > $(SCTool_INTERMEDIATE_DIR)\Tools.d

!if exist($(SCTool_INTERMEDIATE_DIR)\Tools.d)
!include $(SCTool_INTERMEDIATE_DIR)\Tools.d
!endif

$(SCTool_INTERMEDIATE_DIR)\$(TOOL).obj: $(SCTool_INTERMEDIATE_DIR) $(TOOL_SOURCE_DIR)\$(TOOL).cpp
    @cl.exe $(SCTool_CXXFLAGS) /c /sourceDependencies $(SCTool_INTERMEDIATE_DIR)\$(TOOL).json /Fd"$(SCTool_INTERMEDIATE_DIR)\$(TOOL).pdb" /Fo$@ /MD $(TOOL_SOURCE_DIR)\$(TOOL).cpp

$(SCTool_INTERMEDIATE_DIR)\$(TOOL).d: $(SCTool_INTERMEDIATE_DIR)\$(TOOL).obj
    @echo Generating dependencies for $(TOOL).cpp
    @powershell.exe -ExecutionPolicy Bypass -File jsonToDependencies.ps1 -jsonFilePath $(SCTool_INTERMEDIATE_DIR)\$(TOOL).json -objFile $(SCTool_INTERMEDIATE_DIR)\$(TOOL).obj > $(SCTool_INTERMEDIATE_DIR)\$(TOOL).d

!if exist($(SCTool_INTERMEDIATE_DIR)\$(TOOL).d)
!include $(SCTool_INTERMEDIATE_DIR)\$(TOOL).d
!endif

$(SCTool_TARGET_DIR) $(SCTool_INTERMEDIATE_DIR):
    @echo Creating "$@"
    @if not exist "$@" mkdir "$@"

SCTool_BUILD: $(SCTool_TARGET_DIR)\$(SCTool_TARGET_NAME)

$(SCTool_TARGET_DIR)\$(SCTool_TARGET_NAME): $(SCTool_TARGET_DIR) $(SCTool_OBJECT_FILES) $(SCTool_DEPENDENCY_FILES)
    @echo Linking "$(SCTool_TARGET_NAME)"
	@link /nologo /OUT:"$(SCTool_TARGET_DIR)\$(SCTool_TARGET_NAME)" $(SCTool_OBJECT_FILES) $(SCTool_LDFLAGS)
