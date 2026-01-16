/**
 * @file host.h
 * @brief Host string formatting helpers.
 *
 * Declares functions that return or build user-facing strings from a @c host_t
 * instance (IP address string, connection info line, and successful connection
 * line).
 */
#ifndef HOST_H
#define HOST_H

#include "standard.h"

/* Platform-specific in_addr */
#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

/**
 * @brief Get the host IP address as a string.
 *
 * @param[in] host Host information.
 * @return Pointer to an IP address string.
 *
 * @note If @p host is NULL or the stored IP string is empty, a fallback string
 * is returned.
 * @warning The returned pointer may refer to internal static storage.
 */
pcc_t IPAddressString(host_t const *const host);

/**
 * @brief Build the "connecting" message string.
 *
 * Writes a string like: @c "Connecting\ to\ <hostname>\ on\ TCP\ <port>:\\n"
 *
 * @param[in]  host     Host information.
 * @param[out] str      Output buffer to write the message into.
 * @param[in]  str_size Size of @p str in bytes.
 *
 * @retval PINGDD_INVALID_ARGS Invalid input (NULL pointer or zero buffer size).
 * @return On success, returns the value from @c snprintf (number of characters
 * written, excluding the null terminator; may be truncated if the buffer is too
 * small).
 */
int32_t GetConnectInfoString(host_t const *const host, char *const str,
                             size_t const str_size);

/**
 * @brief Build the "connected" success message string.
 *
 * Writes a string like:
 * @c "Connected\ to\ <ip>:\ time=<ms>ms\ protocol=TCP\ port=<port>\\n"
 *
 * @param[in]  host     Host information.
 * @param[out] str      Output buffer to write the message into.
 * @param[in]  str_size Size of @p str in bytes.
 * @param[in]  time     Connection time in seconds.
 *
 * @retval PINGDD_INVALID_ARGS Invalid input (NULL pointer or zero buffer size).
 * @return On success, returns the value from @c snprintf (number of characters
 * written, excluding the null terminator; may be truncated if the buffer is too
 * small).
 */
int32_t GetSuccessfulConnectionString(host_t const *const host, char *const str,
                                      size_t const str_size, double const time);

#endif /* HOST_H */
