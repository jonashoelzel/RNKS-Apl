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
#include <string.h>


// Needed for the Windows 2000 IPv6 Tech Preview.
#if (_WIN32_WINNT == 0x0500)
#include <tpipv6.h>
#endif

#include "../Package.h"
#include "../checksum.h"
#include "../ackn.h"

// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define BUFFER_SIZE              65536
#define DEFAULT_STRING_LENGTH    256
#define FILE_LINE_BUFFER_SIZE    2048

LPTSTR PrintError(int ErrorCode)
{
    static TCHAR Message[1024];

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
            strcpy_s(AddrName, sizeof(AddrName), "<unknown>");
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
            strcpy_s(AddrName, sizeof(AddrName), "<unknown>");
        printf("Connected to %s, port %d\n", AddrName, ntohs(SS_PORT(&Addr)));

        return 1;
    }
}

void parseArgs(int argc, char* argv[], char* FileName, char* ServerAddress, char* Port) {
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
                        strcpy_s(FileName, DEFAULT_STRING_LENGTH, argv[++i]);
                        break;
                    }
                }
            case 'a':
                if (argv[i + 1]) {
                    if (argv[i + 1][0] != '-') {
                        strcpy_s(ServerAddress, DEFAULT_STRING_LENGTH, argv[++i]);
                        break;
                    }
                }
            case 'p':
                if (argv[i + 1]) {
                    if (argv[i + 1][0] != '-') {
                        strcpy_s(Port, DEFAULT_STRING_LENGTH, argv[++i]);
                        break;
                    }
                }
            default:
                printf("Invalid parameter");
                break;
            }
        }
    }
}



int main(int argc, char** argv)
{
	Sleep(1000); // Wait for server to start

    char Buffer[BUFFER_SIZE], AddrName[NI_MAXHOST];

    char ServerAddress[DEFAULT_STRING_LENGTH];
    char Port[DEFAULT_STRING_LENGTH];
    char FileName[DEFAULT_STRING_LENGTH];
    
    int AddrLen;
    struct sockaddr_storage Addr;
    SOCKET ConnSocket = INVALID_SOCKET;
    int RetVal, AmountToSend;


    parseArgs(argc, argv, FileName, ServerAddress, Port);
    if (strlen(FileName) == 0 || strlen(ServerAddress) == 0 || strlen(Port) == 0) {
        printf("File Name, Address or Port not specified!");
        return -1;
    }

    ConnSocket = createAndConnectClient(ServerAddress, Port);
    if (ConnSocket == INVALID_SOCKET)
        return -1; // Socket not created 

    if (!socketConnected(ConnSocket)) {
        return -2; // Socket not connected
    }

    
    // Open file
    FILE* file;
	char line[FILE_LINE_BUFFER_SIZE];

    errno_t err = fopen_s(&file, FileName, "r");
    if (err != 0) {
        printf("\n Unable to open : %s ", FileName);
        return -1;
    }
    
    // Loop over each line of the file and send it in packages to the server
    while (fgets(line, sizeof(line), file)) {
        int numbOfPackagesToSend = strlen(line) / (PACKAGE_BUFFER_SIZE - 1);
        for (int i = 0; i <= numbOfPackagesToSend; i++) {
            Package package;

            // Compose a Package to send.
            if (strncpy_s(package.column, _countof(package.column), &line[i * (PACKAGE_BUFFER_SIZE - 1)], PACKAGE_BUFFER_SIZE - 1) != 0) {
                printf("error copying string to package");
                return -1;
            }
            package.seqNr = i;
            package.checkSum = checksum(&package.column, sizeof(package.column));


            // Send the Package
            RetVal = send(ConnSocket, &package, sizeof(package), 0);
            if (RetVal == SOCKET_ERROR) {
                fprintf(stderr, "send() failed with error %d: %s\n",
                    WSAGetLastError(), PrintError(WSAGetLastError()));
                WSACleanup();
                return -1;
            }

            printf("Sent Package: %d of Size: %d with Data: [%.*s]\n",
                package.seqNr, RetVal, sizeof(package.column), package.column);

            // Clear buffer
            memset(Buffer, 0, sizeof(Buffer));

            // Wait for ACKN
            int AmountRead = recv(ConnSocket, Buffer, sizeof(Buffer), 0);
            if (AmountRead == SOCKET_ERROR) {
                fprintf(stderr, "recv() failed with error %d: %s\n",
                    WSAGetLastError(), PrintError(WSAGetLastError()));
                closesocket(ConnSocket);
                WSACleanup();
                return -1;
            }
            Ackn* ackn = (Ackn*)Buffer;

            printf("Package with SeqNr: %d got acknowledged\n", ackn->seqNr);
        }
    }
    printf("Done sending\n");
    shutdown(ConnSocket, SD_SEND);

    closesocket(ConnSocket);
    WSACleanup();
    fclose(file);
    return 0;
}
