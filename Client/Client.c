// client.c - Simple TCP/UDP client using Winsock 2.2
// 
//      This is a part of the Microsoft<entity type="reg"/> Source Code Samples.
//      Copyright 1996 - 2000 Microsoft Corporation.
//      All rights reserved.
//      This source code is only intended as a supplement to
//      Microsoft Development Tools and/or WinHelp<entity type="reg"/> documentation.
//      See these sources for detailed information regarding the
//      Microsoft samples programs.
//
#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
//#include <string.h>


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

#define DEFAULT_SERVER     NULL // Will use the loopback interface
#define DEFAULT_FAMILY     PF_UNSPEC    // Accept either IPv4 or IPv6
#define DEFAULT_SOCKTYPE   SOCK_STREAM  // TCP
#define DEFAULT_PORT       "5001"       // Arbitrary, albiet a historical test port
#define DEFAULT_EXTRA      0    // Number of "extra" bytes to send

#define BUFFER_SIZE        65536

#define UNKNOWN_NAME "<unknown>"

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

int ReceiveAndPrint(SOCKET ConnSocket, char* Buffer, int BufLen)
{
    int AmountRead;

    AmountRead = recv(ConnSocket, Buffer, BufLen, 0);
    if (AmountRead == SOCKET_ERROR) {
        fprintf(stderr, "recv() failed with error %d: %s\n",
            WSAGetLastError(), PrintError(WSAGetLastError()));
        closesocket(ConnSocket);
        WSACleanup();
        exit(1);
    }
    //
    // We are not likely to see this with UDP, since there is no
    // 'connection' established. 
    //
    if (AmountRead == 0) {
        printf("Server closed connection\n");
        closesocket(ConnSocket);
        WSACleanup();
        exit(0);
    }

    printf("Received %d bytes from server: [%.*s]\n",
        AmountRead, AmountRead, Buffer);

    return AmountRead;
}

