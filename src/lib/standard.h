/**
 * @file standard.h - MISRA C compliant (no Windows conflicts)
 */
#ifndef STANDARD_H
#define STANDARD_H

/* Standard C includes FIRST */
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>

/* Platform includes AFTER */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>
#include <netinet/in.h>
#define CLOSESOCKET(s) close(s)
#endif

/* Application constants */
#define NAME "PingDD"
#define AUTHOR "Jubair Hasan (Joy)"

/* Sizes */
#define HOSTNAME_MAX_LEN 256U
#define IPADDRESS_MAX_LEN 46U

/* Defaults */
#define DEFAULT_TIMEOUT (1000U)
#define DEFAULT_PORT (0U)
#define INFINITE_COUNT (-1)
#define DEFAULT_PORT (0U)   /* ADD THIS */
#define INFINITE_COUNT (-1) /* ADD THIS */

/* PingDD error codes (unique namespace) */
#define SUCCESS 0U
#define PINGDD_OUTOFMEMORY 100U
#define PINGDD_SOCKET_TIMEOUT 101U
#define PINGDD_SOCKET_RESOLVE 102U
#define PINGDD_SOCKET_FAILURE 103U
#define PINGDD_SOCKET_CLOSED 104U
#define PINGDD_INVALID_ARGS 200U

/* Types */
typedef const char *pcc_t;
typedef char *pc_t;

/* Utilities */
#define STATIC_UNUSED(x) ((void)(x))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#endif /* STANDARD_H */
