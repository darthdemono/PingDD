#pragma once

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS 1
#pragma comment(lib, "ws2_32.lib")

#include <Windows.h>
#include <WinSock2.h>
#include <time.h>
#include "gettimeofday.h"

#else // Assuming Linux
#define close(sock) close(sock)
#define snprintf _snprintf

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>

#endif

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>

// Program information
#define NAME "PingDD"
#define VERSION "1.0.0"
#define AUTHOR "Jubair Hasan (Joy)"

// Error codes
#define SUCCESS 0
#define ERROR_POUTOFMEMORY 100
#define ERROR_SOCKET_TIMEOUT 102
#define ERROR_INVALIDARGUMENTS 200
#define ERROR_SOCKET_GENERALFAILURE 103
#define ERROR_SOCKET_CANNOTRESOLVE 101

// Types
typedef const wchar_t *pcw_t;
typedef const char *pcc_t;
typedef wchar_t *pwc_t;
typedef char *pc_t;
typedef unsigned short ushort_t;