SOCKET createAndConnectClient(char *ServerAddress, char *Port) {
    char AddrName[NI_MAXHOST];
    int RetVal;
    WSADATA wsaData;
    ADDRINFO Hints, *AddrInfo;
    SOCKET ConnSocket;

    // Ask for Winsock version 2.2.
    if ((RetVal = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
        fprintf(stderr, "WSAStartup failed with error %d: %s\n",
            RetVal, PrintError(RetVal));
        WSACleanup();
        return -1;
    }
    //
    // By not setting the AI_PASSIVE flag in the hints to getaddrinfo, we're
    // indicating that we intend to use the resulting address(es) to connect
    // to a service.  This means that when the ServerAddress parameter is NULL,
    // getaddrinfo will return one entry per allowed protocol family
    // containing the loopback address for that family.
    //

    memset(&Hints, 0, sizeof(Hints));
    Hints.ai_family = PF_INET6; // IPv6
    Hints.ai_socktype = SOCK_DGRAM; // UDP
    RetVal = getaddrinfo(ServerAddress, Port, &Hints, &AddrInfo);
    if (RetVal != 0) {
        fprintf(stderr,
            "Cannot resolve address [%s] and port [%s], error %d: %s\n",
            ServerAddress, Port, RetVal, gai_strerror(RetVal));
        WSACleanup();
        return -1;
    }

    if (AddrInfo == NULL) {
        fprintf(stderr, "Error: unable to connect to the server.\n");
        WSACleanup();
        return -1;
    }

    // Open a socket with the correct address family for this address.
    ConnSocket = socket(AddrInfo->ai_family, AddrInfo->ai_socktype, AddrInfo->ai_protocol);

    if (ConnSocket == INVALID_SOCKET) {
        fprintf(stderr, "Error Opening socket, error %d: %s\n",
            WSAGetLastError(), PrintError(WSAGetLastError()));
        return -1;
    }

    printf("Attempting to connect to: %s\n", ServerAddress);
    if (connect(ConnSocket, AddrInfo->ai_addr, (int)AddrInfo->ai_addrlen) == SOCKET_ERROR) {

        RetVal = WSAGetLastError();
        if (getnameinfo(AddrInfo->ai_addr, (int)AddrInfo->ai_addrlen, AddrName,
            sizeof(AddrName), NULL, 0, NI_NUMERICHOST) != 0)
            strcpy_s(AddrName, sizeof(AddrName), UNKNOWN_NAME);
        fprintf(stderr, "connect() to %s failed with error %d: %s\n",
            AddrName, RetVal, PrintError(RetVal));
        closesocket(ConnSocket);
        return -1;
    }

    freeaddrinfo(AddrInfo);
    return ConnSocket;
}

int socketConnected(SOCKET socket) {
    char AddrName[NI_MAXHOST];
    struct sockaddr_storage Addr;
    int addrLen = sizeof(Addr);

    if (getpeername(socket, (LPSOCKADDR)&Addr, (int*)&addrLen) == SOCKET_ERROR) {
        fprintf(stderr, "getpeername() failed with error %d: %s\n",
            WSAGetLastError(), PrintError(WSAGetLastError()));

        return 0;
    }
    else {
        if (getnameinfo((LPSOCKADDR)&Addr, addrLen, AddrName, sizeof(AddrName), NULL, 0, NI_NUMERICHOST) != 0)
            strcpy_s(AddrName, sizeof(AddrName), UNKNOWN_NAME);
        printf("Connected to %s, port %d\n", AddrName, ntohs(SS_PORT(&Addr)));

        return 1;
    }
}

int main(int argc, char** argv)
{
	Sleep(1000);

    char Buffer[BUFFER_SIZE], AddrName[NI_MAXHOST];

    char* ServerAddress = DEFAULT_SERVER;
    char* Port = DEFAULT_PORT;
    char* Dateiname;
    
    int AddrLen;
    struct sockaddr_storage Addr;
    SOCKET ConnSocket = INVALID_SOCKET;

    int RetVal, AmountToSend;
    int ExtraBytes = DEFAULT_EXTRA;
    unsigned int Iteration, MaxIterations = 1;
    BOOL RunForever = FALSE;


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
            case 'a':
                if (argv[i + 1]) {
                    if (argv[i + 1][0] != '-') {
                        ServerAddress = argv[++i];
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
    

    ConnSocket = createAndConnectClient(ServerAddress, Port);
    if (ConnSocket == INVALID_SOCKET)
        return -1; // Socket not created 

    if (!socketConnected(ConnSocket)) {
        return -2; // Socket not connected
    }

    //
    // Send and receive in a loop for the requested number of iterations.
    //
    for (Iteration = 0; RunForever || Iteration < MaxIterations; Iteration++) {

        // Compose a message to send.
        AmountToSend = sprintf_s(Buffer, sizeof(Buffer), "Message #%u", Iteration + 1);
        for (int i = 0; i < ExtraBytes; i++) {
            Buffer[AmountToSend++] = (char)((i & 0x3f) + 0x20);
        }

        // Send the message.  Since we are using a blocking socket, this
        // call shouldn't return until it's able to send the entire amount.
        RetVal = send(ConnSocket, Buffer, AmountToSend, 0);
        if (RetVal == SOCKET_ERROR) {
            fprintf(stderr, "send() failed with error %d: %s\n",
                WSAGetLastError(), PrintError(WSAGetLastError()));
            WSACleanup();
            return -1;
        }

        printf("Sent %d bytes (out of %d bytes) of data: [%.*s]\n",
            RetVal, AmountToSend, AmountToSend, Buffer);

        // Clear buffer just to prove we're really receiving something.
        memset(Buffer, 0, sizeof(Buffer));

        // Receive and print server's reply.
        ReceiveAndPrint(ConnSocket, Buffer, sizeof(Buffer));
    }

    // Tell system we're done sending.
    printf("Done sending\n");
    shutdown(ConnSocket, SD_SEND);

    closesocket(ConnSocket);
    WSACleanup();
    return 0;
}
