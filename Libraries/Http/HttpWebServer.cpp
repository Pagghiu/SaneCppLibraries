// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpWebServer.h"
#include "../FileSystem/FileSystem.h"
#include "../Time/Time.h"
#include "Internal/HttpStringIterator.h"
#include <stdio.h>

struct SC::HttpWebServer::Internal
{
    static Result writeGMTHeaderTime(StringSpan headerName, HttpResponse& response,
                                     const Time::Absolute::ParseResult& local);
    static Result readFile(StringSpan initialDirectory, HttpRequest& request, HttpResponse& response);

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
    if (not Internal::readFile(directory.view(), request, response))
    {
        SC_TRUST_RESULT(response.startResponse(404));
        SC_TRUST_RESULT(response.end("Error"));
    }
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
        SC_TRY(response.addHeader("Connection", "Closed"));
        SC_TRY(response.addHeader("Content-Type", Internal::getContentType(extension)));
        SC_TRY(response.addHeader("Server", "SC"));
        Time::Absolute::ParseResult local;
        SC_TRY(Time::Realtime::now().parseUTC(local));
        SC_TRY(Internal::writeGMTHeaderTime("Date", response, local));
        SC_TRY(Time::Realtime(fileStat.modifiedTime).parseUTC(local));
        SC_TRY(Internal::writeGMTHeaderTime("Last-Modified", response, local));
        SC_TRY(response.end(data.toSpanConst()));
    }
    return Result(true);
}

SC::Result SC::HttpWebServer::Internal::writeGMTHeaderTime(StringSpan headerName, HttpResponse& response,
                                                           const Time::Absolute::ParseResult& local)
{
    char   bufferData[128];
    size_t len = static_cast<size_t>(::snprintf(bufferData, sizeof(bufferData), "%s, %02u %s %04u %02u:%02u:%02u GMT",
                                                local.getDay(), local.dayOfMonth, local.getMonth(), local.year,
                                                local.hour, local.minutes, local.seconds));
    if (len == 0 or len >= sizeof(bufferData))
    {
        return Result::Error("Failed to format time");
    }

    SC_TRY(response.addHeader(headerName, {{bufferData, len}, true, StringEncoding::Ascii}));
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
    return "text/html";
}
