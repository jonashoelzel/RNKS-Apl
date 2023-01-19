//
// server.c - Simple TCP/UDP server using Winsock 2.2
//
//      This is a part of the Microsoft<entity type="reg"/> Source Code Samples.
//      Copyright 1996 - 2000 Microsoft Corporation.
//      All rights reserved.
//      This source code is only intended as a supplement to
//      Microsoft Development Tools and/or WinHelp<entity type="reg"/> documentation.
//      See these sources for detailed information regarding the
//      Microsoft samples programs.
//

#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#include <stdlib.h>
#include <stdio.h>

// Needed for the Windows 2000 IPv6 Tech Preview.
#if (_WIN32_WINNT == 0x0500)
#include <tpipv6.h>
#endif

// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define STRICMP _stricmp

//
// This code assumes that at the transport level, the system only supports
// one stream protocol (TCP) and one datagram protocol (UDP).  Therefore,
// specifying a socket type of SOCK_STREAM is equivalent to specifying TCP
// and specifying a socket type of SOCK_DGRAM is equivalent to specifying UDP.
//

#define DEFAULT_FAMILY     PF_UNSPEC    // Accept either IPv4 or IPv6
#define DEFAULT_SOCKTYPE   SOCK_STREAM  // TCP
#define DEFAULT_PORT       "5001"       // Arbitrary, albiet a historical test port
#define BUFFER_SIZE        64   // Set very small for demonstration purposes

LPSTR PrintError(int ErrorCode)
{
    static char Message[1024];

    // If this program was multithreaded, we'd want to use
    // FORMAT_MESSAGE_ALLOCATE_BUFFER instead of a static buffer here.
    // (And of course, free the buffer when we were done with it)

    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
        FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL, ErrorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)Message, 1024, NULL);
    return Message;
}

