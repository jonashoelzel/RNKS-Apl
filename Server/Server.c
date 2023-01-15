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

void Usage(char* ProgName)
{
    fprintf(stderr, "\nSimple socket sample server program.\n");
    fprintf(stderr,
        "\n%s [-f family] [-t transport] [-p port] [-a address]\n\n",
        ProgName);
    fprintf(stderr,
        "  family\tOne of PF_INET, PF_INET6 or PF_UNSPEC.  (default %s)\n",
        (DEFAULT_FAMILY ==
            PF_UNSPEC) ? "PF_UNSPEC" : ((DEFAULT_FAMILY ==
                PF_INET) ? "PF_INET" : "PF_INET6"));
    fprintf(stderr, "  transport\tEither TCP or UDP.  (default: %s)\n",
        (DEFAULT_SOCKTYPE == SOCK_STREAM) ? "TCP" : "UDP");
    fprintf(stderr, "  port\t\tPort on which to bind.  (default %s)\n",
        DEFAULT_PORT);
    fprintf(stderr,
        "  address\tIP address on which to bind.  (default: unspecified address)\n");
    WSACleanup();
    exit(1);
}

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
    int Family = DEFAULT_FAMILY;
    int SocketType = DEFAULT_SOCKTYPE;
    char* Port = DEFAULT_PORT;
    char* Address = NULL;
    int i, NumSocks, RetVal, FromLen, AmountRead;
    //    int idx;
    SOCKADDR_STORAGE From;
    WSADATA wsaData;
    ADDRINFO Hints, * AddrInfo, * AI;
    SOCKET ServSock[FD_SETSIZE];
    fd_set SockSet;

    // Parse arguments
    if (argc > 1) {
        for (i = 1; i < argc; i++) {
            if ((argv[i][0] == '-') || (argv[i][0] == '/') &&
                (argv[i][1] != 0) && (argv[i][2] == 0)) {
                switch (tolower(argv[i][1])) {
                case 'f':
                    if (!argv[i + 1])
                        Usage(argv[0]);
                    if (!STRICMP(argv[i + 1], "PF_INET"))
                        Family = PF_INET;
                    else if (!STRICMP(argv[i + 1], "PF_INET6"))
                        Family = PF_INET6;
                    else if (!STRICMP(argv[i + 1], "PF_UNSPEC"))
                        Family = PF_UNSPEC;
                    else
                        Usage(argv[0]);
                    i++;
                    break;

                case 't':
                    if (!argv[i + 1])
                        Usage(argv[0]);
                    if (!STRICMP(argv[i + 1], "TCP"))
                        SocketType = SOCK_STREAM;
                    else if (!STRICMP(argv[i + 1], "UDP"))
                        SocketType = SOCK_DGRAM;
                    else
                        Usage(argv[0]);
                    i++;
                    break;

                case 'a':
                    if (argv[i + 1]) {
                        if (argv[i + 1][0] != '-') {
                            Address = argv[++i];
                            break;
                        }
                    }
                    Usage(argv[0]);
                    break;

                case 'p':
                    if (argv[i + 1]) {
                        if (argv[i + 1][0] != '-') {
                            Port = argv[++i];
                            break;
                        }
                    }
                    Usage(argv[0]);
                    break;

                default:
                    Usage(argv[0]);
                    break;
                }
            }
            else
                Usage(argv[0]);
        }
    }
    // Ask for Winsock version 2.2.
    if ((RetVal = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
        fprintf(stderr, "WSAStartup failed with error %d: %s\n",
            RetVal, PrintError(RetVal));
        WSACleanup();
        return -1;
    }

    if (Port == NULL) {
        Usage(argv[0]);
    }
    //
    // By setting the AI_PASSIVE flag in the hints to getaddrinfo, we're
    // indicating that we intend to use the resulting address(es) to bind
    // to a socket(s) for accepting incoming connections.  This means that
    // when the Address parameter is NULL, getaddrinfo will return one
    // entry per allowed protocol family containing the unspecified address
    // for that family.
    //
    memset(&Hints, 0, sizeof(Hints));
    Hints.ai_family = Family;
    Hints.ai_socktype = SocketType;
    Hints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;
    RetVal = getaddrinfo(Address, Port, &Hints, &AddrInfo);
    if (RetVal != 0) {
        fprintf(stderr, "getaddrinfo failed with error %d: %s\n",
            RetVal, gai_strerror(RetVal));
        WSACleanup();
        return -1;
    }
    //
    // For each address getaddrinfo returned, we create a new socket,
    // bind that address to it, and create a queue to listen on.
    //
    for (i = 0, AI = AddrInfo; AI != NULL; AI = AI->ai_next) {

        // Highly unlikely, but check anyway.
        if (i == FD_SETSIZE) {
            printf("getaddrinfo returned more addresses than we could use.\n");
            break;
        }
        // This example only supports PF_INET and PF_INET6.
        if ((AI->ai_family != PF_INET) && (AI->ai_family != PF_INET6))
            continue;

        // Open a socket with the correct address family for this address.
        ServSock[i] = socket(AI->ai_family, AI->ai_socktype, AI->ai_protocol);
        if (ServSock[i] == INVALID_SOCKET) {
            fprintf(stderr, "socket() failed with error %d: %s\n",
                WSAGetLastError(), PrintError(WSAGetLastError()));
            continue;
        }

        if ((AI->ai_family == PF_INET6) &&
            IN6_IS_ADDR_LINKLOCAL((IN6_ADDR*)INETADDR_ADDRESS(AI->ai_addr)) &&
            (((SOCKADDR_IN6*)(AI->ai_addr))->sin6_scope_id == 0)
            ) {
            fprintf(stderr,
                "IPv6 link local addresses should specify a scope ID!\n");
        }
        //
        // bind() associates a local address and port combination
        // with the socket just created. This is most useful when
        // the application is a server that has a well-known port
        // that clients know about in advance.
        //
        if (bind(ServSock[i], AI->ai_addr, (int)AI->ai_addrlen) == SOCKET_ERROR) {
            fprintf(stderr, "bind() failed with error %d: %s\n",
                WSAGetLastError(), PrintError(WSAGetLastError()));
            closesocket(ServSock[i]);
            continue;
        }
        //
        // So far, everything we did was applicable to TCP as well as UDP.
        // However, there are certain fundamental differences between stream
        // protocols such as TCP and datagram protocols such as UDP.
        //
        // Only connection orientated sockets, for example those of type
        // SOCK_STREAM, can listen() for incoming connections.
        //
        if (SocketType == SOCK_STREAM) {
            if (listen(ServSock[i], 5) == SOCKET_ERROR) {
                fprintf(stderr, "listen() failed with error %d: %s\n",
                    WSAGetLastError(), PrintError(WSAGetLastError()));
                closesocket(ServSock[i]);
                continue;
            }
        }

        printf("'Listening' on port %s, protocol %s, protocol family %s\n",
            Port, (SocketType == SOCK_STREAM) ? "TCP" : "UDP",
            (AI->ai_family == PF_INET) ? "PF_INET" : "PF_INET6");
        i++;
    }

    freeaddrinfo(AddrInfo);

    if (i == 0) {
        fprintf(stderr, "Fatal error: unable to serve on any address.\n");
        WSACleanup();
        return -1;
    }
    NumSocks = i;

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
        for (i = 0; i < NumSocks; i++) {
            if (FD_ISSET(ServSock[i], &SockSet))
                break;
        }
        if (i == NumSocks) {
            for (i = 0; i < NumSocks; i++)
                FD_SET(ServSock[i], &SockSet);
            if (select(NumSocks, &SockSet, 0, 0, 0) == SOCKET_ERROR) {
                fprintf(stderr, "select() failed with error %d: %s\n",
                    WSAGetLastError(), PrintError(WSAGetLastError()));
                WSACleanup();
                return -1;
            }
        }
        for (i = 0; i < NumSocks; i++) {
            if (FD_ISSET(ServSock[i], &SockSet)) {
                FD_CLR(ServSock[i], &SockSet);
                break;
            }
        }

        if (SocketType == SOCK_STREAM) {
            SOCKET ConnSock;

            //
            // Since this socket was returned by the select(), we know we
            // have a connection waiting and that this accept() won't block.
            //
            ConnSock = accept(ServSock[i], (LPSOCKADDR)&From, &FromLen);
            if (ConnSock == INVALID_SOCKET) {
                fprintf(stderr, "accept() failed with error %d: %s\n",
                    WSAGetLastError(), PrintError(WSAGetLastError()));
                WSACleanup();
                return -1;
            }
            if (getnameinfo((LPSOCKADDR)&From, FromLen, Hostname,
                sizeof(Hostname), NULL, 0, NI_NUMERICHOST) != 0)
                strcpy_s(Hostname, NI_MAXHOST, "<unknown>");
            printf("\nAccepted connection from %s\n", Hostname);

            //
            // This sample server only handles connections sequentially.
            // To handle multiple connections simultaneously, a server
            // would likely want to launch another thread or process at this
            // point to handle each individual connection.  Alternatively,
            // it could keep a socket per connection and use select()
            // on the fd_set to determine which to read from next.
            //
            // Here we just loop until this connection terminates.
            //

            while (1) {

                //
                // We now read in data from the client.  Because TCP
                // does NOT maintain message boundaries, we may recv()
                // the client's data grouped differently than it was
                // sent.  Since all this server does is echo the data it
                // receives back to the client, we don't need to concern
                // ourselves about message boundaries.  But it does mean
                // that the message data we print for a particular recv()
                // below may contain more or less data than was contained
                // in a particular client send().
                //

                AmountRead = recv(ConnSock, Buffer, sizeof(Buffer), 0);
                if (AmountRead == SOCKET_ERROR) {
                    fprintf(stderr, "recv() failed with error %d: %s\n",
                        WSAGetLastError(), PrintError(WSAGetLastError()));
                    closesocket(ConnSock);
                    break;
                }
                if (AmountRead == 0) {
                    printf("Client closed connection\n");
                    closesocket(ConnSock);
                    break;
                }

                printf("Received %d bytes from client: [%.*s]\n",
                    AmountRead, AmountRead, Buffer);
                printf("Echoing same data back to client\n");

                RetVal = send(ConnSock, Buffer, AmountRead, 0);
                if (RetVal == SOCKET_ERROR) {
                    fprintf(stderr, "send() failed: error %d: %s\n",
                        WSAGetLastError(), PrintError(WSAGetLastError()));
                    closesocket(ConnSock);
                    break;
                }
            }

        }
        else {

            //
            // Since UDP maintains message boundaries, the amount of data
            // we get from a recvfrom() should match exactly the amount of
            // data the client sent in the corresponding sendto().
            //
            AmountRead = recvfrom(ServSock[i], Buffer, sizeof(Buffer), 0,
                (LPSOCKADDR)&From, &FromLen);
            if (AmountRead == SOCKET_ERROR) {
                fprintf(stderr, "recvfrom() failed with error %d: %s\n",
                    WSAGetLastError(), PrintError(WSAGetLastError()));
                closesocket(ServSock[i]);
                break;
            }
            if (AmountRead == 0) {
                // This should never happen on an unconnected socket, but...
                printf("recvfrom() returned zero, aborting\n");
                closesocket(ServSock[i]);
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

            RetVal = sendto(ServSock[i], Buffer, AmountRead, 0,
                (LPSOCKADDR)&From, FromLen);
            if (RetVal == SOCKET_ERROR) {
                fprintf(stderr, "send() failed with error %d: %s\n",
                    WSAGetLastError(), PrintError(WSAGetLastError()));
            }
        }
    }

    return 0;
}

