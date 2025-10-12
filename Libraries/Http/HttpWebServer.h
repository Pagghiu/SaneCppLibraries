// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Memory/String.h"
#include "HttpServer.h"

namespace SC
{
struct SC_COMPILER_EXPORT HttpWebServer;
}

/// @brief Http web server helps statically serves files from a directory.
/// @n
/// It can be used in conjunction with SC::HttpServer, by calling SC::HttpWebServer::serveFile
/// inside the SC::HttpServer::onRequest callback to statically serve files.
///
/// @see SC::HttpServer
///
/// \snippet Tests/Libraries/Http/HttpWebServerTest.cpp HttpWebServerSnippet
struct SC::HttpWebServer
{
    /// @brief Initialize the web server on the given file system directory to serve
    Result init(StringSpan directoryToServe);

    /// @brief Release all resources allocated by this web server
    Result stopAsync();

    /// @brief Serve the file requested by this Http Client on its channel
    /// Call this method in response to HttpServer::onRequest to serve a file
    void serveFile(HttpRequest& request, HttpResponse& response);

  private:
    String directory;

    struct Internal;
};
