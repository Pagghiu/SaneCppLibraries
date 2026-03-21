// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Strings/CommandLine.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/StringFormat.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct CommandLineTest;
}

namespace
{
using namespace SC;

struct CommandLineFixture
{
    bool       verbose   = false;
    bool       sendfile  = true;
    bool       useEpoll  = false;
    int32_t    clients   = 0;
    uint16_t   port      = 0;
    StringView directory = {};
    StringSpan command   = {};

    StringSpan       filesStorage[4];
    Span<StringSpan> files = {};

    CommandLineOption     options[6];
    CommandLinePositional positionals[2];
    CommandLineSpec       spec;

    CommandLineFixture()
    {
        options[0].longName  = "verbose";
        options[0].help      = "Enable verbose logs";
        options[0].shortName = 'v';
        options[0].value     = CommandLineValue::boolean(verbose);

        options[1].longName         = "sendfile";
        options[1].negativeLongName = "no-sendfile";
        options[1].help             = "Enable or disable file send optimization";
        options[1].value            = CommandLineValue::boolean(sendfile);

        options[2].longName         = "epoll";
        options[2].negativeLongName = "uring";
        options[2].help             = "Select Linux backend";
        options[2].value            = CommandLineValue::boolean(useEpoll);

        options[3].longName  = "clients";
        options[3].help      = "Maximum number of clients";
        options[3].valueName = "NUM";
        options[3].shortName = 'c';
        options[3].value     = CommandLineValue::int32(clients);

        options[4].longName  = "port";
        options[4].help      = "Port to listen on";
        options[4].valueName = "PORT";
        options[4].shortName = 'p';
        options[4].required  = true;
        options[4].value     = CommandLineValue::uint16(port);

        options[5].longName  = "directory";
        options[5].help      = "Directory to serve";
        options[5].valueName = "PATH";
        options[5].shortName = 'd';
        options[5].value     = CommandLineValue::stringView(directory);

        positionals[0].name     = "command";
        positionals[0].help     = "Command to execute";
        positionals[0].required = true;
        positionals[0].value    = CommandLineValue::stringSpan(command);

        positionals[1].name             = "files";
        positionals[1].help             = "Additional file names";
        positionals[1].required         = false;
        positionals[1].remaining        = true;
        positionals[1].remainingStorage = filesStorage;
        positionals[1].parsedValues     = &files;

        spec.programName = "sample";
        spec.summary     = "Sample command line parser.";
        spec.options     = options;
        spec.positionals = positionals;
    }
};

static bool writeHelpToString(const SC::CommandLineSpec& spec, SC::String& text)
{
    SC::GrowableBuffer<SC::String> growable(text);
    SC::StringFormatOutput         output(SC::StringEncoding::Utf8, growable);
    const bool                     result = spec.writeHelp(output);
    growable.finalize();
    return result;
}

static bool writeErrorToString(const SC::CommandLineSpec& spec, const SC::CommandLineParseResult& parseResult,
                               SC::String& text)
{
    SC::GrowableBuffer<SC::String> growable(text);
    SC::StringFormatOutput         output(SC::StringEncoding::Utf8, growable);
    const bool                     result = spec.writeError(parseResult, output);
    growable.finalize();
    return result;
}
} // namespace

