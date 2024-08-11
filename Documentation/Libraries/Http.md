@page library_http Http

@brief 🟥 HTTP parser, client and server

[TOC]

Http library contains a hand-written http 1.1 parser, client and server.

# Features
- HTTP 1.1 Parser
- HTTP 1.1 Client
- HTTP 1.1 Server

# Status
🟥 Draft  
In current state the library is able to host simple static website but it cannot be used for any internet facing application.  

# Description
The HTTP parser is an incremental parser, that will emit events as soon as a valid element has been successfully parsed.
This allows handling incomplete responses without needing holding it entirely in memory.

The HTTP client and server are for now just some basic implementations and are missing some important feature.  

## HttpServer
@copydoc SC::HttpServer

## HttpWebServer
@copydoc SC::HttpWebServer

## HttpClient
@copydoc SC::HttpClient

# Examples

- [SCExample](@ref page_examples) features the `WebServerExample` sample showing how to use SC::HttpWebServer and SC::HttpServer
- Unit tests show how to use SC::HttpWebServer, SC::HttpServer and SC::HttpClient

# Roadmap

🟨 MVP
- Server+Client: Support mostly used HTTP verbs / methods
- Server+Client: HTTP 1.1 Chunked Encoding

🟩 Usable Features:
- Implement ([Web Streams API](https://developer.mozilla.org/en-US/docs/Web/API/Streams_API))
- Connection Upgrade
- Multipart encoding

🟦 Complete Features:
- HTTPS
- Support all HTTP verbs / methods

💡 Unplanned Features:
- Http 2.0 
- Http 3.0
