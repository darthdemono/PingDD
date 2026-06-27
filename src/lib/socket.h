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
 * @brief Get a human-readable name for a PingDD result or error code.
 *
 * @param[in] type Result/error code.
 * @return Constant string describing the code.
 */
pcc_t GetFriendlyTypeName(int32_t const type);

/**
 * @brief Probe the host with a timeout, trying each resolved address in turn.
 *
 * For TCP this is a connect() handshake; for UDP a datagram is sent and the
 * socket is watched for a reply or an ICMP error. The first address that
 * yields a definitive answer wins.
 *
 * @param[in]  host        Target host information (resolved addresses + port).
 * @param[in]  timeout_ms  Timeout in milliseconds (per address).
 * @param[out] rtt         Round-trip time in seconds.
 * @param[out] out_ip      Buffer receiving the IP string actually probed.
 * @param[in]  out_ip_size Size of @p out_ip in bytes.
 * @param[out] out_ttl     Receives the reply TTL/hop-limit, or -1 when the
 *                         platform/probe type cannot report it. May be NULL.
 *
 * @retval SUCCESS                 Port reachable (TCP connected / UDP replied).
 * @retval PINGDD_SOCKET_TIMEOUT   Attempt timed out.
 * @retval PINGDD_SOCKET_CLOSED    Connection refused (closed port).
 * @retval PINGDD_SOCKET_UNREACH   Destination unreachable.
 * @retval PINGDD_UDP_OPENFILTERED UDP probe got no response.
 * @retval PINGDD_SOCKET_FAILURE   Socket operation failed.
 * @retval PINGDD_INTERRUPTED      Aborted by user interrupt.
 * @retval PINGDD_INVALID_ARGS     Invalid input arguments.
 */
int32_t Connect(const host_t *const host, uint32_t const timeout_ms,
                double *const rtt, char *const out_ip, size_t const out_ip_size,
                int32_t *const out_ttl);

/**
 * @brief Set the IP ToS/DSCP byte applied to all subsequent probe sockets.
 *
 * @param[in] tos ToS byte value (0..255), or negative to leave the OS default.
 */
void SetTos(int32_t const tos);

/**
 * @brief Resolve a destination into one or more IPv4/IPv6 addresses.
 *
 * @param[in]  destination Hostname or IP address string.
 * @param[out] host        Output host structure to fill (Type must be set).
 *
 * @retval SUCCESS              Resolve succeeded.
 * @retval PINGDD_SOCKET_RESOLVE Resolve failed.
 * @retval PINGDD_INVALID_ARGS   Invalid input arguments.
 */
int32_t Resolve(pcc_t const destination, host_t *const host);

/**
 * @brief Reverse-resolve an address to a hostname (PTR lookup).
 *
 * @param[in]  addr     Socket address to look up.
 * @param[in]  addrlen  Length of @p addr.
 * @param[out] out      Buffer receiving the hostname (empty on failure).
 * @param[in]  out_size Size of @p out in bytes.
 *
 * @retval SUCCESS              A name was found.
 * @retval PINGDD_SOCKET_RESOLVE No name (or lookup failed).
 * @retval PINGDD_INVALID_ARGS   Invalid input arguments.
 */
int32_t ReverseResolve(const struct sockaddr *const addr,
                       socklen_t const addrlen, char *const out,
                       size_t const out_size);

/**
 * @brief Test whether an address is private/loopback/link-local.
 *
 * Used as a safety gate for load-test and resilience modes: public targets are
 * refused unless explicitly authorized.
 *
 * @param[in] addr Socket address to classify.
 * @retval true  Address is loopback, RFC1918/ULA, CGNAT, or link-local.
 * @retval false Address is public (or unknown family).
 */
bool IsPrivateAddress(const struct sockaddr *const addr);

/**
 * @brief Bind subsequent probes to a source interface (by IP or name).
 *
 * Accepts a literal source IP (IPv4 or IPv6) or, on POSIX, an interface name
 * such as "eth0"/"wlan0" (resolved to its address via getifaddrs). Probes then
 * leave via that interface, enabling wifi-vs-ethernet path comparison.
 *
 * @param[in]  spec     Source IP string or interface name.
 * @param[out] err      Buffer for a human-readable error on failure.
 * @param[in]  err_size Size of @p err.
 * @retval SUCCESS              Source set for at least one family.
 * @retval PINGDD_INVALID_ARGS  Could not resolve the interface/IP.
 */
int32_t SetSourceInterface(pcc_t const spec, char *const err,
                           size_t const err_size);

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
