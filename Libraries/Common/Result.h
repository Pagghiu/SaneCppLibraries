// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#ifdef SC_FOUNDATION_RESULT_DEFINITION_H
#if SC_FOUNDATION_RESULT_DEFINITION_H != 2
#error "Result.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_RESULT_DEFINITION_H 2 // Increment to indicate a new version of the file

#include "CompilerMacrosExport.h"     // SC_FOUNDATION_EXPORT
#include "CompilerMacrosStdVersion.h" // SC_LANGUAGE_LIKELY
#include "PrimitiveDefinitions.h"     // uint32_t, uint64_t
#include "TypeTraits.h"               // RemoveConst, RemoveReference
namespace SC
{
struct SC_FOUNDATION_EXPORT Result;
//! @addtogroup group_foundation_utility
//! @{

/// @brief Open numeric namespace identifying the library or subsystem that owns an error.
struct ResultCategory
{
    uint32_t value;

    explicit constexpr ResultCategory(uint32_t value) : value(value) {}

    constexpr bool operator==(ResultCategory other) const { return value == other.value; }
    constexpr bool operator!=(ResultCategory other) const { return value != other.value; }
};

/// @brief Success/failure value. #SC_TRY forwards errors to the caller.
/// @details During the structured-error migration this type temporarily carries both the legacy stable ASCII message
/// pointer and a numeric category/error identity. The pointer will be removed after all libraries have been ported.
struct [[nodiscard]] Result
{
    /// @brief Legacy stable ASCII error message. Temporarily public for source compatibility during migration.
    const char* message;

    /// @brief Temporary structured identity storage. Use the accessors below instead of reading it directly.
    /// @details This field is public only to keep the migration bridge standard-layout while `message` remains public.
    uint64_t structuredErrorIdentity;

    static constexpr uint32_t UncategorizedValue    = 0;
    static constexpr uint32_t UnspecifiedErrorValue = 1;

    /// @brief Build a Result object from a boolean.
    /// @param result Passing `true` constructs a valid Result. Passing `false` constructs invalid Result.
    explicit constexpr Result(bool result)
        : message(result ? nullptr : "Unspecified Error"),
          structuredErrorIdentity(result ? 0 : encode(ResultCategory(UncategorizedValue), UnspecifiedErrorValue))
    {}

    /// @brief Constructs an Error from a pointer to an ASCII string literal
    /// @tparam numChars Size of the character array holding the ASCII string
    /// @param msg The custom error message
    /// @return A Result object in invalid state
    template <int numChars>
    static constexpr Result Error(const char (&msg)[numChars])
    {
        return Result(msg, 0);
    }

    /// @brief Constructs a structured error without embedding or retrieving an error message.
    /// @details Error value zero is invalid and is normalized to the uncategorized unspecified error.
    static constexpr Result Error(ResultCategory category, uint32_t errorValue)
    {
        return errorValue == 0 ? Result(false) : Result(nullptr, encode(category, errorValue));
    }

    /// @brief Constructs a structured error from a library-owned enum.
    template <typename ErrorEnum>
    static constexpr Result Error(ResultCategory category, ErrorEnum error)
    {
        return Error(category, static_cast<uint32_t>(error));
    }

    static constexpr Result Explicit(bool result) { return Result(result); }

    template <typename T>
    static constexpr typename TypeTraits::RemoveConst<typename TypeTraits::RemoveReference<T>::type>::type Explicit(
        T&& result)
    {
        using Value = typename TypeTraits::RemoveConst<typename TypeTraits::RemoveReference<T>::type>::type;
        return Value(forward<T>(result));
    }

    /// @brief Constructs an Error from a pointer to an ascii string.
    ///        Caller of this function must ensure such pointer to be valid until Result is used.
    /// @param msg  Pointer to ASCII string representing the message.
    /// @return A Result object in invalid state
    static constexpr Result FromStableCharPointer(const char* msg) { return Result(msg, 0); }

    /// @brief Returns true when this result carries a numeric category/error identity.
    constexpr bool hasStructuredError() const { return structuredErrorIdentity != 0; }

    /// @brief Returns true for an unported error carrying only a legacy message pointer.
    constexpr bool hasLegacyError() const { return message != nullptr and not hasStructuredError(); }

    /// @brief Returns true when a temporary legacy-compatible message is available.
    constexpr bool hasMessage() const { return message != nullptr; }

    /// @brief Returns the category of a structured error, or category zero otherwise.
    constexpr ResultCategory category() const
    {
        return ResultCategory(static_cast<uint32_t>(structuredErrorIdentity >> 32));
    }

    /// @brief Returns the error value of a structured error, or zero otherwise.
    constexpr uint32_t errorValue() const { return static_cast<uint32_t>(structuredErrorIdentity); }

    /// @brief Checks a structured error identity. Zero is never considered an error value.
    constexpr bool isError(ResultCategory wantedCategory, uint32_t wantedErrorValue) const
    {
        return wantedErrorValue != 0 and structuredErrorIdentity == encode(wantedCategory, wantedErrorValue);
    }

    /// @brief Checks a structured error identity using a library-owned enum.
    template <typename ErrorEnum>
    constexpr bool isError(ResultCategory wantedCategory, ErrorEnum wantedError) const
    {
        return isError(wantedCategory, static_cast<uint32_t>(wantedError));
    }

    /// @brief Returns a plain Result for generic propagation.
    constexpr Result toResult() const { return *this; }

    /// @brief Converts to `true` if the Result is valid, to `false` if it's invalid
    constexpr operator bool() const { return message == nullptr and structuredErrorIdentity == 0; }

  private:
    static constexpr uint64_t encode(ResultCategory category, uint32_t errorValue)
    {
        return (static_cast<uint64_t>(category.value) << 32) | static_cast<uint64_t>(errorValue);
    }

    explicit constexpr Result(const char* message, uint64_t structuredErrorIdentity)
        : message(message), structuredErrorIdentity(structuredErrorIdentity)
    {}
};

//! @}
} // namespace SC

//! @addtogroup group_foundation_utility
//! @{

/// @brief Checks the value of the given expression and if failed, returns this value to caller
#define SC_TRY(expression)                                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        if (auto _exprResConv = SC::Result::Explicit(expression))                                                      \
            SC_LANGUAGE_LIKELY { (void)0; }                                                                            \
        else                                                                                                           \
        {                                                                                                              \
            return _exprResConv;                                                                                       \
        }                                                                                                              \
    } while (false);

/// @brief Checks the value of the given expression and if failed, returns a result with failedMessage to caller
#define SC_TRY_MSG(expression, failedMessage)                                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
        if (not(expression))                                                                                           \
            SC_LANGUAGE_UNLIKELY { return SC::Result::Error(failedMessage); }                                          \
    } while (false);
//! @}

#endif // SC_FOUNDATION_RESULT_DEFINITION_H
