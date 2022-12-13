// Server side implementation of UDP client-server model
#include <stdio.h>
#include <stdlib.h>
// #include <unistd.h>
#include <io.h>
#include <string.h>
//#include <sys/types.h>
#include <winsock2.h>

// #include <sys/socket.h>
// #include <arpa/inet.h>
// #include <netinet/in.h>

// https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-recvfrom

#define PORT	 8080
#define MAXLINE 1024

// Driver code
int main() 
{
	printf("Server...\n");

	int result;
	WSADATA wsaData;
	SOCKET serverSocket;
	char buffer[MAXLINE];
	char* hello = "Hello from server";
	struct sockaddr_in servaddr, cliaddr;

	// Initialize Winsock
	result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != NO_ERROR) {
		wprintf(L"WSAStartup failed with error: %d\n", result);
		return 1;
	}

	// Create a socket for sending data
	serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (serverSocket == INVALID_SOCKET)
	{
		wprintf(L"socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		exit(EXIT_FAILURE);
	}

	memset(&servaddr, 0, sizeof(servaddr));
	memset(&cliaddr, 0, sizeof(cliaddr));

	// Filling server information
	servaddr.sin_family = AF_INET; // IPv4/6
	servaddr.sin_addr.s_addr = INADDR_ANY; // bind to any -> because it's a server
	servaddr.sin_port = htons(PORT);

	// Bind the socket with the server address
	if (bind(serverSocket, (const struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	int len, n;

	len = sizeof(cliaddr); //len is value/result

	n = recvfrom(serverSocket, (char*)buffer, MAXLINE, MSG_WAITALL, (struct sockaddr*)&cliaddr, &len);
	if (n < 0 || n > sizeof(buffer))
	{
		printf("Error while receiving: %s\n", n);
		exit(EXIT_FAILURE);
	}
	buffer[n] = '\0';

	printf("Client : %s\n", buffer);
	
	sendto(serverSocket, (const char*)hello, strlen(hello), 0, (const struct sockaddr*)&cliaddr, len);
	printf("Hello message sent.\n");

	_close(serverSocket);

	return 0;
}
