/**
 * @file socket.h
 * @brief Socket and hostname resolution functions.
 *
 * Declares the functions used to resolve a destination to an IP address and to
 * attempt a TCP connection with a timeout.
 */
#ifndef PINGDD_SOCKET_H
#define PINGDD_SOCKET_H

#include "standard.h"

#ifdef _WIN32
typedef SOCKET pingdd_socket_t;
#define PINGDD_INVALID_SOCKET INVALID_SOCKET
#define PINGDD_SOCKET_ERROR SOCKET_ERROR
#else
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>

typedef int pingdd_socket_t;
#define PINGDD_INVALID_SOCKET (-1)
#define PINGDD_SOCKET_ERROR (-1)
#endif

/**
 * @brief Get a human-readable name for a PingDD result/error code.
 *
 * @param[in] type Result/error code.
 * @return Constant string describing the code.
 */
pcc_t GetFriendlyTypeName(int32_t const type);

/**
 * @brief Attempt a connection to the host with a timeout.
 *
 * @param[in]  host       Target host information (must contain an IPv4 address
 *                        string and port).
 * @param[in]  timeout_ms Timeout in milliseconds.
 * @param[out] rtt        Connection time in seconds.
 *
 * @retval SUCCESS               Connection succeeded.
 * @retval PINGDD_SOCKET_TIMEOUT Connection attempt timed out.
 * @retval PINGDD_SOCKET_CLOSED  Connection was refused (closed port).
 * @retval PINGDD_SOCKET_FAILURE Socket operation failed.
 * @retval PINGDD_INVALID_ARGS   Invalid input arguments.
 */
int32_t Connect(const host_t *const host, uint32_t const timeout_ms,
                double *const rtt);

/**
 * @brief Resolve a destination hostname to an IPv4 address.
 *
 * @param[in]  destination Hostname or IP address string.
 * @param[out] host        Output host structure to fill.
 *
 * @retval SUCCESS               Resolve succeeded.
 * @retval PINGDD_SOCKET_RESOLVE Resolve failed.
 * @retval PINGDD_INVALID_ARGS   Invalid input arguments.
 */
int32_t Resolve(pcc_t const destination, host_t *const host);

/**
 * @brief Set the port and protocol type in a host structure.
 *
 * @param[in]  port TCP/UDP port number.
 * @param[in]  type Protocol identifier (e.g., IPPROTO_TCP, IPPROTO_UDP).
 * @param[out] host Host structure to configure.
 *
 * @note If @p host is NULL, the function does nothing.
 */
void SetPortAndType(uint16_t const port, int32_t const type,
                    host_t *const host);

/**
 * @brief Get the socket type constant for a protocol identifier.
 *
 * @param[in] type Protocol identifier (e.g., IPPROTO_TCP, IPPROTO_UDP).
 * @return Socket type constant (e.g., SOCK_STREAM or SOCK_DGRAM).
 */
int32_t GetSocketType(int32_t const type);

#endif /* PINGDD_SOCKET_H */
