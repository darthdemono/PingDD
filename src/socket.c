/**
 * @file socket.c
 * @brief Socket and hostname resolution implementation.
 *
 * Implements the functions declared in socket.h.
 */

#include "socket.h"

#include "print.h"
#include "timer.h"

#include <string.h>

static int32_t InitializeWinsock(void);
static void CloseSocket(pingdd_socket_t socket_fd);
static void SetSockAddrPort(host_t const *const host,
                            struct sockaddr_storage *const addr,
                            uint16_t const port);

pcc_t GetFriendlyTypeName(int32_t const type) {
  switch (type) {
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
  case PINGDD_SOCKET_UNREACH:
    return "Destination unreachable";
  case PINGDD_INVALID_ARGS:
    return "Invalid arguments";
  default:
    return "Unknown error";
  }
}

void SetPortAndType(uint16_t const port, int32_t const type,
                    host_t *const host) {
  if (host != NULL) {
    host->Port = port;
    host->Type = type;
  }
}

int32_t GetSocketType(int32_t const type) {
  switch (type) {
  case IPPROTO_UDP:
    return SOCK_DGRAM;
  case IPPROTO_TCP:
  default:
    return SOCK_STREAM;
  }
}

/**
 * @brief Write a port number into a stored socket address (IPv4 or IPv6).
 */
static void SetSockAddrPort(host_t const *const host,
                            struct sockaddr_storage *const addr,
                            uint16_t const port) {
  if (host->Family == AF_INET6) {
    ((struct sockaddr_in6 *)addr)->sin6_port = htons(port);
  } else {
    ((struct sockaddr_in *)addr)->sin_port = htons(port);
  }
}

int32_t Resolve(pcc_t const destination, host_t *const host) {
  struct addrinfo hints = (struct addrinfo){0};
  struct addrinfo *result = NULL;
  struct addrinfo *iter = NULL;
  int32_t resolved = 0;

  if ((destination == NULL) || (host == NULL)) {
    return PINGDD_INVALID_ARGS;
  }

  if (InitializeWinsock() != SUCCESS) {
    return PINGDD_SOCKET_FAILURE;
  }

  /* AF_UNSPEC lets getaddrinfo return both IPv4 and IPv6 candidates. */
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = GetSocketType(host->Type);
  hints.ai_protocol = host->Type;

  if (getaddrinfo(destination, NULL, &hints, &result) != 0) {
    return PINGDD_SOCKET_RESOLVE;
  }

  for (iter = result; iter != NULL; iter = iter->ai_next) {
    void *src_addr = NULL;
    char ip[IPADDRESS_MAX_LEN] = {0};

    if ((iter->ai_family != AF_INET) && (iter->ai_family != AF_INET6)) {
      continue;
    }
    if (iter->ai_addrlen > sizeof(host->SockAddr)) {
      continue;
    }

    if (iter->ai_family == AF_INET6) {
      src_addr = &((struct sockaddr_in6 *)iter->ai_addr)->sin6_addr;
    } else {
      src_addr = &((struct sockaddr_in *)iter->ai_addr)->sin_addr;
    }

    if (inet_ntop(iter->ai_family, src_addr, ip, sizeof(ip)) == NULL) {
      continue;
    }

    memcpy(&host->SockAddr, iter->ai_addr, iter->ai_addrlen);
    host->SockAddrLen = (socklen_t)iter->ai_addrlen;
    host->Family = iter->ai_family;

    strncpy(host->IPAddress, ip, sizeof(host->IPAddress) - 1U);
    host->IPAddress[sizeof(host->IPAddress) - 1U] = '\0';

    strncpy(host->Hostname, destination, sizeof(host->Hostname) - 1U);
    host->Hostname[sizeof(host->Hostname) - 1U] = '\0';

    resolved = 1;
    break;
  }

  freeaddrinfo(result);

  /* B3 fix: do not report success when no usable address was stored. */
  return (resolved != 0) ? SUCCESS : PINGDD_SOCKET_RESOLVE;
}

/**
 * @brief Map a socket-level errno/SO_ERROR value to a PingDD result code.
 */
