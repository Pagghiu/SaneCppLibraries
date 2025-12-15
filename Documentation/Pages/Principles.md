@page page_principles Principles

[TOC]

@brief
✅ Fast compile times  
✅ Bloat free  
✅ Simple and readable code  
✅ Easy to integrate  
⛔️ No C++ Standard Library / Exceptions / RTTI  
⛔️ No third party build dependencies (prefer OS API)

# 📖 Readability
- Common:
    - Do not use obscure C++ constructs
    - Do not break code completion engines (interactive API discovery by typing `.` on an object)
    - Simple readable code is preferred for a first implementation (iterate to achieve Correctness and Optimization)
    - New code should be "looking similar" to existing code
- Naming:
    - Name things properly
    - Rename classes / methods / parameters until they match what they actually represent or do
    - Long classes, variables, methods, parameters names are fine
    - Renaming a class or method later after realizing there is a better name is fine
    - Avoid generic words in class names like `Manager`, `Context`, `Factory`, `Handler`, `Helper`, `Utility` etc. that do not explain anything
- Classes:
    - Avoid operator overloading (only exception can be math classes eventually)
    - Avoid automatic conversions to bool
    - Avoid creating constructors taking bool
- Pointers / References:
    - Pointer parameters are always **optional**
    - Pointer parameters must always be assumed to be possibly nullptr when accessing it
    - References should be used when parameter is not optional

# ✅ Correctness
- Make sure that memory ownership is clear and explicit
- Focus on all error conditions cases when writing error handling code
- Debug builds should run with sanitizers on by default (`ASAN`, `UBSAN` all the time and `TSAN` when needed)
- Compiler warnings are errors
- Compile with maximum warnings level
- Silence warnings only when it's not reasonable or possible avoiding them
- Code should always work correctly on recent `GCC`, `Clang` and `MSVC` compilers
- Code should always work on `x86`, `x86_64`, `ARM64` and `Wasm`
- No code should be added without a test
- Code coverage is expected to be >= 90%
- Minimize Undefined Behavior (within limits dictated by other principles)
- If a method can fail, place it in a "create / init / assign" method instead of the constructor

# 🚀 Speed
- Do not use non zero-cost abstractions (**exceptions** or **RTTI**)
- Do not allocate many tiny objects with their own lifetime (and probably unclear or shared ownership)
- Optimization includes fast Debug builds
- Optimization includes fast compile times
- Optimization should not compromise correctness and readability
- Do not use system headers (coming with the compiler or OS SDK) in public facing headers to keep compile times fast
- Do not put code in headers if it's possible putting it in a .cpp file
- Do not use templates for premature optimization, causing unnecessary code to be put in headers
- Prefer static dispatch to virtual dispatch where technically possible
- Prefer using `Span<T>` and `StringView` to `Vector<T>` and `String`
- Anything behind libraries API should not allocate on the heap
- API should give control over allocations to the caller where dynamic allocation can't be avoided
- Consider using pre-defined fixed buffers to avoid dynamic allocation callbacks
