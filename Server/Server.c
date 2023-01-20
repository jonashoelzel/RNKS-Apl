#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include "../Package.h"
#include "../checksum.h"
#include "../ackn.h"

// Needed for the Windows 2000 IPv6 Tech Preview.
#if (_WIN32_WINNT == 0x0500)
#include <tpipv6.h>
#endif

// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define BUFFER_SIZE              65536
#define DEFAULT_STRING_LENGTH    256

LPSTR PrintError(int ErrorCode)
{
    static char Message[1024];

    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
        FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL, ErrorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)Message, 1024, NULL);
    return Message;
}

void usage() {
    printf("usage: [-f filename] [-p port]");
}

void writeToFile(char* FileName, char* str) {
    // Open or create File
    FILE* file;
    if (fopen_s(&file, FileName, "a+") != 0) {
        printf("Error opening file\n");
        exit(-1);
    }

    fwrite(str, sizeof(char), strlen(str), file);

    fclose(file);
}

void parseArgs(int argc, char** argv, char* FileName, char* Port) {
    if (argc < 1) {
        printf("No arguments given");
        usage();
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
                    else usage();
                }
                else usage();
            case 'p':
                if (argv[i + 1]) {
                    if (argv[i + 1][0] != '-') {
                        strcpy_s(Port, DEFAULT_STRING_LENGTH, argv[++i]);
                        break;
                    }
                    else usage();
                }
                else usage();
            default:
                printf("Invalid parameter");
                usage();
                break;
            }
        }
    }
}

int main(int argc, char** argv)
{
    char Buffer[BUFFER_SIZE], Hostname[NI_MAXHOST];
    char* Address = NULL;
    char Port[DEFAULT_STRING_LENGTH];
    char FileName[DEFAULT_STRING_LENGTH];
    int RetVal, FromLen, AmountRead;
    SOCKADDR_STORAGE From;
    WSADATA wsaData;
    ADDRINFO Hints, * AddrInfo;
    SOCKET ServSock;
    fd_set SockSet;

    
	parseArgs(argc, argv, FileName, Port);
	if (strlen(FileName) == 0 || strlen(Port) == 0) {
        printf("File Name or Port not specified!");
        return -1;
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

    // Get AddrInfo
    RetVal = getaddrinfo(Address, Port, &Hints, &AddrInfo);
    if (RetVal != 0) {
        fprintf(stderr, "getaddrinfo failed with error %d: %s\n",
            RetVal, gai_strerror(RetVal));
        WSACleanup();
        return -1;
    }

    // Open a socket for this address
    ServSock = socket(AddrInfo->ai_family, AddrInfo->ai_socktype, AddrInfo->ai_protocol);
    if (ServSock == INVALID_SOCKET) {
        fprintf(stderr, "socket() failed with error %d: %s\n",
            WSAGetLastError(), PrintError(WSAGetLastError()));
        return -1;
    }

    // Check if Address is a link-local-address
    if (IN6_IS_ADDR_LINKLOCAL((IN6_ADDR*)INETADDR_ADDRESS(AddrInfo->ai_addr)) &&
        (((SOCKADDR_IN6*)(AddrInfo->ai_addr))->sin6_scope_id == 0)
        ) {
        fprintf(stderr,
            "IPv6 link local addresses should specify a scope ID!\n");
    }
    
    // Bind to Socket
    if (bind(ServSock, AddrInfo->ai_addr, (int)AddrInfo->ai_addrlen) == SOCKET_ERROR) {
        fprintf(stderr, "bind() failed with error %d: %s\n",
            WSAGetLastError(), PrintError(WSAGetLastError()));
        closesocket(ServSock);
        return -1;
    }
    freeaddrinfo(AddrInfo);

    
    printf("'Listening' on port %s\n", Port);

	// Loop forever and serve requests.
    FD_ZERO(&SockSet);
    while (1) {
        
        if (!FD_ISSET(ServSock, &SockSet)) {
            FD_SET(ServSock, &SockSet);
            
            if (select(1, &SockSet, 0, 0, 0) == SOCKET_ERROR) {
                fprintf(stderr, "select() failed with error %d: %s\n",
                    WSAGetLastError(), PrintError(WSAGetLastError()));
                WSACleanup();
                return -1;
            }
        }
        
        if (FD_ISSET(ServSock, &SockSet)) {
            FD_CLR(ServSock, &SockSet);
        }
        
        FromLen = sizeof(From);
        AmountRead = recvfrom(ServSock, Buffer, sizeof(Buffer), 0, (LPSOCKADDR)&From, &FromLen);
        if (AmountRead == SOCKET_ERROR) {
            fprintf(stderr, "recvfrom() failed with error %d: %s\n",
                WSAGetLastError(), PrintError(WSAGetLastError()));
            closesocket(ServSock);
            break;
        }
        if (AmountRead == 0) {
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

		Package* package = (Package*)Buffer;
        
		writeToFile(FileName, package->column);

        unsigned short calcedCS = checksum(&package->column, sizeof(package->column));
        
		if (calcedCS != package->checkSum) {
			printf("Checksum error for Package: %d, expected %d, got %d\n", package->seqNr, package->checkSum, calcedCS);
		}
		else {
			printf("Checksum OK for Package: %d with Data: [%.*s]\n", package->seqNr, sizeof(package->column), package->column);

            Ackn ackn;
			ackn.seqNr = package->seqNr;
            
            RetVal = sendto(ServSock, &ackn, sizeof(ackn), 0, (LPSOCKADDR)&From, FromLen);
            if (RetVal == SOCKET_ERROR) {
                fprintf(stderr, "send ackn failed with error %d: %s\n",
                    WSAGetLastError(), PrintError(WSAGetLastError()));
            }
		}
        
    }
    return 0;
}
