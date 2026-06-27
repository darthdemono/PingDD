/**
 * @file config.c
 * @brief Configuration-file loading implementation.
 *
 * Implements the functions declared in config.h. The parser is deliberately
 * small and dependency-free: a line-oriented `key = value` reader with `#`/`;`
 * comments. String-valued options are copied into module-static storage whose
 * lifetime is the whole program, so @ref arguments_t may keep `pcc_t` pointers
 * into them just as it does for `argv`.
 */

#include "config.h"

#include "print.h"

#include <ctype.h>
#include <errno.h>

/** @brief Backing storage for string-valued configuration options. */
typedef struct {
  char Port[64];
  char Protocol[64];
  char Interface[128];
  bool PortSet;
  bool ProtocolSet;
  bool InterfaceSet;
} config_storage_t;

static config_storage_t g_config_storage;

/** @brief Trim leading and trailing ASCII whitespace in place. */
static char *Trim(char *const s) {
  char *start = s;
  char *end = NULL;

  while ((*start != '\0') && (isspace((unsigned char)*start) != 0)) {
    start++;
  }
  end = start + strlen(start);
  while ((end > start) && (isspace((unsigned char)end[-1]) != 0)) {
    end--;
  }
  *end = '\0';
  return start;
}

/** @brief Parse a boolean token (true/false/yes/no/on/off/1/0). */
static bool ParseBool(pcc_t const text, bool *const out) {
  if ((strcmp(text, "true") == 0) || (strcmp(text, "yes") == 0) ||
      (strcmp(text, "on") == 0) || (strcmp(text, "1") == 0)) {
    *out = true;
    return true;
  }
  if ((strcmp(text, "false") == 0) || (strcmp(text, "no") == 0) ||
      (strcmp(text, "off") == 0) || (strcmp(text, "0") == 0)) {
    *out = false;
    return true;
  }
  return false;
}

/** @brief Parse a decimal long, rejecting junk and overflow. */
static bool ParseLongValue(pcc_t const text, long *const out) {
  char *end = NULL;
  long value = 0;

  if ((text == NULL) || (text[0] == '\0')) {
    return false;
  }
  errno = 0;
  value = strtol(text, &end, 10);
  if ((errno != 0) || (end == text) || (*end != '\0')) {
    return false;
  }
  *out = value;
  return true;
}

/** @brief Copy a string option into module storage and point @p slot at it. */
static void StoreString(char *const buf, size_t const buf_size, pcc_t const val,
                        pcc_t *const slot, bool *const flag) {
  (void)snprintf(buf, buf_size, "%s", val);
  *slot = buf;
  *flag = true;
}

/**
 * @brief Apply a single key/value pair onto the argument struct.
 * @retval SUCCESS             Recognised key, value accepted.
 * @retval PINGDD_INVALID_ARGS Unknown key or invalid value.
 */
