// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#ifdef SC_FOUNDATION_PLATFORM_INSTRUCTION_SET_DEFINITION_H
#if SC_FOUNDATION_PLATFORM_INSTRUCTION_SET_DEFINITION_H != 1
#error "PlatformInstructionSet.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_PLATFORM_INSTRUCTION_SET_DEFINITION_H 1 // Increment to indicate a new version of the file

namespace SC
{

/// @brief Indicates the current host instruction set
enum class InstructionSet
{
    ARM64,
    Intel64,
    Intel32
};

#if defined(_M_ARM64) || defined(__aarch64__)
static constexpr InstructionSet HostInstructionSet = InstructionSet::ARM64;
#elif defined(_WIN64) || (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8)
static constexpr InstructionSet HostInstructionSet = InstructionSet::Intel64;
#else
static constexpr InstructionSet HostInstructionSet = InstructionSet::Intel32;
#endif
} // namespace SC

#endif // SC_FOUNDATION_PLATFORM_INSTRUCTION_SET_DEFINITION_H
