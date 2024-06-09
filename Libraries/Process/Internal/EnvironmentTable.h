// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "StringsArena.h"
namespace SC
{
template <int MAX_NUM_ENVIRONMENT>
struct EnvironmentTable;
}

template <int MAX_NUM_ENVIRONMENT>
struct SC::EnvironmentTable
{
    using environment_ptr = const native_char_t* const*;
    const native_char_t* childEnvs[MAX_NUM_ENVIRONMENT];

    Result writeTo(environment_ptr& environmentArray, bool inheritEnvironment, const StringsArena& table,
                   const ProcessEnvironment& parentEnv)
    {
        if (table.bufferString.view().isEmpty())
        {
            if (!inheritEnvironment)
            {
                // If we've been told not to inherit environment, let's pass an empty environment pointers array
                childEnvs[0]     = nullptr;
                environmentArray = childEnvs;
            }
        }
        else
        {
            // We have some custom user environment variables to add, and we push them to the childEnvironmentArray
            // array. If we also have to inherit paren environment variables, we will need to skip the ones that have
            // been redefined by the user.
            const char* envsView = table.bufferString.view().bytesIncludingTerminator();
            for (size_t idx = 0; idx < table.numberOfStrings; ++idx)
            {
                // Make argv point at the beginning of the idx-th arg
                childEnvs[idx] = reinterpret_cast<const native_char_t*>(envsView + table.stringsStart[idx]);
            }

            size_t childEnvCount = 0;
            if (inheritEnvironment)
            {
                // Parse names of the custom / redefined environment variables
                StringView names[MAX_NUM_ENVIRONMENT];
                (void)table.writeTo({names, MAX_NUM_ENVIRONMENT});
                for (size_t idx = 0; idx < table.numberOfStrings; ++idx)
                {
                    StringViewTokenizer tokenizer(names[idx]);
                    (void)tokenizer.tokenizeNext({'='});
                    names[idx] = tokenizer.component;
                }

                for (size_t parentIdx = 0; parentIdx < parentEnv.size(); ++parentIdx)
                {
                    bool       envHasBeenRedefined = false;
                    StringView name, value;
                    (void)parentEnv.get(parentIdx, name, value);
                    for (size_t idx = 0; idx < table.numberOfStrings; ++idx)
                    {
                        if (names[idx] == name)
                        {
                            envHasBeenRedefined = true;
                            break;
                        }
                    }
                    if (not envHasBeenRedefined)
                    {
                        if (childEnvCount + table.numberOfStrings >= MAX_NUM_ENVIRONMENT)
                        {
                            return Result(false);
                        }
                        childEnvs[childEnvCount + table.numberOfStrings] =
                            reinterpret_cast<const native_char_t*>(name.bytesWithoutTerminator());
                        childEnvCount++;
                    }
                }
            }
            // The last item also must be a nullptr to signal "end of array"
            childEnvs[childEnvCount + table.numberOfStrings] = nullptr;

            environmentArray = childEnvs;
        }
        return Result(true);
    }
};
