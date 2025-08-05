// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Foundation/Assert.h"
#include "../../Process/Internal/StringsArena.h"
#include "../../Process/Process.h"

#include <string.h>
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
        if (table.view().isEmpty())
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
            SC_ASSERT_RELEASE(table.view().isNullTerminated());
            const char* envsView = table.view().bytesWithoutTerminator();
            for (size_t idx = 0; idx < table.numberOfStrings; ++idx)
            {
                // Make argv point at the beginning of the idx-th arg
                childEnvs[idx] = reinterpret_cast<const native_char_t*>(envsView + table.stringsStart[idx]);
            }

            size_t childEnvCount = 0;
            if (inheritEnvironment)
            {
                // Parse names of the custom / redefined environment variables
                StringSpan names[MAX_NUM_ENVIRONMENT];
                (void)table.writeTo({names, MAX_NUM_ENVIRONMENT});
                for (size_t idx = 0; idx < table.numberOfStrings; ++idx)
                {
                    const native_char_t* keyValue = names[idx].getNullTerminatedNative();
#if SC_PLATFORM_WINDOWS
                    const native_char_t* equalSign = ::wcschr(keyValue, '=');
#else
                    const native_char_t* equalSign = ::strchr(keyValue, '=');
#endif
                    SC_ASSERT_RELEASE(equalSign != nullptr);
                    names[idx] = StringSpan({keyValue, static_cast<size_t>(equalSign - keyValue)}, false,
                                            StringEncoding::Native);
                }

                for (size_t parentIdx = 0; parentIdx < parentEnv.size(); ++parentIdx)
                {
                    bool       envHasBeenRedefined = false;
                    StringSpan name, value;
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
                            return Result::Error("EnvironmentTable::writeTo - MAX_NUM_ENVIRONMENT exceeded");
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
