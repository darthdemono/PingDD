/**
 * @file host.h - CROSS-PLATFORM Windows/Linux MISRA C
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
 * @brief Host structure
 */
typedef struct
{
    char Hostname[HOSTNAME_MAX_LEN];
    char IPAddress[IPADDRESS_MAX_LEN];
    bool HostIsIP;
    uint16_t Port;
    int32_t Type;
    uint32_t ipAddress;
} host_t;

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
