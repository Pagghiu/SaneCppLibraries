// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpWebServer.h"
#include "../FileSystem/FileSystem.h"
#include "../FileSystem/Path.h"
#include "../Strings/StringBuilder.h"

struct SC::HttpWebServer::Internal
{
    static Result     writeGMTHeaderTime(StringView headerName, HttpResponse& response,
                                         const Time::Absolute::ParseResult& local);
    static StringView getContentType(const StringView extension);

    static Result readFile(StringView initialDirectory, HttpRequest& request, HttpResponse& response);
};

SC::Result SC::HttpWebServer::init(StringView directoryToServe)
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

SC::Result SC::HttpWebServer::Internal::readFile(StringView directory, HttpRequest& request, HttpResponse& response)
{
    if (not request.getURL().startsWith("/"))
    {
        return Result::Error("Wrong url");
    }
    StringView url = request.getURL().sliceStart(1);
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
        StringView name, extension;
        SC_TRY(Path::parseNameExtension(url, name, extension));
        Vector<char> data;
        SC_TRY(fileSystem.read(url, data));
        SC_TRY(response.startResponse(200));
        SC_TRY(response.addHeader("Connection", "Closed"));
        SC_TRY(response.addHeader("Content-Type", Internal::getContentType(extension)));
        SC_TRY(response.addHeader("Server", "SC"));
        Time::Absolute::ParseResult local;
        SC_TRY(Time::Realtime::now().parseUTC(local));
        SC_TRY(Internal::writeGMTHeaderTime("Date", response, local));
        SC_TRY(fileStat.modifiedTime.parseUTC(local));
        SC_TRY(Internal::writeGMTHeaderTime("Last-Modified", response, local));
        SC_TRY(response.end(data.toSpanConst()));
    }
    return Result(true);
}

SC::Result SC::HttpWebServer::Internal::writeGMTHeaderTime(StringView headerName, HttpResponse& response,
                                                           const Time::Absolute::ParseResult& local)
{
    SmallString<512> buffer;
    SC_TRY(StringBuilder(buffer).format("{}, {:02} {} {} {:02}:{:02}:{:02} GMT", local.getDay(), local.dayOfMonth,
                                        local.getMonth(), local.year, local.hour, local.minutes, local.seconds));

    SC_TRY(response.addHeader(headerName, buffer.view()));
    return Result(true);
}

SC::StringView SC::HttpWebServer::Internal::getContentType(const StringView extension)
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
