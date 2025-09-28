@page library_socket Socket

@brief 🟨 Synchronous socket networking and DNS lookup

[TOC]

[SaneCppSocket.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppSocket.h) is a library implementing synchronous socket networking and DNS lookup.

# Dependencies
- Dependencies: [Foundation](@ref library_foundation)
- All dependencies: [Foundation](@ref library_foundation)

![Dependency Graph](Socket.svg)

# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 133			| 190		| 323	|
| Sources   | 722			| 142		| 864	|
| Sum       | 855			| 332		| 1187	|

# Features
| Class                     | Description
|:--------------------------|:----------------------------------|
| SC::SocketDescriptor      | @copybrief SC::SocketDescriptor   |
| SC::SocketServer          | @copybrief SC::SocketServer       |
| SC::SocketClient          | @copybrief SC::SocketClient       |
| SC::SocketIPAddress       | @copybrief SC::SocketIPAddress    |
| SC::SocketDNS             | @copybrief SC::SocketDNS          |
| SC::SocketNetworking      | @copybrief SC::SocketNetworking   |

# Details

@copydetails group_socket

# Status

🟨 MVP  
Simple synchronous TCP client / server workflow is supported, but it would need better testing.  

# Blog

Some relevant blog posts are:

- [June 2025 Update](https://pagghiu.github.io/site/blog/2025-06-30-SaneCppLibrariesUpdate.html)
- [July 2025 Update](https://pagghiu.github.io/site/blog/2025-07-31-SaneCppLibrariesUpdate.html)

# Description
- SC::SocketDescriptor can create and destroy the OS level socket descriptor.
- SC::SocketServer can SC::SocketServer::listen on a given port / address and SC::SocketServer::accept a new client socket.
- SC::SocketClient can SC::SocketClient::connect, SC::SocketClient::write, SC::SocketClient::read and SC::SocketClient::readWithTimeout.
- SC::SocketIPAddress allows creating an ip address native object from a SC::StringView and port.
- SC::SocketDNS class allow resolving a StringView to an IP address.
- SC::SocketNetworking initializes WinSock2 for current process.

## SocketDescriptor

@copydetails SC::SocketDescriptor 

## SocketServer

@copydetails SC::SocketServer 

## SocketClient

@copydetails SC::SocketClient 

## SocketIPAddress

@copydetails SC::SocketIPAddress 

## SocketDNS

@copydetails SC::SocketDNS

# Roadmap

🟩 Usable
- Add UDP specific socket operations

🟦 Complete Features:
- To be decided

💡 Unplanned Features:
- None so far
