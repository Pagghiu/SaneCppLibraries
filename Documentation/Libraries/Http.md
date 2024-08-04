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
In current state the library is not even able to host a simple static website.  

# Description
The HTTP parser is an incremental parser, that will emit events as soon as a valid element has been successfully parsed.
This allows handling incomplete responses without needing holding it entirely in memory.

The HTTP client and server are for now just some toy implementations missing almost everything needed for real usage.  
They only contain what's used in the test so far, so really can't be defined as more than a Draft.
## HttpServer
@copydoc SC::HttpServer

## HttpClient
@copydoc SC::HttpClient

# Examples

No examples are provided so far as the API is very likely to change drastically going towards MVP.  
If you like to see where we are just a take a look at the associated unit test.

# Roadmap

🟨 MVP
- Server+Client: Support mostly used HTTP verbs / methods
- Server: Ability to host a local website (handle content encoding)

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
