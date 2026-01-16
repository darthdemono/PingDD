/**
 * @file i18n.c
 * @brief Internationalization (i18n) string table implementation.
 *
 * Implements the functions declared in i18n.h.
 */

#include "i18n.h"

static pcc_t const string_table[STRING_ID_COUNT] = {
    [STRING_ID_USAGE] =
        "PingDD - A simple ping utility\n"
        "Syntax: pingdd [options] destination\n"
        "\n"
        "Options:\n"
        "  -p, --port N       set TCP port N (required)\n"
        "  -t, --timeout N    timeout in milliseconds (default 1000)\n"
        "  -c, --count N      set number of checks to N (default infinite)\n"
        "  --no-color         disable color output\n"
        "  -?, --help         display this help\n",

    [STRING_ID_CONNECT_INFO_FULL] = "Connecting to %s on TCP %d:\n",
    [STRING_ID_CONNECT_INFO_IP] = "[%s] ",
    [STRING_ID_CONNECT_SUCCESS] =
        "Connected to %s: time=%.4fms protocol=TCP port=%d\n",

    [STRING_ID_STATS] =
        "Connection statistics:\n"
        "        Attempted = %lu , Connected = %lu , Failed = %lu ( %.2f%% )\n"
        "Approximate connection times:\n"
        "        Minimum = %.4fms , Maximum = %.4fms , Average = %.5fms\n",

    [STRING_ID_CONNECTION_TIMEOUT] = "Connection timeout"};

pcc_t GetString(string_id_t const string_id) {
  if (string_id < STRING_ID_COUNT) {
    return string_table[string_id];
  }

  return "Unknown string ID";
}
