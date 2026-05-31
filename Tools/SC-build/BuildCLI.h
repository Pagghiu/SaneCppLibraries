// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Libraries/Memory/String.h"
#include "../../Libraries/Strings/CommandLine.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/Path.h"
#include "../../Libraries/Strings/StringBuilder.h"
#include "../../Libraries/Strings/StringFormat.h"
#include "../SC-package.h"
#include "../Tools.h"
#include "Build.h"

namespace SC
{
namespace Tools
{
namespace detail
{
enum class BuildCLIStatus : uint8_t
{
    Ready,
    HelpRequested,
    Error,
};

struct BuildCLIResolvedStorage
{
    String configurationName = StringEncoding::Ascii;
};

struct BuildCLIParseContext
{
    StringSpan target           = {};
    StringSpan targetProfile    = {};
    StringSpan toolchain        = {};
    StringSpan configuration    = {};
    StringSpan generator        = {};
    StringSpan architecture     = {};
    StringSpan abi              = {};
    StringSpan targetTriple     = {};
    StringSpan sysroot          = {};
    StringSpan windowsLongPath  = {};
    StringSpan runner           = {};
    StringSpan runnerPath       = {};
    StringSpan output           = {};
    bool       quietRequested   = false;
    bool       normalRequested  = false;
    bool       verboseRequested = false;

    StringSpan       legacyStorage[4];
    Span<StringSpan> legacyValues = {};
};

[[nodiscard]] Result appendBuildActionHelpAddendum(StringFormatOutput& output, Build::Action::Type actionType);

[[nodiscard]] Result prepareBuildAction(Build::Action::Type actionType, Tool::Arguments& arguments,
                                        Build::Action& action, BuildCLIResolvedStorage& resolvedStorage,
                                        BuildCLIStatus& status);
} // namespace detail
} // namespace Tools
} // namespace SC