int main(int argc, char** argv)
{
    char Buffer[BUFFER_SIZE], Hostname[NI_MAXHOST];
    int SocketType = DEFAULT_SOCKTYPE;
    char* Port = DEFAULT_PORT;
    char* Dateiname;
    char* Address = NULL;
    int NumSocks, RetVal, FromLen, AmountRead;
    //    int idx;
    SOCKADDR_STORAGE From;
    WSADATA wsaData;
    ADDRINFO Hints, * AddrInfo;
    SOCKET ServSock;
    fd_set SockSet;
    
    //Address = "::1";
    //Port = "50000";
    
    SocketType = SOCK_DGRAM;
    
    if (argc < 1) {
        printf("No arguments given");
        return -1;
    }
    for (int i = 1; i < argc; i++) {
        if ((argv[i][0] == '-') || (argv[i][0] == '/') &&
            (argv[i][1] != 0) && (argv[i][2] == 0)) {
            switch (tolower(argv[i][1]))
            {
            case 'f':
                if (argv[i + 1]) {
                    if (argv[i + 1][0] != '-') {
                        Dateiname = argv[++i];
                        break;
                    }
                }
            case 'p':
                if (argv[i + 1]) {
                    if (argv[i + 1][0] != '-') {
                        Port = argv[++i];
                        break;
                    }
                }
            default:
                printf("Invalid parameter");
                break;
            }
        }
    }



    // Ask for Winsock version 2.2.
    if ((RetVal = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
        fprintf(stderr, "WSAStartup failed with error %d: %s\n",
            RetVal, PrintError(RetVal));
        WSACleanup();
        return -1;
    }
    
    memset(&Hints, 0, sizeof(Hints));
    Hints.ai_family = PF_INET6; // IPv6
    Hints.ai_socktype = SOCK_DGRAM; // UDP
    Hints.ai_flags = AI_NUMERICHOST | AI_PASSIVE; // Address will be binded
    RetVal = getaddrinfo(Address, Port, &Hints, &AddrInfo);
    if (RetVal != 0) {
        fprintf(stderr, "getaddrinfo failed with error %d: %s\n",
            RetVal, gai_strerror(RetVal));
        WSACleanup();
        return -1;
    }

    // Open a socket with the correct address family for this address.
    ServSock = socket(AddrInfo->ai_family, AddrInfo->ai_socktype, AddrInfo->ai_protocol);
    if (ServSock == INVALID_SOCKET) {
        fprintf(stderr, "socket() failed with error %d: %s\n",
            WSAGetLastError(), PrintError(WSAGetLastError()));
        return -1;
    }

    if ((AddrInfo->ai_family == PF_INET6) &&
        IN6_IS_ADDR_LINKLOCAL((IN6_ADDR*)INETADDR_ADDRESS(AddrInfo->ai_addr)) &&
        (((SOCKADDR_IN6*)(AddrInfo->ai_addr))->sin6_scope_id == 0)
        ) {
        fprintf(stderr,
            "IPv6 link local addresses should specify a scope ID!\n");
    }
    
    if (bind(ServSock, AddrInfo->ai_addr, (int)AddrInfo->ai_addrlen) == SOCKET_ERROR) {
        fprintf(stderr, "bind() failed with error %d: %s\n",
            WSAGetLastError(), PrintError(WSAGetLastError()));
        closesocket(ServSock);
        return -1;
    }

    printf("'Listening' on port %s\n", Port);

    freeaddrinfo(AddrInfo);
    
    NumSocks = 1;

    //
    // We now put the server into an eternal loop,
    // serving requests as they arrive.
    //
    FD_ZERO(&SockSet);
    while (1) {

        FromLen = sizeof(From);

        //
        // For connection orientated protocols, we will handle the
        // packets received from a connection collectively.  For datagram
        // protocols, we have to handle each datagram individually.
        //

        //
        // Check to see if we have any sockets remaining to be served
        // from previous time through this loop.  If not, call select()
        // to wait for a connection request or a datagram to arrive.
        //
        if (!FD_ISSET(ServSock, &SockSet)) {
            FD_SET(ServSock, &SockSet);
            
            if (select(NumSocks, &SockSet, 0, 0, 0) == SOCKET_ERROR) {
                fprintf(stderr, "select() failed with error %d: %s\n",
                    WSAGetLastError(), PrintError(WSAGetLastError()));
                WSACleanup();
                return -1;
            }
        }
        
        if (FD_ISSET(ServSock, &SockSet)) {
            FD_CLR(ServSock, &SockSet);
        }
        
        
        //
        // Since UDP maintains message boundaries, the amount of data
        // we get from a recvfrom() should match exactly the amount of
        // data the client sent in the corresponding sendto().
        //
        AmountRead = recvfrom(ServSock, Buffer, sizeof(Buffer), 0,
            (LPSOCKADDR)&From, &FromLen);
        if (AmountRead == SOCKET_ERROR) {
            fprintf(stderr, "recvfrom() failed with error %d: %s\n",
                WSAGetLastError(), PrintError(WSAGetLastError()));
            closesocket(ServSock);
            break;
        }
        if (AmountRead == 0) {
            // This should never happen on an unconnected socket, but...
            printf("recvfrom() returned zero, aborting\n");
            closesocket(ServSock);
            break;
        }

        RetVal = getnameinfo((LPSOCKADDR)&From, FromLen, Hostname,
            sizeof(Hostname), NULL, 0, NI_NUMERICHOST);
        if (RetVal != 0) {
            fprintf(stderr, "getnameinfo() failed with error %d: %s\n",
                RetVal, PrintError(RetVal));
            strcpy_s(Hostname, NI_MAXHOST, "<unknown>");
        }

        printf("Received a %d byte datagram from %s: [%.*s]\n",
            AmountRead, Hostname, AmountRead, Buffer);
        printf("Echoing same data back to client\n");

        RetVal = sendto(ServSock, Buffer, AmountRead, 0,
            (LPSOCKADDR)&From, FromLen);
        if (RetVal == SOCKET_ERROR) {
            fprintf(stderr, "send() failed with error %d: %s\n",
                WSAGetLastError(), PrintError(WSAGetLastError()));
        }
        
    }

    return 0;
}