static int32_t MapConnectError(int const err) {
#ifdef _WIN32
  switch (err) {
  case WSAECONNREFUSED:
    return PINGDD_SOCKET_CLOSED;
  case WSAETIMEDOUT:
    return PINGDD_SOCKET_TIMEOUT;
  case WSAEHOSTUNREACH:
  case WSAENETUNREACH:
    return PINGDD_SOCKET_UNREACH;
  default:
    return PINGDD_SOCKET_FAILURE;
  }
#else
  switch (err) {
  case ECONNREFUSED:
    return PINGDD_SOCKET_CLOSED;
  case ETIMEDOUT:
    return PINGDD_SOCKET_TIMEOUT;
  case EHOSTUNREACH:
  case ENETUNREACH:
    return PINGDD_SOCKET_UNREACH;
  default:
    return PINGDD_SOCKET_FAILURE;
  }
#endif
}

int32_t Connect(host_t const *const host, uint32_t const timeout_ms,
                double *const rtt) {
  pingdd_socket_t client_socket = PINGDD_INVALID_SOCKET;
  struct sockaddr_storage server_addr = (struct sockaddr_storage){0};
  struct timeval timeout = (struct timeval){0};
  pingdd_timer_t timer = (pingdd_timer_t){0};
  fd_set writefds;

  if ((host == NULL) || (rtt == NULL)) {
    return PINGDD_INVALID_ARGS;
  }

  if (InitializeWinsock() != SUCCESS) {
    return PINGDD_SOCKET_FAILURE;
  }

  client_socket = socket(host->Family, GetSocketType(host->Type), 0);
  if (client_socket == PINGDD_INVALID_SOCKET) {
    return PINGDD_SOCKET_FAILURE;
  }

  memcpy(&server_addr, &host->SockAddr, host->SockAddrLen);
  SetSockAddrPort(host, &server_addr, host->Port);

#ifdef _WIN32
  {
    u_long mode = 1U;
    if (ioctlsocket(client_socket, FIONBIO, &mode) != 0) {
      CloseSocket(client_socket);
      return PINGDD_SOCKET_FAILURE;
    }
  }
#else
  {
    int32_t flags = fcntl(client_socket, F_GETFL, 0);
    if (fcntl(client_socket, F_SETFL, flags | O_NONBLOCK) == -1) {
      CloseSocket(client_socket);
      return PINGDD_SOCKET_FAILURE;
    }
  }
#endif

  Timer_Start(&timer);

  if (connect(client_socket, (struct sockaddr *)&server_addr,
              host->SockAddrLen) == PINGDD_SOCKET_ERROR) {
#ifdef _WIN32
    int err = WSAGetLastError();
    if (err != WSAEWOULDBLOCK)
#else
    int err = errno;
    if (err != EINPROGRESS)
#endif
    {
      /* Immediate failure (e.g. refused on loopback). Map it accurately. */
      *rtt = Timer_Stop(&timer);
      CloseSocket(client_socket);
      return MapConnectError(err);
    }
  }

  timeout.tv_sec = (long)(timeout_ms / 1000U);
  timeout.tv_usec = (long)((timeout_ms % 1000U) * 1000U);

  /* Connect completion (success or failure) signals the socket writable. */
  FD_ZERO(&writefds);
  FD_SET(client_socket, &writefds);

  {
    int sel = 0;
#ifdef _WIN32
    /* Winsock ignores the first parameter of select(). */
    sel = select(0, NULL, &writefds, NULL, &timeout);
#else
    sel = select(client_socket + 1, NULL, &writefds, NULL, &timeout);
#endif
    if (sel == 0) {
      CloseSocket(client_socket);
      return PINGDD_SOCKET_TIMEOUT;
    }
    /* B2 fix: any positive count means the connect completed; inspect
     * SO_ERROR below instead of insisting select() return exactly 1. */
    if (sel < 0) {
      CloseSocket(client_socket);
      return PINGDD_SOCKET_FAILURE;
    }
  }

  *rtt = Timer_Stop(&timer);

  {
    int error = 0;
    socklen_t len = (socklen_t)sizeof(error);
    if (getsockopt(client_socket, SOL_SOCKET, SO_ERROR, (char *)&error, &len) <
        0) {
      CloseSocket(client_socket);
      return PINGDD_SOCKET_FAILURE;
    }

    if (error != 0) {
      CloseSocket(client_socket);
      return MapConnectError(error);
    }
  }

  CloseSocket(client_socket);
  return SUCCESS;
}

static int32_t InitializeWinsock(void) {
#ifdef _WIN32
  static bool initialized = false;

  if (!initialized) {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
      return PINGDD_SOCKET_FAILURE;
    }
    initialized = true;
  }
#endif

  return SUCCESS;
}

static void CloseSocket(pingdd_socket_t socket_fd) {
#ifdef _WIN32
  closesocket(socket_fd);
#else
  close(socket_fd);
#endif
}
