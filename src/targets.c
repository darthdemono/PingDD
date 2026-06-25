/**
 * @file targets.c
 * @brief Multi-target probe set implementation.
 */

#include "targets.h"

#include "socket.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------------
 * Small parsers
 * ------------------------------------------------------------------------ */

/** @brief Map a protocol token (case-insensitive) to IPPROTO_*, or -1. */
static int32_t ProtoFromToken(const char *tok) {
  size_t n = 0;
  char low[8] = {0};
  for (n = 0; (tok[n] != '\0') && (n < sizeof(low) - 1U); n++) {
    low[n] = (char)tolower((unsigned char)tok[n]);
  }
  if (strcmp(low, "tcp") == 0) {
    return IPPROTO_TCP;
  }
  if (strcmp(low, "udp") == 0) {
    return IPPROTO_UDP;
  }
  if (strcmp(low, "icmp") == 0) {
    return IPPROTO_ICMP;
  }
  return -1;
}

/** @brief Parse "80,443,8000-8010" into a port array. */
static int ParsePortList(const char *spec, uint16_t *out, size_t max,
                         size_t *count, char *err, size_t err_size) {
  char buf[512] = {0};
  char *save = NULL;
  char *tok = NULL;

  *count = 0;
  if (spec == NULL) {
    return 0;
  }
  (void)strncpy(buf, spec, sizeof(buf) - 1U);

  for (tok = strtok_r(buf, ",", &save); tok != NULL;
       tok = strtok_r(NULL, ",", &save)) {
    char *dash = strchr(tok, '-');
    long lo = 0;
    long hi = 0;
    char *end = NULL;

    if (dash != NULL) {
      *dash = '\0';
      lo = strtol(tok, &end, 10);
      hi = strtol(dash + 1, &end, 10);
    } else {
      lo = strtol(tok, &end, 10);
      hi = lo;
    }
    if ((lo < 1) || (hi > 65535) || (lo > hi)) {
      (void)snprintf(err, err_size, "invalid port spec near '%s'", tok);
      return -1;
    }
    for (; lo <= hi; lo++) {
      if (*count >= max) {
        (void)snprintf(err, err_size, "too many ports (max %zu)", max);
        return -1;
      }
      out[(*count)++] = (uint16_t)lo;
    }
  }
  return 0;
}

/** @brief Parse "TCP,UDP,ICMP" into a protocol array. */
static int ParseProtoList(const char *spec, int32_t *out, size_t max,
                          size_t *count, char *err, size_t err_size) {
  char buf[64] = {0};
  char *save = NULL;
  char *tok = NULL;

  *count = 0;
  if (spec == NULL) {
    out[(*count)++] = IPPROTO_TCP; /* default */
    return 0;
  }
  (void)strncpy(buf, spec, sizeof(buf) - 1U);

  for (tok = strtok_r(buf, ",", &save); tok != NULL;
       tok = strtok_r(NULL, ",", &save)) {
    int32_t t = ProtoFromToken(tok);
    if (t < 0) {
      (void)snprintf(err, err_size, "invalid protocol '%s'", tok);
      return -1;
    }
    if (*count >= max) {
      return -1;
    }
    out[(*count)++] = t;
  }
  if (*count == 0) {
    out[(*count)++] = IPPROTO_TCP;
  }
  return 0;
}

/* --------------------------------------------------------------------------
 * List management
 * ------------------------------------------------------------------------ */

static int TargetList_Add(target_list_t *list, const char *host, uint16_t port,
                          int32_t type) {
  probe_target_t *t = NULL;

  if (list->Count == list->Cap) {
    size_t cap = (list->Cap == 0U) ? 8U : (list->Cap * 2U);
    probe_target_t *grown =
        (probe_target_t *)realloc(list->Items, cap * sizeof(probe_target_t));
    if (grown == NULL) {
      return -1;
    }
    list->Items = grown;
    list->Cap = cap;
  }

  t = &list->Items[list->Count];
  memset(t, 0, sizeof(*t));
  (void)strncpy(t->Host, host, sizeof(t->Host) - 1U);
  t->Port = port;
  t->Type = type;
  Stats_Init(&t->Stats);

  if (type == IPPROTO_ICMP) {
    (void)snprintf(t->Label, sizeof(t->Label), "%s icmp", t->Host);
  } else {
    (void)snprintf(t->Label, sizeof(t->Label), "%s:%u %s", t->Host,
                   (unsigned)port, (type == IPPROTO_UDP) ? "udp" : "tcp");
  }

  list->Count++;
  return 0;
}

