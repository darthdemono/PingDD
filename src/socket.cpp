#include "socket.h"
#include "i18n.h"
#include "timer.h"
#include <iostream>
#include <cstring>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#endif

std::string socket_c::GetFriendlyTypeName(int type)
{
	switch (type)
	{
	case ERROR_SOCKET_GENERALFAILURE:
		return i18n_c::GetString(ERROR_SOCKET_GENERALFAILURE);
	case ERROR_SOCKET_CANNOTRESOLVE:
		return i18n_c::GetString(ERROR_SOCKET_CANNOTRESOLVE);
	default:
		return i18n_c::GetString(0); // Default to "Unknown Error"
	}
}

void socket_c::SetPortAndType(int port, int type, host_c &host)
{
	host.Port = port;
	host.Type = type;
}

int socket_c::GetSocketType(int type)
{
	switch (type)
	{
	case IPPROTO_UDP:
		return SOCK_DGRAM;
	default:
		return SOCK_STREAM;
	}
}

int socket_c::Resolve(const std::string &destination, host_c &host)
{
	// Initialize Winsock on Windows
#ifdef WIN32
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cerr << "WSAStartup failed with error: " << WSAGetLastError() << std::endl;
		return ERROR_SOCKET_GENERALFAILURE;
	}
#endif

	addrinfo hints, *result = nullptr;
	std::memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	int ret = getaddrinfo(destination.c_str(), nullptr, &hints, &result);
	if (ret != 0)
	{
		std::cerr << "getaddrinfo failed: " << gai_strerror(ret) << std::endl;
#ifdef WIN32
		WSACleanup();
#endif
		return ERROR_SOCKET_CANNOTRESOLVE;
	}

	sockaddr_in *addr = reinterpret_cast<sockaddr_in *>(result->ai_addr);
	char ip[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(addr->sin_addr), ip, INET_ADDRSTRLEN);
	host.IPAddress = ip;

	freeaddrinfo(result);

#ifdef WIN32
	WSACleanup();
#endif

	return SUCCESS;
}

int socket_c::Connect(host_c host, int timeout, double &time)
{
	int clientSocket;
	int result = SUCCESS;

#ifdef WIN32
	// Initialize Winsock
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		return ERROR_SOCKET_GENERALFAILURE;
#endif

	// Create socket
	clientSocket = socket(AF_INET, socket_c::GetSocketType(host.Type), 0);
	if (clientSocket == -1)
	{
#ifdef WIN32
		// Cleanup Winsock
		WSACleanup();
#endif
		return ERROR_SOCKET_GENERALFAILURE;
	}

	// Set socket address information
	sockaddr_in clientAddress;
	clientAddress.sin_family = AF_INET;
	clientAddress.sin_port = htons(host.Port);
	if (inet_pton(AF_INET, host.IPAddress.c_str(), &clientAddress.sin_addr) <= 0)
	{
#ifdef WIN32
		closesocket(clientSocket);
		WSACleanup();
#else
		close(clientSocket);
#endif
		return ERROR_SOCKET_GENERALFAILURE;
	}

	// Set non-blocking mode
#ifdef WIN32
	u_long mode = 1;
	ioctlsocket(clientSocket, FIONBIO, &mode);
#else
	int flags = fcntl(clientSocket, F_GETFL, 0);
	if (fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		close(clientSocket);
		return ERROR_SOCKET_GENERALFAILURE;
	}
#endif

	// Set timeout
	timeval tv;
	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000;

	// Start timer
	timer_c timer;
	timer.Start();

	// Connect to server
	if (connect(clientSocket, (sockaddr *)&clientAddress, sizeof(clientAddress)) == SOCKET_ERROR)
	{
#ifdef WIN32
		int errorCode = WSAGetLastError();
		if (errorCode != WSAEWOULDBLOCK)
		{
			closesocket(clientSocket);
			WSACleanup();
			return ERROR_SOCKET_TIMEOUT;
		}
#else
		if (errno != EINPROGRESS)
		{
			close(clientSocket);
			return ERROR_SOCKET_TIMEOUT;
		}
#endif
	}

	// Use select to wait for socket to be ready
	fd_set read, write;
	FD_ZERO(&read);
	FD_ZERO(&write);
	FD_SET(clientSocket, &read);
	FD_SET(clientSocket, &write);

	result = select(clientSocket + 1, &read, &write, nullptr, &tv);
	if (result != 1)
	{
		close(clientSocket);
#ifdef WIN32
		WSACleanup();
#endif
		return ERROR_SOCKET_TIMEOUT;
	}

	// Stop timer and calculate time taken
	time = timer.Stop();

	// Check socket state
	if (!FD_ISSET(clientSocket, &read) && !FD_ISSET(clientSocket, &write))
	{
		close(clientSocket);
#ifdef WIN32
		WSACleanup();
#endif
		return ERROR_SOCKET_TIMEOUT;
	}

	// Determine if the connection was successful
	int error = 0;
	socklen_t len = sizeof(error);
	if (getsockopt(clientSocket, SOL_SOCKET, SO_ERROR, (char *)&error, &len) < 0)
	{
		close(clientSocket);
#ifdef WIN32
		WSACleanup();
#endif
		return ERROR_SOCKET_GENERALFAILURE;
	}

	if (error != 0)
	{
		close(clientSocket);
#ifdef WIN32
		WSACleanup();
#endif
		return ERROR_SOCKET_GENERALFAILURE;
	}

	// Close socket
	close(clientSocket);

#ifdef WIN32
	// Cleanup Winsock
	WSACleanup();
#endif

	return SUCCESS;
}
