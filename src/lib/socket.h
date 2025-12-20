/**
 * @file socket.h
 * @brief MISRA C compliant socket interface for PingDD
 * @author Jubair Hasan (Joy)
 * @version 1.0.0
 * @date 2025-12-19
 */

#ifndef SOCKET_H
#define SOCKET_H

#include "standard.h"

#ifndef INVALID_SOCKET /* Only define if not already defined */
#define INVALID_SOCKET -1
#endif
#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

/* Define socket constants */
#ifdef _WIN32
#else
#include <sys/socket.h>
#include <netdb.h>
#include <sys/select.h>
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

/* Function prototypes using host_t */
/**
 * @brief Get human-readable name for error/result codes
 * @param type Error/result code
 * @return Constant string describing the type
 */
pcc_t GetFriendlyTypeName(int32_t const type);

/**
 * @brief Establish TCP connection with timeout
 * @param host Target host information
 * @param timeout Timeout in milliseconds
 * @param rtt Output: round-trip time in seconds
 * @return SUCCESS or error code
 */
int32_t Connect(const host_t *const host, uint32_t const timeout_ms, double *const rtt);
/**
 * @brief Resolve hostname to IP address
 * @param destination Hostname or IP address
 * @param host Output: resolved host structure
 * @return SUCCESS or error code
 */
int32_t Resolve(pcc_t const destination, host_t *const host);

/**
 * @brief Configure port and protocol type
 * @param port TCP/UDP port number
 * @param type Protocol type (IPPROTO_TCP, etc.)
 * @param host Host structure to configure
 */
void SetPortAndType(uint16_t const port, int32_t const type, host_t *const host);

/**
 * @brief Get socket protocol type constant
 * @param type Protocol identifier
 * @return Socket type constant
 */
int32_t GetSocketType(int32_t const type);

#endif /* SOCKET_H */
