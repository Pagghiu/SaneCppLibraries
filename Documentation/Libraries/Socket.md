@page library_socket Socket

@brief 🟨 Synchronous socket networking and DNS lookup

[TOC]

@copydetails group_socket

# Features
| Class                     | Description
|:--------------------------|:----------------------------------|
| SC::SocketServer          | @copybrief SC::SocketServer       |
| SC::SocketClient          | @copybrief SC::SocketClient       |
| SC::SocketIPAddress       | @copybrief SC::SocketIPAddress    |
| SC::SocketDNS             | @copybrief SC::SocketDNS          |
| SC::SocketNetworking      | @copybrief SC::SocketNetworking   |

# Status

🟨 MVP  
Simple synchronous TCP client / server workflow is supported, but it would need better testing.  

# Description
- SC::SocketServer can SC::SocketServer::listen on a given port / address and SC::SocketServer::accept a new client socket.
- SC::SocketClient can SC::SocketClient::connect, SC::SocketClient::write, SC::SocketClient::read and SC::SocketClient::readWithTimeout.
- SC::SocketIPAddress allows creating an ip address native object from a SC::StringView and port.
- SC::SocketDNS class allow resolving a StringView to an IP address.
- SC::SocketNetworking initializes WinSock2 for current process.

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
