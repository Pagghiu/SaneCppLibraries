// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpWebServer.h"
#include "../FileSystem/FileSystem.h"
#include "Internal/HttpStringIterator.h"
#include <stdio.h>
#include <time.h>
#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <sys/timeb.h>
#else
#include <math.h>
#endif

struct SC::HttpWebServer::Internal
{
    static Result writeGMTHeaderTime(StringSpan headerName, HttpResponse& response, int64_t millisecondsSinceEpoch);
    static Result readFile(StringSpan initialDirectory, HttpRequest& request, HttpResponse& response);
    static Result formatHttpDate(int64_t millisecondsSinceEpoch, char* buffer, size_t bufferSize, size_t& outLength);

    static int64_t    getCurrentTimeMilliseconds();
    static StringSpan getContentType(const StringSpan extension);
};

SC::Result SC::HttpWebServer::init(StringSpan directoryToServe)
{
    SC_TRY_MSG(FileSystem().existsAndIsDirectory(directoryToServe), "Invalid directory");
    SC_TRY(directory.assign(directoryToServe));
    return Result(true);
}

SC::Result SC::HttpWebServer::stopAsync() { return Result(true); }

void SC::HttpWebServer::serveFile(HttpRequest& request, HttpResponse& response)
{
    SC_ASSERT_RELEASE(Internal::readFile(directory.view(), request, response));
}

SC::Result SC::HttpWebServer::Internal::readFile(StringSpan directory, HttpRequest& request, HttpResponse& response)
{
    if (not HttpStringIterator::startsWith(request.getURL(), "/"))
    {
        return Result::Error("Wrong url");
    }
    StringSpan url = HttpStringIterator::sliceStart(request.getURL(), 1);
    if (url.isEmpty())
    {
        url = "index.html";
    }
    FileSystem fileSystem;
    SC_TRY(fileSystem.init(directory));
    if (fileSystem.existsAndIsFile(url))
    {
        FileSystem::FileStat fileStat;
        SC_TRY(fileSystem.getFileStat(url, fileStat));
        StringSpan name, extension;
        SC_TRY(HttpStringIterator::parseNameExtension(url, name, extension));
        Buffer data;
        SC_TRY(fileSystem.read(url, data));
        SC_TRY(response.startResponse(200));
        char buffer[20];
        snprintf(buffer, sizeof(buffer), "%zu", data.size());
        StringSpan contentLength = {{buffer, ::strlen(buffer)}, true, StringEncoding::Ascii};
        SC_TRY(response.addHeader("Content-Length", contentLength));
        SC_TRY(response.addHeader("Content-Type", Internal::getContentType(extension)));
        SC_TRY(Internal::writeGMTHeaderTime("Date", response, Internal::getCurrentTimeMilliseconds()));
        SC_TRY(Internal::writeGMTHeaderTime("Last-Modified", response, fileStat.modifiedTime.milliseconds));
        SC_TRY(response.addHeader("Server", "SC"));
        SC_TRY(response.addHeader("Connection", "Closed"));

        SC_TRY(response.sendHeaders());
        SC_TRY(response.getWritableStream().write(move(data)));
        SC_TRY(response.end());
    }
    else
    {
        SC_TRY(response.startResponse(404));
        SC_TRY(response.addHeader("Server", "SC"));
        SC_TRY(response.addHeader("Connection", "Closed"));
        SC_TRY(response.sendHeaders());
        SC_TRY(response.end());
    }
    return Result(true);
}

SC::StringSpan SC::HttpWebServer::Internal::getContentType(const StringSpan extension)
{
    if (extension == "htm" or extension == "html")
    {
        return "text/html";
    }
    if (extension == "css")
    {
        return "text/css";
    }
    if (extension == "png")
    {
        return "image/png";
    }
    if (extension == "jpeg" or extension == "jpg")
    {
        return "image/jpg";
    }
    if (extension == "svg")
    {
        return "image/svg+xml";
    }
    if (extension == "js")
    {
        return "application/javascript";
    }
    if (extension == "json")
    {
        return "application/json";
    }
    if (extension == "xml")
    {
        return "application/xml";
    }
    if (extension == "pdf")
    {
        return "application/pdf";
    }
    if (extension == "ico")
    {
        return "image/x-icon";
    }
    if (extension == "txt")
    {
        return "text/plain";
    }
    return "text/html";
}

SC::int64_t SC::HttpWebServer::Internal::getCurrentTimeMilliseconds()
{
#if SC_PLATFORM_WINDOWS
    struct _timeb t;
    _ftime_s(&t);
    return static_cast<int64_t>(t.time) * 1000 + t.millitm;
#else
    struct timespec nowTimeSpec;
    clock_gettime(CLOCK_REALTIME, &nowTimeSpec);
    return static_cast<int64_t>(round(nowTimeSpec.tv_nsec / 1.0e6) + nowTimeSpec.tv_sec * 1000);
#endif
}

SC::Result SC::HttpWebServer::Internal::formatHttpDate(int64_t millisecondsSinceEpoch, char* buffer, size_t bufferSize,
                                                       size_t& outLength)
{
    const time_t seconds = static_cast<time_t>(millisecondsSinceEpoch / 1000);
    struct tm    parsedTm;
#if SC_PLATFORM_WINDOWS
    if (_gmtime64_s(&parsedTm, &seconds) != 0)
    {
        return Result::Error("Failed to convert time");
    }
#else
    if (gmtime_r(&seconds, &parsedTm) == nullptr)
    {
        return Result::Error("Failed to convert time");
    }
#endif

    // Format as HTTP date: "Wed, 21 Oct 2015 07:28:00 GMT"
    const char* days[]   = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    outLength = static_cast<size_t>(
        snprintf(buffer, bufferSize, "%s, %02d %s %04d %02d:%02d:%02d GMT", days[parsedTm.tm_wday], parsedTm.tm_mday,
                 months[parsedTm.tm_mon], parsedTm.tm_year + 1900, parsedTm.tm_hour, parsedTm.tm_min, parsedTm.tm_sec));

    if (outLength == 0 || outLength >= bufferSize)
    {
        return Result::Error("Failed to format time");
    }

    return Result(true);
}

SC::Result SC::HttpWebServer::Internal::writeGMTHeaderTime(StringSpan headerName, HttpResponse& response,
                                                           int64_t millisecondsSinceEpoch)
{
    char   bufferData[128];
    size_t len;
    SC_TRY(formatHttpDate(millisecondsSinceEpoch, bufferData, sizeof(bufferData), len));

    SC_TRY(response.addHeader(headerName, {{bufferData, len}, true, StringEncoding::Ascii}));
    return Result(true);
}
