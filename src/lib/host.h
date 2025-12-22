/**
 * @file host.h
 */
#ifndef HOST_H
#define HOST_H

#include "standard.h"

/* Platform-specific in_addr */
#ifdef _WIN32
#include <ws2tcpip.h> /* Windows: struct in_addr */
#else
#include <arpa/inet.h> /* Linux: struct in_addr */
#endif

/**
 * @brief Get IP as string
 */
pcc_t IPAddressString(host_t const *const host);

/**
 * @brief Connection info string
 */
int32_t GetConnectInfoString(host_t const *const host,
                             char *const str,
                             size_t const str_size);

/**
 * @brief Success connection string
 */
int32_t GetSuccessfulConnectionString(host_t const *const host,
                                      char *const str,
                                      size_t const str_size,
                                      double const time);

#endif /* HOST_H */
