/**
 * @file socket.c
 * @brief Socket and hostname resolution implementation.
 *
 * Implements the functions declared in socket.h.
 */

#include "socket.h"

#include "host.h"
#include "print.h"
#include "timer.h"

#include <string.h>

static int32_t InitializeWinsock(void);
static void CloseSocket(pingdd_socket_t socket_fd);

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

int32_t Resolve(pcc_t const destination, host_t *const host) {
  struct addrinfo hints = (struct addrinfo){0};
  struct addrinfo *result = NULL;
  int32_t ret = 0;

  if ((destination == NULL) || (host == NULL)) {
    return PINGDD_INVALID_ARGS;
  }

  ret = InitializeWinsock();
  if (ret != SUCCESS) {
    return ret;
  }

  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  ret = getaddrinfo(destination, NULL, &hints, &result);
  if (ret != 0) {
    return PINGDD_SOCKET_RESOLVE;
  }

  if ((result != NULL) && (result->ai_addrlen >= sizeof(struct sockaddr_in))) {
    struct sockaddr_in *addr = (struct sockaddr_in *)result->ai_addr;
    char ip[IPADDRESS_MAX_LEN] = {0};

    if (inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip)) != NULL) {
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

int32_t Connect(host_t const *const host, uint32_t const timeout_ms,
                double *const rtt) {
  pingdd_socket_t client_socket = PINGDD_INVALID_SOCKET;
  struct sockaddr_in server_addr = (struct sockaddr_in){0};
  struct timeval timeout = (struct timeval){0};
  pingdd_timer_t timer = (pingdd_timer_t){0};
  fd_set readfds, writefds;

  if ((host == NULL) || (rtt == NULL)) {
    return PINGDD_INVALID_ARGS;
  }

  if (InitializeWinsock() != SUCCESS) {
    return PINGDD_SOCKET_FAILURE;
  }

  client_socket = socket(AF_INET, GetSocketType(host->Type), 0);
  if (client_socket == PINGDD_INVALID_SOCKET) {
    return PINGDD_SOCKET_FAILURE;
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(host->Port);
  if (inet_pton(AF_INET, host->IPAddress, &server_addr.sin_addr) <= 0) {
    CloseSocket(client_socket);
    return PINGDD_SOCKET_FAILURE;
  }

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
              sizeof(server_addr)) == SOCKET_ERROR) {
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

  timeout.tv_sec = (long)(timeout_ms / 1000U);
  timeout.tv_usec = (long)((timeout_ms % 1000U) * 1000U);

  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_SET(client_socket, &readfds);
  FD_SET(client_socket, &writefds);

  {
    int sel = 0;
#ifdef _WIN32
    /* Winsock ignores the first parameter of select(). */
    sel = select(0, &readfds, &writefds, NULL, &timeout);
#else
    sel = select(client_socket + 1, &readfds, &writefds, NULL, &timeout);
#endif
    if (sel != 1) {
      CloseSocket(client_socket);
      return PINGDD_SOCKET_TIMEOUT;
    }
  }

  *rtt = Timer_Stop(&timer);

  {
    int32_t error = 0;
    socklen_t len = (socklen_t)sizeof(error);
    if (getsockopt(client_socket, SOL_SOCKET, SO_ERROR, (char *)&error, &len) <
        0) {
      CloseSocket(client_socket);
      return PINGDD_SOCKET_FAILURE;
    }

    if (error != 0) {
      CloseSocket(client_socket);
      return PINGDD_SOCKET_CLOSED;
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