static int32_t ApplyKeyValue(pcc_t const key, pcc_t const value,
                             arguments_t *const args) {
  long n = 0;
  bool b = false;
  char msg[160];

  if (strcmp(key, "port") == 0) {
    StoreString(g_config_storage.Port, sizeof(g_config_storage.Port), value,
                &args->PortSpec, &g_config_storage.PortSet);
    return SUCCESS;
  }
  if (strcmp(key, "protocol") == 0) {
    StoreString(g_config_storage.Protocol, sizeof(g_config_storage.Protocol),
                value, &args->ProtoSpec, &g_config_storage.ProtocolSet);
    return SUCCESS;
  }
  if (strcmp(key, "interface") == 0) {
    StoreString(g_config_storage.Interface, sizeof(g_config_storage.Interface),
                value, &args->Interface, &g_config_storage.InterfaceSet);
    return SUCCESS;
  }
  if (strcmp(key, "timeout") == 0) {
    if (!ParseLongValue(value, &n) || (n < 1) || (n > 3600000)) {
      PrintError("Config: 'timeout' must be 1..3600000");
      return PINGDD_INVALID_ARGS;
    }
    args->Timeout = (uint32_t)n;
    return SUCCESS;
  }
  if (strcmp(key, "count") == 0) {
    if (!ParseLongValue(value, &n) || (n < -1) || (n > INT32_MAX)) {
      PrintError("Config: 'count' must be -1 or >= 0");
      return PINGDD_INVALID_ARGS;
    }
    args->Count = (int32_t)n;
    return SUCCESS;
  }
  if (strcmp(key, "rate") == 0) {
    if (!ParseLongValue(value, &n) || (n < 0) || (n > 3600000)) {
      PrintError("Config: 'rate' must be 0..3600000");
      return PINGDD_INVALID_ARGS;
    }
    args->Rate = (uint32_t)n;
    return SUCCESS;
  }
  if (strcmp(key, "deadline") == 0) {
    if (!ParseLongValue(value, &n) || (n < 1) || (n > 86400000)) {
      PrintError("Config: 'deadline' must be 1..86400000");
      return PINGDD_INVALID_ARGS;
    }
    args->Deadline = (uint32_t)n;
    return SUCCESS;
  }
  if (strcmp(key, "color") == 0) {
    if (!ParseBool(value, &b)) {
      PrintError("Config: 'color' must be a boolean");
      return PINGDD_INVALID_ARGS;
    }
    args->UseColor = b;
    args->ForceColor = b;
    return SUCCESS;
  }
  if (strcmp(key, "quiet") == 0) {
    if (!ParseBool(value, &args->Quiet)) {
      PrintError("Config: 'quiet' must be a boolean");
      return PINGDD_INVALID_ARGS;
    }
    return SUCCESS;
  }
  if (strcmp(key, "audible") == 0) {
    if (!ParseBool(value, &args->Audible)) {
      PrintError("Config: 'audible' must be a boolean");
      return PINGDD_INVALID_ARGS;
    }
    return SUCCESS;
  }
  if (strcmp(key, "json") == 0) {
    if (!ParseBool(value, &args->Json)) {
      PrintError("Config: 'json' must be a boolean");
      return PINGDD_INVALID_ARGS;
    }
    if (args->Json) {
      args->UseColor = false;
    }
    return SUCCESS;
  }
  if (strcmp(key, "csv") == 0) {
    if (!ParseBool(value, &args->CSVOutput)) {
      PrintError("Config: 'csv' must be a boolean");
      return PINGDD_INVALID_ARGS;
    }
    return SUCCESS;
  }
  if (strcmp(key, "concurrent") == 0) {
    if (!ParseBool(value, &args->Concurrent)) {
      PrintError("Config: 'concurrent' must be a boolean");
      return PINGDD_INVALID_ARGS;
    }
    return SUCCESS;
  }
  if (strcmp(key, "tos") == 0) {
    if (!ParseLongValue(value, &n) || (n < 0) || (n > 255)) {
      PrintError("Config: 'tos' must be 0..255");
      return PINGDD_INVALID_ARGS;
    }
    args->Tos = (int32_t)n;
    return SUCCESS;
  }
  if (strcmp(key, "resolve") == 0) {
    if (!ParseBool(value, &args->Resolve)) {
      PrintError("Config: 'resolve' must be a boolean");
      return PINGDD_INVALID_ARGS;
    }
    return SUCCESS;
  }

  (void)snprintf(msg, sizeof(msg), "Config: unknown key '%s'", key);
  PrintError(msg);
  return PINGDD_INVALID_ARGS;
}

bool Config_DefaultPath(char *const out, size_t const out_size) {
  pcc_t home = NULL;

  if ((out == NULL) || (out_size == 0U)) {
    return false;
  }
  out[0] = '\0';

#ifdef _WIN32
  home = getenv("USERPROFILE");
#else
  home = getenv("HOME");
#endif
  if ((home == NULL) || (home[0] == '\0')) {
    return false;
  }
  (void)snprintf(out, out_size, "%s/.pingdd", home);
  return true;
}

int32_t Config_Load(pcc_t const path, arguments_t *const arguments,
                    bool const required) {
  FILE *file = NULL;
  char line[512];
  int32_t lineno = 0;

  if ((path == NULL) || (arguments == NULL)) {
    return PINGDD_INVALID_ARGS;
  }

  file = fopen(path, "r");
  if (file == NULL) {
    if (required) {
      char msg[256];
      (void)snprintf(msg, sizeof(msg), "Error: cannot open config file '%s'",
                     path);
      PrintError(msg);
      return PINGDD_INVALID_ARGS;
    }
    return SUCCESS; /* implicit ~/.pingdd is optional */
  }

  while (fgets(line, (int)sizeof(line), file) != NULL) {
    char *trimmed = NULL;
    char *eq = NULL;
    char *key = NULL;
    char *value = NULL;

    lineno++;

    /* Strip an inline comment introduced by '#' or ';'. */
    {
      char *hash = strchr(line, '#');
      char *semi = strchr(line, ';');
      if ((hash != NULL) && ((semi == NULL) || (hash < semi))) {
        *hash = '\0';
      } else if (semi != NULL) {
        *semi = '\0';
      }
    }

    trimmed = Trim(line);
    if (trimmed[0] == '\0') {
      continue; /* blank or comment-only line */
    }

    eq = strchr(trimmed, '=');
    if (eq == NULL) {
      char msg[128];
      (void)snprintf(msg, sizeof(msg),
                     "Config: line %d is not 'key = value'", lineno);
      PrintError(msg);
      (void)fclose(file);
      return PINGDD_INVALID_ARGS;
    }
    *eq = '\0';
    key = Trim(trimmed);
    value = Trim(eq + 1);

    if ((key[0] == '\0') || (value[0] == '\0')) {
      char msg[128];
      (void)snprintf(msg, sizeof(msg),
                     "Config: line %d has an empty key or value", lineno);
      PrintError(msg);
      (void)fclose(file);
      return PINGDD_INVALID_ARGS;
    }

    if (ApplyKeyValue(key, value, arguments) != SUCCESS) {
      (void)fclose(file);
      return PINGDD_INVALID_ARGS;
    }
  }

  (void)fclose(file);
  return SUCCESS;
}
