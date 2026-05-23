// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "PackageManager.h"

namespace SC
{
namespace Tools
{
PackageRegistry builtinPackageRegistry();
Result          addBuiltinPackages(PackageRegistryBuilder& registry);
} // namespace Tools
} // namespace SC