/*
// https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-recvfrom

#ifndef UNICODE
#define UNICODE
#endif

#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <stdio.h>

// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

LPTSTR PrintError(int ErrorCode)
{
    static TCHAR Message[1024];

    // If this program was multithreaded, we'd want to use
    // FORMAT_MESSAGE_ALLOCATE_BUFFER instead of a static buffer here.
    // (And of course, free the buffer when we were done with it)

    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
        FORMAT_MESSAGE_MAX_WIDTH_MASK,
        NULL, ErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        Message, 1024, NULL);
    return Message;
}

int main()
{

    int iResult = 0;

    WSADATA wsaData;

    SOCKET RecvSocket = INVALID_SOCKET;
    // struct sockaddr_in6 RecvAddr;
    ADDRINFO *RecvAddr;
    ADDRINFO HintAddr;

    unsigned short Port = 50000;

    char RecvBuf[1024];
    int BufLen = 1024;

    struct sockaddr_in6 SenderAddr;
    char* Address = "::1";

    ADDRINFO AddrInfo;
    int SenderAddrSize = sizeof(SenderAddr);
    fd_set SockSet;
    
    //-----------------------------------------------
    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        wprintf(L"WSAStartup failed with error %d\n", iResult);
        return 1;
    }


    //-----------------------------------------------
    // Bind the socket to any address and the specified port.
    HintAddr.ai_family = PF_INET6;
    HintAddr.ai_socktype = SOCK_DGRAM;
    HintAddr.ai_flags = AI_NUMERICHOST | AI_PASSIVE;
    HintAddr.ai_addr = "::1";

    
    iResult = getaddrinfo(Address, Port, &HintAddr, &RecvAddr);
    if (iResult != 0) {
        fprintf(stderr, L"getaddrinfo failed with error %d: %s\n", iResult, gai_strerror(iResult));
        WSACleanup();
        return -1;
    }

    //-----------------------------------------------
    // Create a receiver socket to receive datagrams
    RecvSocket = socket(RecvAddr->ai_family, RecvAddr->ai_socktype, IPPROTO_UDP);
    if (RecvSocket == INVALID_SOCKET) {
        wprintf(L"socket failed with error %d\n", WSAGetLastError());
        return 1;
    }

    // iResult = bind(RecvSocket, (struct sockaddr*)&RecvAddr, sizeof(RecvAddr));
    iResult = bind(RecvSocket, RecvAddr->ai_addr, (int) RecvAddr->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        wprintf(L"bind failed with error %d\n", WSAGetLastError());
        return 1;
    }




    FD_ZERO(&SockSet);

    while (1) {
        if (FD_ISSET(RecvSocket, &SockSet))
            break;

        FD_SET(RecvSocket, &SockSet);

        if (select(1, &SockSet, 0, 0, 0) == SOCKET_ERROR) {
            fprintf(stderr, "select() failed with error %d: %s\n",
                WSAGetLastError(), PrintError(WSAGetLastError()));
            WSACleanup();
            return -1;
        }

        if (FD_ISSET(RecvSocket, &SockSet)) {
            FD_CLR(RecvSocket, &SockSet);
        }

        int AmountRead = recvfrom(RecvSocket, RecvBuf, sizeof(RecvBuf), 0, (LPSOCKADDR)&SenderAddr, &SenderAddrSize);
        if (AmountRead == SOCKET_ERROR) {
            fprintf(stderr, L"recvfrom() failed with error %d: %s\n",
                WSAGetLastError(), PrintError(WSAGetLastError()));
            closesocket(RecvSocket);
            break;
        }

        if (AmountRead == 0) {
            // This should never happen on an unconnected socket, but...
            printf("recvfrom() returned zero, aborting\n");
            closesocket(RecvSocket);
            break;
        }

        printf("Received a %d byte datagram: [%.*s]\n", AmountRead, AmountRead, RecvBuf);
    }



    return -10;



    ////-----------------------------------------------
    //// Call the recvfrom function to receive datagrams
    //// on the bound socket.
    //wprintf(L"Receiving datagrams...\n");
    //iResult = recvfrom(RecvSocket,
    //    RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SenderAddrSize);
    //if (iResult == SOCKET_ERROR) {
    //    wprintf(L"recvfrom failed with error %d\n", WSAGetLastError());
    //}
    //else {
    //    puts(RecvBuf);
    //}

    ////-----------------------------------------------
    //// Close the socket when finished receiving datagrams
    //wprintf(L"Finished receiving. Closing socket.\n");
    //iResult = closesocket(RecvSocket);
    //if (iResult == SOCKET_ERROR) {
    //    wprintf(L"closesocket failed with error %d\n", WSAGetLastError());
    //    return 1;
    //}

    ////-----------------------------------------------
    //// Clean up and exit.
    //wprintf(L"Exiting.\n");
    //WSACleanup();
    //return 0;
}
*/