/**
 * @file standard.h
 * @brief Common includes, types, and constants used across PingDD.
 *
 * This header provides:
 * - Standard and platform-specific includes.
 * - Shared project types (like @ref host_t).
 * - Project constants, default values, and error codes.
 */
#ifndef PINGDD_STANDARD_H
#define PINGDD_STANDARD_H

/* Standard C includes */
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Platform includes */
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

#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include <arpa/inet.h>  /* inet_ntop, inet_pton */
#include <netdb.h>      /* getaddrinfo, freeaddrinfo, struct addrinfo */
#include <netinet/in.h> /* struct sockaddr_in */

#include <errno.h>  /* errno */
#include <fcntl.h>  /* fcntl */
#include <unistd.h> /* usleep, close */

/**
 * @brief Close a socket on POSIX platforms.
 * @param s Socket file descriptor.
 */
#define CLOSESOCKET(s) close(s)

#endif /* _WIN32 */

/** @brief Application name string. */
#define NAME "PingDD"

/** @brief Application author string. */
#define AUTHOR "DarthDemono"

#ifndef _WIN32
/**
 * @brief Timezone offset in seconds west of UTC (platform-provided global).
 */
extern long timezone;

/**
 * @brief Daylight savings time flag (platform-provided global).
 */
extern int daylight;
#endif

/**
 * @typedef pcc_t
 * @brief Pointer to constant C string.
 */
typedef const char *pcc_t;

/**
 * @typedef pc_t
 * @brief Pointer to mutable C string.
 */
typedef char *pc_t;

/**
 * @struct host_t
 * @brief Stores resolved host and connection settings.
 */
typedef struct {
  /** @brief IPv4/IPv6 address string representation. */
  char IPAddress[64];

  /** @brief Hostname string as provided/resolved. */
  char Hostname[256];

  /** @brief IPv4 address as a 32-bit value (network order usage depends on
   * implementation). */
  uint32_t ipAddress;

  /** @brief Target port number. */
  uint16_t Port;

  /** @brief Protocol identifier (e.g., IPPROTO_TCP). */
  int32_t Type;

  /** @brief True if the destination was an IP address string. */
  bool HostIsIP;
} host_t;

/** @brief Maximum hostname string length. */
#define HOSTNAME_MAX_LEN 256U

/** @brief Maximum IP address string length (supports IPv6 text length). */
#define IPADDRESS_MAX_LEN 46U

/** @brief Default connection timeout in milliseconds. */
#define DEFAULT_TIMEOUT (1000U)

/** @brief Default port value (0 means "not set"). */
#define DEFAULT_PORT (0U)

/** @brief Value used to represent an infinite count. */
#define INFINITE_COUNT (-1)

/** @brief Operation succeeded. */
#define SUCCESS 0U

/** @brief Memory allocation failed. */
#define PINGDD_OUTOFMEMORY 100U

/** @brief Connection attempt timed out. */
#define PINGDD_SOCKET_TIMEOUT 101U

/** @brief Hostname resolution failed. */
#define PINGDD_SOCKET_RESOLVE 102U

/** @brief Generic socket failure. */
#define PINGDD_SOCKET_FAILURE 103U

/** @brief Connection refused / port closed. */
#define PINGDD_SOCKET_CLOSED 104U

/** @brief Invalid arguments were provided to a function. */
#define PINGDD_INVALID_ARGS 200U

/**
 * @brief Mark a variable as intentionally unused.
 * @param x Variable/expression to ignore.
 */
#define STATIC_UNUSED(x) ((void)(x))

/**
 * @brief Get number of elements in an array.
 * @param arr Array expression.
 */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#endif /* PINGDD_STANDARD_H */