/** @brief Parse one "host:port/proto" (or "[v6]:port/proto", "host/icmp"). */
static int AddSpec(target_list_t *list, const char *spec, char *err,
                   size_t err_size) {
  char work[300] = {0};
  char host[256] = {0};
  char *slash = NULL;
  int32_t type = IPPROTO_TCP;
  long port = 0;
  const char *hostport = work;

  (void)strncpy(work, spec, sizeof(work) - 1U);

  slash = strrchr(work, '/');
  if (slash != NULL) {
    int32_t t = ProtoFromToken(slash + 1);
    if (t < 0) {
      (void)snprintf(err, err_size, "invalid protocol in '%s'", spec);
      return -1;
    }
    type = t;
    *slash = '\0';
  }

  if (work[0] == '[') {
    /* [IPv6]:port */
    char *rb = strchr(work, ']');
    if (rb == NULL) {
      (void)snprintf(err, err_size, "unterminated '[' in '%s'", spec);
      return -1;
    }
    *rb = '\0';
    (void)strncpy(host, work + 1, sizeof(host) - 1U);
    if (*(rb + 1) == ':') {
      port = strtol(rb + 2, NULL, 10);
    }
  } else {
    char *colon = strrchr(work, ':');
    if (colon != NULL) {
      *colon = '\0';
      (void)strncpy(host, hostport, sizeof(host) - 1U);
      port = strtol(colon + 1, NULL, 10);
    } else {
      (void)strncpy(host, hostport, sizeof(host) - 1U);
    }
  }

  if (host[0] == '\0') {
    (void)snprintf(err, err_size, "missing host in '%s'", spec);
    return -1;
  }
  if (type != IPPROTO_ICMP) {
    if ((port < 1) || (port > 65535)) {
      (void)snprintf(err, err_size, "TCP/UDP target '%s' needs a valid port",
                     spec);
      return -1;
    }
  }
  return TargetList_Add(list, host, (uint16_t)port, type);
}

/** @brief Parse one whitespace-separated targets-file line. */
static int AddFileLine(target_list_t *list, char *line, char *err,
                       size_t err_size) {
  char *save = NULL;
  char *tok = NULL;
  char host[256] = {0};
  int32_t type = IPPROTO_TCP;
  long port = 0;
  int have_proto = 0;
  int have_port = 0;
  int field = 0;

  for (tok = strtok_r(line, " \t", &save); tok != NULL;
       tok = strtok_r(NULL, " \t", &save)) {
    if (field == 0) {
      (void)strncpy(host, tok, sizeof(host) - 1U);
    } else {
      int32_t t = ProtoFromToken(tok);
      if (t >= 0) {
        type = t;
        have_proto = 1;
      } else {
        char *end = NULL;
        long p = strtol(tok, &end, 10);
        if ((end != tok) && (*end == '\0')) {
          port = p;
          have_port = 1;
        }
      }
    }
    field++;
  }
  (void)have_proto;

  if (host[0] == '\0') {
    return 0; /* blank line */
  }
  if ((type != IPPROTO_ICMP) && (have_port == 0)) {
    (void)snprintf(err, err_size, "targets-file line for '%s' needs a port",
                   host);
    return -1;
  }
  return TargetList_Add(list, host, (uint16_t)port, type);
}

/* --------------------------------------------------------------------------
 * Public build
 * ------------------------------------------------------------------------ */

