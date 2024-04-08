@page page_coding_style Coding Style

[TOC]

> Note: this document will be updated regularly clarifying existing rules and adding missing guidelines that will emerge from discussions or PRs being reviewed.

@note
If you really like to contribute check also [CONTRIBUTING.md](https://github.com/Pagghiu/SaneCppLibraries/blob/main/CONTRIBUTING.md)!

# Formatting
All files should be formatted according to the `.clang-format` file using `clang-format` version 15.  


Github CI will fail on PR that are not properly formatted.

In some specific cases use `// clang-format off` and `// clang-format on` where a custom formatting can improve code look.
For example many template specializations in the `SC::Reflection` library are better manually formatted/aligned to highlight the pattern.

All headers must have a trailing newline.

@note You can easily format files using the `SC.sh format execute` (or `SC.bat format execute`) command

# Casing

- `CamelCase` must be used for:
    - structs
    - namespaces
    - Template parameters
    - file names (both .h and .cpp and .inl)
- `camelCase` must be used for:
    - variables
    - methods
- `SCREAMING_CASE` muse be used for
    - Preprocessor `#define`
    
# [[nodiscard]]

Function and methods returning a value must always be marked as `[[nodiscard]]`

# Error checking

When using system API try to handle at least the basic error codes in the very first draft implementation and leave `TODO:` to remember handling additional errors that can occur.

Always use `SC_TRY` family of macros to check report errors to the caller.
Some very specific use cases may be exempted from this rule when it's unreasonable and useless.
When the specific use case has been approved, the method that doesn't want to do any error checking should use the `SC_COMPILER_WARNING_*` macros.

Example:
```cpp
struct MyClass
{
    [[nodiscard]] bool canFail(){ ... }
    [[nodiscard]] SC::Result alsoThisCanFail(){ ... }
}

void myFunction(MyClass& stuff)
{
    SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
    stuff.canFail();
    stuff.alsoThisCanFail();
    SC_COMPILER_WARNING_POP;
}
```

For example `SC::Build` writers disable unused result warnings for ignoring the potential failure of appending strings in the `SC::StringBuilder` being used.

# Return values

If the return value of a function is already returning `bool` or `SC::Result`, consider adding an `out` parameter, that can be a `pointer` or a `reference.`

Example of mandatory out value:

```cpp
bool formatZeroValue(int parameter, String& mandatoryOutValue)
{
    if(parameter == 0)
    {
        return mandatoryOutValue.assign("zero");
    }
    return mandatoryOutValue.assign("non zero");
}

// ... usage
int someParameter = 123;
SmallString<20> myString;

SC_TRY(formatZeroValue(someParameter, myString));

SC_ASSERT_RELEASE(myString.view() == "non zero");
```

Example of optional out value:

```cpp
template<typename T>
struct Vector
{

    /// ...
    /// @brief Check if the current vector contains a given value.
    /// @tparam U Type of the object being searched
    /// @param value Value being searched
    /// @param foundIndex if passed in != `nullptr`, receives index where item was found.
    /// Only written if function returns `true`
    /// @return `true` if the vector contains the given value.
    template <typename U>
    [[nodiscard]] bool contains(const U& value, size_t* foundIndex = nullptr) const;

    /// ...
};

///. usage

Vector<int> myVector = {1,2,3};

if(myVector.contains(1))
{
    // do stuff with knowing this
}

size_t foundIndex;
if(myVector.contains(2, &foundIndex))
{
    // we know also at what index the item was found
    myVector[foundIndex]++;
}
```

# struct vs class
`struct` is preferred to `class` due to an arbitrary initial choice made during the very first phases of the project.

Using `struct` is now necessary for consistency with the existing code.

# Namespaces

All functions and structs must be defined in the namespace `SC`.  
Some libraries defining many related classes may group them in a nested namespace inside `SC`.

`using namespace` is only allowed inside a function or method scope.

# Platform specific code

Platform specific code should be isolated with proper `#ifdef` inside a specific function if it's a small amount of code.  
Bigger platform specific re-implementation should be separated in specific `.inl` files included by the library `.cpp` file.

# Header inclusion

Each header and compile unit should include what's necessary to use or compile it in isolation.
> The unity build is a distribution mechanism detail.

# Public Headers

Public headers are meant to be included by users of the library.
Such headers should have the smallest amount of code that is needed to use them.
Everywhere possible, write the implementation code in a .cpp file rather than an header.

> Public headers are not allowed to include any OS specific or System / compiler provided header.

# Forward declarations

Use forward declarations everywhere possible to reduce the number of header dependencies.

# Functions

Free functions should be defined as `static` inside a struct (or nested namespace) with very few specific exceptions.

# Member variables

Member variables should not start with lowercase `m` to indicate it's a member variable (so `someThing` is good but `mSomeThing` is not)

Good:
```cpp
int myVariable;
```

Bad 1:
```cpp
int mMyVariable;
```

Bad 2:
```cpp
int m_myVariable;
```

# Testing

All newly added code must have associated tests.

# Documentation

All public classes should be documented, possibly with an usage example.

# Comments

Comments should in general avoid repeating what's stated in the code.  
Please, take some time to find a good variable and method name that can avoid a comment.  
Also restructuring code into a function with a proper name can sometimes avoid a comment.

When a comment is necessary, it's better for it to comprehensively explain a piece of code that is doing something not immediately obvious.

`TODO:` (without specific attribution) are welcome to signal incomplete error handling, missing code paths and future development.

```cpp
SC_TRY(converter.appendNullTerminated(currentWorkingDirectory));
// TODO: Assert if path is not absolute
return Result(existsAndIsDirectory("."));
```

# Braces

Braces may be omitted only if a single statement guarded by the if and only if there are no else clauses.

Good:
```cpp
if (someCondition)
    myVariable += 1;
```
Good:
```cpp
if (someCondition)
{
    myVariable += 1;
}
```
Good:
```cpp
if (someCondition)
{
    myVariable += 1;
}
else
{
    myVariable = 0;
    otherStuff();
}
```


Bad:
```cpp
if (someCondition)
    myVariable += 1;
else
{
    myVariable = 0;
    otherStuff();
}
```

# Parameters

Pointer parameters always indicate that such parameter _can_ be null.  
Use references to indicate that a parameter is mandatory.

Example:
```cpp
struct Value
{
    int value;
};

int someFunction(int parameter, Value* optionalParameter)
{
    if (optionalParameter != nullptr)
    {
        // This is now safe to use
        return parameter + optionalParameter->value;
    }
    return parameter;
}

int someOtherFunction(int parameter, Value& mandatoryParameter)
{
    return parameter + mandatoryParameter.value;
}
```

# Casts

Try writing code that can avoid casts, by using correct integer types where possible.

> When cast is needed, do not use C-style casts but ONLY C++ style casts (`static_cast`, `reinterpret_cast`).

# Globals / Static member variables

Globals and static member variables should be avoided at all costs.  

> Special exemption allowing usage of globals will need to be discussed and approved on a case by case basis.

# Virtual 

Usage of `virtual` should be avoided if possible.  
Prefer static dispatch (using `enums`) and for runtime-only use cases consider using `SC::Function`

> Special exemption allowing `virtual` usage will need to be discussed and approved on a case by case basis.

# Exceptions / RTTI

Exceptions and RTTI are not allowed.
