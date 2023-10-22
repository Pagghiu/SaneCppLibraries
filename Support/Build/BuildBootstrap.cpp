// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.

#if not defined(SC_LIBRARY_PATH)
#include "../../Libraries/FileSystem/Path.h"
#include "../../Libraries/Foundation/Containers/SmallVector.h"
#include "../../Libraries/Foundation/Strings/SmallString.h"
#include "../../Libraries/System/Console.h"
#include "../../Libraries/System/Time.h"

int main(int argc, const char* argv[])
{
    using namespace SC;
    SC_COMPILER_WARNING_PUSH_UNUSED_RESULT; // Doing some optimistic coding here, ignoring all failures
    SC::AbsoluteTime       started = SC::AbsoluteTime::now();
    SmallVector<char, 512> consoleBuffer;
    Console                console(consoleBuffer);
    console.printLine("SCBuild configure start...");
    StringNative<256>     buffer;
    StringBuilder         builder(buffer);
    StringView            targetDirectory;
    StringView            sourcesDirectory;
    Array<StringView, 10> params;
    for (int idx = 1; idx < min(argc, 10); ++idx)
    {
        // TODO: Set proper encoding
        params.push_back(StringView::fromNullTerminated(argv[idx], StringEncoding::Ascii));
    }
    size_t argIndex;
    if (not params.contains("--target"_a8, &argIndex) or argIndex + 1 == params.size())
    {
        console.printLine("Build error missing --target\n");
        return -1;
    }
    targetDirectory = params[argIndex + 1];
    if (not params.contains("--sources"_a8, &argIndex) or argIndex + 1 == params.size())
    {
        console.printLine("Build error missing --sources\n");
        return -1;
    }
    sourcesDirectory = params[argIndex + 1];
    builder.format("targetDirectory   = {}", targetDirectory);
    console.printLine(buffer.view());
    builder.format("sourcesDirectory  = {}", sourcesDirectory);
    console.printLine(buffer.view());
    if (not Path::isAbsolute(targetDirectory, SC::Path::AsNative) or
        not Path::isAbsolute(sourcesDirectory, SC::Path::AsNative))
    {
        console.printLine("Both --target and --sources must be absolute paths");
        return -1;
    }
    Result res(true);
    res = SCBuild::generate(Build::Generator::VisualStudio2022, targetDirectory, sourcesDirectory);
    if (not res)
    {
        console.printLine("Build error Visual Studio 2022\n");
        return -1;
    }
    res = SCBuild::generate(Build::Generator::XCode14, targetDirectory, sourcesDirectory);
    if (not res)
    {
        console.printLine("Build error XCode\n");
        return -1;
    }
    RelativeTime elapsed = SC::AbsoluteTime::now().subtract(started);
    builder.format("Build finished (configure took {} ms)", elapsed.inRoundedUpperMilliseconds().ms);
    console.printLine(buffer.view());
    SC_COMPILER_WARNING_POP;
    return 0;
}
#endif