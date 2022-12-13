// Client side implementation of UDP client-server model
#include <stdio.h>
#include <stdlib.h>
//#include <unistd.h>
#include <io.h>
#include <string.h>
//#include <sys/types.h>
#include <WinSock2.h>
//#include <sys/socket.h>
//#include <arpa/inet.h>
//#include <netinet/in.h>

// https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-sendto

#define PORT	 8080
#define MAXLINE 1024

// Driver code
int main()
{
	printf("Client...\n");

	int result;
	WSADATA wsaData;
	SOCKET clientSocket;
	char buffer[MAXLINE];
	char* hello = "Hello from client";
	struct sockaddr_in	 servaddr;

	// Initialize Winsock
	result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != NO_ERROR) {
		wprintf(L"WSAStartup failed with error: %d\n", result);
		return 1;
	}

	// Creating socket file descriptor
	clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (clientSocket == INVALID_SOCKET)
	{
		wprintf(L"socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		exit(EXIT_FAILURE);
	}

	memset(&servaddr, 0, sizeof(servaddr));

	// Filling server information
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(PORT);
	servaddr.sin_addr.s_addr = INADDR_ANY;
	//servaddr.sin_addr.s_addr = inet_addr("");

	int n, len;

	result = sendto(clientSocket, (const char*)hello, strlen(hello), 0, (const struct sockaddr*)&servaddr, sizeof(servaddr));
	printf("Hello message sent.\n");

	n = recvfrom(clientSocket, (char*)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr*)&servaddr, &len);
	if (n < 0 || n > sizeof(buffer))
	{
		printf("Error while receiving: %s\n", n);
		exit(EXIT_FAILURE);
	}
	buffer[n] = '\0';
	printf("Server : %s\n", buffer);

	_close(clientSocket);
	
	return 0;
}
