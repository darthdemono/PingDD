/**
 * @file socket.c
 * @brief MISRA C compliant socket implementation for PingDD
 * @author Jubair Hasan (Joy)
 * @version 1.0.0
 * @date 2025-12-19
 */

#include "socket.h"
#include "host.h"
#include "timer.h"
#include "print.h"

#include <string.h>

/* Static function prototypes */
static int32_t InitializeWinsock(void);
// static void CleanupWinsock(void);
static void CloseSocket(int32_t socket_fd);

/**
 * @brief Get human-readable error message
 */
pcc_t GetFriendlyTypeName(int32_t const type)
{
    switch (type)
    {
    case SUCCESS:
        return "Success";
    case PINGDD_SOCKET_TIMEOUT:
        return "Connection timeout";
    case PINGDD_SOCKET_RESOLVE:
        return "Cannot resolve hostname";
    case PINGDD_SOCKET_FAILURE:
        return "Socket general failure";
    case PINGDD_SOCKET_CLOSED:
        return "Connection refused (closed port)";
    case PINGDD_INVALID_ARGS:
        return "Invalid arguments";
    default:
        return "Unknown error";
    }
}

/**
 * @brief Configure port and protocol type
 */
void SetPortAndType(uint16_t const port, int32_t const type, host_t *const host)
{
    if (host != NULL)
    {
        host->Port = port;
        host->Type = type;
    }
}

/**
 * @brief Get socket type from protocol
 */
int32_t GetSocketType(int32_t const type)
{
    switch (type)
    {
    case IPPROTO_UDP:
        return SOCK_DGRAM;
    case IPPROTO_TCP:
    default:
        return SOCK_STREAM;
    }
}

/**
 * @brief Resolve hostname to IP address
 */
int32_t Resolve(pcc_t const destination, host_t *const host)
{
    struct addrinfo hints = {0};
    struct addrinfo *result = NULL;
    int32_t ret = 0;

    /* Validate inputs */
    if ((destination == NULL) || (host == NULL))
    {
        return PINGDD_INVALID_ARGS;
    }

    /* Initialize Winsock (Windows only, once) */
    ret = InitializeWinsock();
    if (ret != SUCCESS)
    {
        return ret;
    }

    /* Setup hints */
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    /* Resolve address */
    ret = getaddrinfo(destination, NULL, &hints, &result);
    if (ret != 0)
    {

        return PINGDD_SOCKET_RESOLVE;
    }

    /* Extract IP address */
    if ((result != NULL) && (result->ai_addrlen >= sizeof(struct sockaddr_in)))
    {
        struct sockaddr_in *addr = (struct sockaddr_in *)result->ai_addr;
        char ip[IPADDRESS_MAX_LEN] = {0};

        if (inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip)) != NULL)
        {
            /* Safe string copy */
            strncpy(host->IPAddress, ip, sizeof(host->IPAddress) - 1U);
            host->IPAddress[sizeof(host->IPAddress) - 1U] = '\0';
            strncpy(host->Hostname, destination, sizeof(host->Hostname) - 1U);
            host->Hostname[sizeof(host->Hostname) - 1U] = '\0';
            host->ipAddress = addr->sin_addr.s_addr;
            host->HostIsIP = true;
        }
    }

    freeaddrinfo(result);
    return SUCCESS;
}

/**
 * @brief Establish TCP connection with timeout
 */
int32_t Connect(host_t const *const host, uint32_t const timeout_ms, double *const rtt)
{
    int32_t client_socket = INVALID_SOCKET;
    struct sockaddr_in server_addr = {0};
    struct timeval timeout = {0};
    pingdd_timer_t timer = {0};
    fd_set readfds, writefds;

    /* Validate inputs */
    if ((host == NULL) || (rtt == NULL))
    {
        return PINGDD_INVALID_ARGS;
    }

    /* Initialize Winsock */
    if (InitializeWinsock() != SUCCESS)
    {
        return PINGDD_SOCKET_FAILURE;
    }

    /* Create socket */
    client_socket = socket(AF_INET, GetSocketType(host->Type), 0);
    if (client_socket == (int32_t)INVALID_SOCKET)
    {

        return PINGDD_SOCKET_FAILURE;
    }

    /* Setup server address */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(host->Port);
    if (inet_pton(AF_INET, host->IPAddress, &server_addr.sin_addr) <= 0)
    {
        CloseSocket(client_socket);

        return PINGDD_SOCKET_FAILURE;
    }

    /* Set non-blocking */
#ifdef _WIN32
    {
        u_long mode = 1U;
        if (ioctlsocket(client_socket, FIONBIO, &mode) != 0)
        {
            CloseSocket(client_socket);

            return PINGDD_SOCKET_FAILURE;
        }
    }
#else
    {
        int32_t flags = fcntl(client_socket, F_GETFL, 0);
        if (fcntl(client_socket, F_SETFL, flags | O_NONBLOCK) == -1)
        {
            CloseSocket(client_socket);
            return PINGDD_SOCKET_FAILURE;
        }
    }
#endif

    /* Start timer */
    Timer_Start(&timer);

    /* Initiate non-blocking connect */
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR)
    {
#ifdef _WIN32
        if (WSAGetLastError() != WSAEWOULDBLOCK)
#else
        if (errno != EINPROGRESS)
#endif
        {
            CloseSocket(client_socket);

            return PINGDD_SOCKET_TIMEOUT;
        }
    }

    /* Setup select timeout */
    timeout.tv_sec = timeout_ms / 1000U;
    timeout.tv_usec = (timeout_ms % 1000U) * 1000U;

    /* Wait for connect completion */
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_SET(client_socket, &readfds);
    FD_SET(client_socket, &writefds);

    if (select(client_socket + 1, &readfds, &writefds, NULL, &timeout) != 1)
    {
        CloseSocket(client_socket);

        return PINGDD_SOCKET_TIMEOUT;
    }

    /* Get elapsed time */
    *rtt = Timer_Stop(&timer);

    /* Check socket error */
    {
        int32_t error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(client_socket, SOL_SOCKET, SO_ERROR, (char *)&error, &len) < 0)
        {
            CloseSocket(client_socket);

            return PINGDD_SOCKET_FAILURE;
        }

        if (error != 0)
        {
            CloseSocket(client_socket);
            return PINGDD_SOCKET_CLOSED;
        }
    }

    /* Cleanup */
    CloseSocket(client_socket);

    return SUCCESS;
}

/* Static helper functions */
static int32_t InitializeWinsock(void)
{
#ifdef _WIN32
    static bool initialized = false;
    if (!initialized)
    {
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
        {
            return PINGDD_SOCKET_FAILURE;
        }
        initialized = true;
    }
#endif
    return SUCCESS;
}

// static void CleanupWinsock(void)
// {
// #ifdef _WIN32
//     /* WSACleanup called once at program exit */
//     static bool cleaned = false;
//     if (cleaned)
//     {
//         return;
//     }
//     cleaned = true;
//     WSACleanup();
// #endif
// }

static void CloseSocket(int32_t socket_fd)
{
#ifdef _WIN32
    closesocket(socket_fd);
#else
    close(socket_fd);
#endif
}