int Targets_Build(const arguments_t *const args, target_list_t *const list,
                  char *const err, size_t const err_size) {
  if ((args == NULL) || (list == NULL)) {
    return -1;
  }
  list->Items = NULL;
  list->Count = 0;
  list->Cap = 0;

  /* 1) Positional host matrix: hosts x ports x protocols. */
  if (args->HostCount > 0U) {
    uint16_t ports[1024];
    int32_t protos[8];
    size_t nport = 0;
    size_t nproto = 0;
    size_t h = 0;
    size_t pi = 0;
    size_t ti = 0;
    int need_port = 0;

    if (ParsePortList(args->PortSpec, ports, 1024U, &nport, err, err_size) !=
        0) {
      return -1;
    }
    if (ParseProtoList(args->ProtoSpec, protos, 8U, &nproto, err, err_size) !=
        0) {
      return -1;
    }
    for (ti = 0; ti < nproto; ti++) {
      if (protos[ti] != IPPROTO_ICMP) {
        need_port = 1;
      }
    }
    if (need_port && (nport == 0U)) {
      (void)snprintf(err, err_size,
                     "TCP/UDP targets need a port: pass -p <port[,list,a-b]>");
      return -1;
    }

    for (h = 0; h < args->HostCount; h++) {
      for (ti = 0; ti < nproto; ti++) {
        if (protos[ti] == IPPROTO_ICMP) {
          if (TargetList_Add(list, args->Hosts[h], 0U, IPPROTO_ICMP) != 0) {
            return -1;
          }
        } else {
          for (pi = 0; pi < nport; pi++) {
            if (TargetList_Add(list, args->Hosts[h], ports[pi], protos[ti]) !=
                0) {
              return -1;
            }
          }
        }
      }
    }
  }

  /* 2) Repeated --target specs. */
  {
    size_t s = 0;
    for (s = 0; s < args->TargetSpecCount; s++) {
      if (AddSpec(list, args->TargetSpecs[s], err, err_size) != 0) {
        return -1;
      }
    }
  }

  /* 3) Targets file. */
  if (args->TargetsFile != NULL) {
    FILE *f = fopen(args->TargetsFile, "r");
    char line[512];
    if (f == NULL) {
      (void)snprintf(err, err_size, "cannot open targets file '%s'",
                     args->TargetsFile);
      return -1;
    }
    while (fgets(line, sizeof(line), f) != NULL) {
      char *nl = strpbrk(line, "\r\n");
      char *p = line;
      if (nl != NULL) {
        *nl = '\0';
      }
      while ((*p == ' ') || (*p == '\t')) {
        p++;
      }
      if ((*p == '\0') || (*p == '#')) {
        continue;
      }
      if (AddFileLine(list, p, err, err_size) != 0) {
        (void)fclose(f);
        return -1;
      }
    }
    (void)fclose(f);
  }

  if (list->Count == 0U) {
    (void)snprintf(err, err_size, "no valid targets");
    return -1;
  }
  return 0;
}

size_t Targets_Resolve(target_list_t *const list) {
  size_t i = 0;
  size_t ok = 0;

  if (list == NULL) {
    return 0;
  }

  for (i = 0; i < list->Count; i++) {
    probe_target_t *t = &list->Items[i];
    size_t j = 0;
    int cached = 0;

    /* Reuse a previously-resolved identical host (same string). */
    for (j = 0; j < i; j++) {
      probe_target_t *p = &list->Items[j];
      if (p->ResolveOk && (strcmp(p->Host, t->Host) == 0)) {
        host_t saved_type_port;
        (void)saved_type_port;
        t->Resolved = p->Resolved; /* copy addresses */
        t->Resolved.Type = t->Type;
        t->Resolved.Port = t->Port;
        t->ResolveOk = true;
        cached = 1;
        break;
      }
    }
    if (cached) {
      ok++;
      continue;
    }

    SetPortAndType(t->Port, t->Type, &t->Resolved);
    t->ResolveErr = Resolve(t->Host, &t->Resolved);
    t->ResolveOk = (t->ResolveErr == (int32_t)SUCCESS);
    if (t->ResolveOk) {
      ok++;
    }
  }
  return ok;
}

void Targets_Free(target_list_t *const list) {
  size_t i = 0;
  if ((list == NULL) || (list->Items == NULL)) {
    return;
  }
  for (i = 0; i < list->Count; i++) {
    Stats_Free(&list->Items[i].Stats);
  }
  free(list->Items);
  list->Items = NULL;
  list->Count = 0;
  list->Cap = 0;
}