struct SC::CommandLineTest : public SC::TestCase
{
    CommandLineTest(SC::TestReport& report) : TestCase(report, "CommandLineTest")
    {
        using namespace SC;

        if (test_section("main argument adapter"))
        {
            const char*          argv[] = {"sample", "--port", "8080"};
            StringSpan           storage[2];
            CommandLineArguments arguments;
            SC_TEST_EXPECT(arguments.setFromMainArguments(3, argv, storage));
            SC_TEST_EXPECT(arguments.values.sizeInElements() == 2);
            SC_TEST_EXPECT(arguments.values[0] == "--port");
            SC_TEST_EXPECT(arguments.values[1] == "8080");
        }

        if (test_section("main argument adapter insufficient storage"))
        {
            const char*          argv[] = {"sample", "--port", "8080"};
            StringSpan           storage[1];
            CommandLineArguments arguments;
            SC_TEST_EXPECT(not arguments.setFromMainArguments(3, argv, storage));
        }

        if (test_section("parse long short inline and positionals"))
        {
            CommandLineFixture fixture;
            const StringSpan   args[] = {"run", "--directory=htdocs", "-p", "8080", "-c", "64", "index.html"};
            const auto         result = fixture.spec.parse(args);
            SC_TEST_EXPECT(result.status == CommandLineParseResult::Status::Success);
            SC_TEST_EXPECT(fixture.command == "run");
            SC_TEST_EXPECT(fixture.directory == "htdocs");
            SC_TEST_EXPECT(fixture.port == 8080);
            SC_TEST_EXPECT(fixture.clients == 64);
            SC_TEST_EXPECT(fixture.files.sizeInElements() == 1);
            SC_TEST_EXPECT(fixture.files[0] == "index.html");
        }

        if (test_section("grouped short boolean flags"))
        {
            bool a = false;
            bool b = false;
            bool c = false;

            CommandLineOption options[3];
            options[0].longName  = "alpha";
            options[0].shortName = 'a';
            options[0].value     = CommandLineValue::boolean(a);
            options[1].longName  = "beta";
            options[1].shortName = 'b';
            options[1].value     = CommandLineValue::boolean(b);
            options[2].longName  = "charlie";
            options[2].shortName = 'c';
            options[2].value     = CommandLineValue::boolean(c);

            CommandLineSpec spec;
            spec.programName = "group";
            spec.options     = options;

            const StringSpan args[] = {"-abc"};
            const auto       result = spec.parse(args);
            SC_TEST_EXPECT(result.status == CommandLineParseResult::Status::Success);
            SC_TEST_EXPECT(a and b and c);
        }

        if (test_section("terminator and remaining positionals"))
        {
            CommandLineFixture fixture;
            const StringSpan   args[] = {"run", "-p", "8080", "--", "-literal", "file.txt"};
            const auto         result = fixture.spec.parse(args);
            SC_TEST_EXPECT(result.status == CommandLineParseResult::Status::Success);
            SC_TEST_EXPECT(fixture.command == "run");
            SC_TEST_EXPECT(fixture.files.sizeInElements() == 2);
            SC_TEST_EXPECT(fixture.files[0] == "-literal");
            SC_TEST_EXPECT(fixture.files[1] == "file.txt");
            SC_TEST_EXPECT(not fixture.verbose);
        }

        if (test_section("explicit boolean pairs last one wins"))
        {
            CommandLineFixture fixture;
            const StringSpan   args[] = {"run", "-p", "8080", "--no-sendfile", "--sendfile", "--uring", "--epoll"};
            const auto         result = fixture.spec.parse(args);
            SC_TEST_EXPECT(result.status == CommandLineParseResult::Status::Success);
            SC_TEST_EXPECT(fixture.sendfile);
            SC_TEST_EXPECT(fixture.useEpoll);
        }

        if (test_section("help requested"))
        {
            CommandLineFixture fixture;
            const StringSpan   args[] = {"--help"};
            const auto         result = fixture.spec.parse(args);
            SC_TEST_EXPECT(result.status == CommandLineParseResult::Status::HelpRequested);
        }

        if (test_section("unknown option error"))
        {
            CommandLineFixture fixture;
            const StringSpan   args[] = {"run", "-p", "8080", "--wat"};
            const auto         result = fixture.spec.parse(args);
            SC_TEST_EXPECT(result.status == CommandLineParseResult::Status::Error);
            SC_TEST_EXPECT(result.error == CommandLineParseResult::Error::UnknownOption);
            SC_TEST_EXPECT(result.argument == "--wat");
        }

        if (test_section("missing option value error"))
        {
            CommandLineFixture fixture;
            const StringSpan   args[] = {"run", "-p"};
            const auto         result = fixture.spec.parse(args);
            SC_TEST_EXPECT(result.status == CommandLineParseResult::Status::Error);
            SC_TEST_EXPECT(result.error == CommandLineParseResult::Error::MissingOptionValue);
            SC_TEST_EXPECT(result.argument == "-p");
        }

        if (test_section("invalid option value error"))
        {
            CommandLineFixture fixture;
            const StringSpan   args[] = {"run", "--port=abc"};
            const auto         result = fixture.spec.parse(args);
            SC_TEST_EXPECT(result.status == CommandLineParseResult::Status::Error);
            SC_TEST_EXPECT(result.error == CommandLineParseResult::Error::InvalidOptionValue);
            SC_TEST_EXPECT(result.name == "port");
            SC_TEST_EXPECT(result.value == "abc");
        }

        if (test_section("missing required option error"))
        {
            CommandLineFixture fixture;
            const StringSpan   args[] = {"run"};
            const auto         result = fixture.spec.parse(args);
            SC_TEST_EXPECT(result.status == CommandLineParseResult::Status::Error);
            SC_TEST_EXPECT(result.error == CommandLineParseResult::Error::MissingOptionValue);
            SC_TEST_EXPECT(result.name == "port");
        }

        if (test_section("too many positionals error"))
        {
            StringSpan            command;
            CommandLinePositional positionals[1];
            positionals[0].name     = "command";
            positionals[0].required = true;
            positionals[0].value    = CommandLineValue::stringSpan(command);

            CommandLineSpec spec;
            spec.programName = "single";
            spec.positionals = positionals;

            const StringSpan args[] = {"run", "extra"};
            const auto       result = spec.parse(args);
            SC_TEST_EXPECT(result.status == CommandLineParseResult::Status::Error);
            SC_TEST_EXPECT(result.error == CommandLineParseResult::Error::TooManyPositionals);
            SC_TEST_EXPECT(result.argument == "extra");
        }

        if (test_section("remaining positional storage overflow"))
        {
            StringSpan            command;
            StringSpan            filesStorage[1];
            Span<StringSpan>      files;
            CommandLineOption     options[1];
            CommandLinePositional positionals[2];

            options[0].longName  = "port";
            options[0].shortName = 'p';
            uint16_t port        = 0;
            options[0].value     = CommandLineValue::uint16(port);

            positionals[0].name     = "command";
            positionals[0].required = true;
            positionals[0].value    = CommandLineValue::stringSpan(command);

            positionals[1].name             = "files";
            positionals[1].required         = false;
            positionals[1].remaining        = true;
            positionals[1].remainingStorage = filesStorage;
            positionals[1].parsedValues     = &files;

            CommandLineSpec spec;
            spec.programName = "overflow";
            spec.options     = options;
            spec.positionals = positionals;

            const StringSpan args[] = {"run", "-p", "8080", "a.txt", "b.txt"};
            const auto       result = spec.parse(args);
            SC_TEST_EXPECT(result.status == CommandLineParseResult::Status::Error);
            SC_TEST_EXPECT(result.error == CommandLineParseResult::Error::InsufficientPositionalStorage);
        }

        if (test_section("help formatting"))
        {
            CommandLineFixture fixture;
            String             text;
            SC_TEST_EXPECT(writeHelpToString(fixture.spec, text));
            SC_TEST_EXPECT(StringView(text.view()).containsString("Usage: sample"));
            SC_TEST_EXPECT(StringView(text.view()).containsString("--no-sendfile"));
            SC_TEST_EXPECT(StringView(text.view()).containsString("<command>"));
            SC_TEST_EXPECT(StringView(text.view()).containsString("-h, --help"));
        }

        if (test_section("error formatting"))
        {
            CommandLineFixture fixture;
            String             text;
            const StringSpan   args[] = {"run", "-p", "8080", "--wat"};
            const auto         result = fixture.spec.parse(args);
            SC_TEST_EXPECT(writeErrorToString(fixture.spec, result, text));
            SC_TEST_EXPECT(StringView(text.view()).containsString("Unknown option: --wat"));
            SC_TEST_EXPECT(StringView(text.view()).containsString("Use --help to show usage."));
        }

        if (test_section("integration async web server options"))
        {
            StringView directory;
            bool       sendfile = true;
            bool       useEpoll = false;
            int32_t    clients  = 400;
            int32_t    threads  = 4;
            uint16_t   port     = 8090;

            CommandLineOption options[6];
            options[0].longName  = "directory";
            options[0].valueName = "PATH";
            options[0].value     = CommandLineValue::stringView(directory);

            options[1].longName         = "sendfile";
            options[1].negativeLongName = "no-sendfile";
            options[1].value            = CommandLineValue::boolean(sendfile);

            options[2].longName         = "epoll";
            options[2].negativeLongName = "uring";
            options[2].value            = CommandLineValue::boolean(useEpoll);

            options[3].longName  = "clients";
            options[3].valueName = "NUM";
            options[3].value     = CommandLineValue::int32(clients);

            options[4].longName  = "threads";
            options[4].valueName = "NUM";
            options[4].value     = CommandLineValue::int32(threads);

            options[5].longName  = "port";
            options[5].valueName = "PORT";
            options[5].shortName = 'p';
            options[5].value     = CommandLineValue::uint16(port);

            CommandLineSpec spec;
            spec.programName = "AsyncWebServer";
            spec.options     = options;

            const StringSpan args[] = {"--directory", "htdocs", "--no-sendfile", "--uring",
                                       "--clients",   "200",    "--threads",     "6",
                                       "-p",          "9001"};
            const auto       result = spec.parse(args);
            SC_TEST_EXPECT(result.status == CommandLineParseResult::Status::Success);
            SC_TEST_EXPECT(directory == "htdocs");
            SC_TEST_EXPECT(not sendfile);
            SC_TEST_EXPECT(not useEpoll);
            SC_TEST_EXPECT(clients == 200);
            SC_TEST_EXPECT(threads == 6);
            SC_TEST_EXPECT(port == 9001);
        }

        if (test_section("utf16 borrowed values"))
        {
            StringView        directory;
            uint16_t          port = 0;
            CommandLineOption options[2];
            options[0].longName  = "directory";
            options[0].valueName = "PATH";
            options[0].value     = CommandLineValue::stringView(directory);
            options[1].longName  = "port";
            options[1].valueName = "PORT";
            options[1].value     = CommandLineValue::uint16(port);

            CommandLineSpec spec;
            spec.programName = "utf16";
            spec.options     = options;

#if SC_COMPILER_MSVC
            const StringSpan args[] = {L"--directory", L"htdocs", L"--port", L"8080"};
#else
            const auto       directoryOpt = "\x2d\x00\x2d\x00\x64\x00\x69\x00\x72\x00\x65\x00\x63\x00\x74\x00\x6f\x00"
                                            "\x72\x00\x79\x00\x00"_u16;
            const auto       directoryVal = "\x68\x00\x74\x00\x64\x00\x6f\x00\x63\x00\x73\x00\x00"_u16;
            const auto       portOpt      = "\x2d\x00\x2d\x00\x70\x00\x6f\x00\x72\x00\x74\x00\x00"_u16;
            const auto       portVal      = "\x38\x00\x30\x00\x38\x00\x30\x00\x00"_u16;
            const StringSpan args[]       = {directoryOpt, directoryVal, portOpt, portVal};
#endif
            const auto result = spec.parse(args);
            SC_TEST_EXPECT(result.status == CommandLineParseResult::Status::Success);
            SC_TEST_EXPECT(directory == "htdocs");
            SC_TEST_EXPECT(directory.getEncoding() == StringEncoding::Utf16);
            SC_TEST_EXPECT(port == 8080);
        }
    }
};

namespace SC
{
void runCommandLineTest(SC::TestReport& report) { CommandLineTest test(report); }
} // namespace SC
