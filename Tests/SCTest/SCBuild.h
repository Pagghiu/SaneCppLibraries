// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../../Libraries/Build/Build.h"

namespace SC
{
struct SCBuild;
} // namespace SC

struct SC::SCBuild
{
    [[nodiscard]] static Result configure(Build::Definition& definition, Build::Parameters& parameters,
                                          StringView rootDirectory);
    [[nodiscard]] static Result generate(Build::Generator::Type generator, StringView targetDirectory,
                                         StringView sourcesDirectory);
};
