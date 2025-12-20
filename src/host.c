/**
 * @file host.c - COMPLETE MISRA C IMPLEMENTATION
 */
#include "host.h"

pcc_t IPAddressString(host_t const *const host)
{
    static char fallback[IPADDRESS_MAX_LEN] = "0.0.0.0";

    if (host == NULL || host->IPAddress[0] == '\0')
    {
        return fallback;
    }
    return host->IPAddress;
}

int32_t GetConnectInfoString(host_t const *const host,
                             char *const str,
                             size_t const str_size)
{
    if (host == NULL || str == NULL || str_size == 0U)
    {
        return PINGDD_INVALID_ARGS;
    }

    return snprintf(str, str_size,
                    "Connecting to %s on TCP %u:\n",
                    host->Hostname, host->Port);
}

int32_t GetSuccessfulConnectionString(host_t const *const host,
                                      char *const str,
                                      size_t const str_size,
                                      double const time)
{
    if (host == NULL || str == NULL || str_size == 0U)
    {
        return PINGDD_INVALID_ARGS;
    }

    return snprintf(str, str_size,
                    "Connected to %s: time=%.4fms protocol=TCP port=%u\n",
                    IPAddressString(host), time * 1000.0, host->Port);
}
