/**
 * @file i18n.h
 * @brief Internationalization (i18n) string interface.
 *
 * Declares string identifiers and the function used to fetch constant,
 * localized strings for messages printed by PingDD.
 */
#ifndef I18N_H
#define I18N_H

#include "standard.h"

/**
 * @enum string_id_t
 * @brief Identifiers for localized strings.
 *
 * These IDs are used with @ref GetString to select a specific constant message
 * format.
 */
typedef enum {
  /** @brief Usage/help text. */
  STRING_ID_USAGE = 0U,

  /** @brief "Connecting to %s on TCP %d:\\n" format string. */
  STRING_ID_CONNECT_INFO_FULL,

  /** @brief "[%s] " prefix format string. */
  STRING_ID_CONNECT_INFO_IP,

  /** @brief "Connected to %s: time=... protocol=TCP port=...\\n" format string.
   */
  STRING_ID_CONNECT_SUCCESS,

  /** @brief Connection statistics multi-line format string. */
  STRING_ID_STATS,

  /** @brief "Connection timeout" message string. */
  STRING_ID_CONNECTION_TIMEOUT,

  /** @brief Number of string IDs (sentinel value, not a valid string ID). */
  STRING_ID_COUNT
} string_id_t;

/**
 * @brief Get a constant string for a given string ID.
 *
 * @param[in] string_id String identifier.
 * @return Pointer to a constant string.
 *
 * @note If @p string_id is out of range, a fallback string is returned.
 */
pcc_t GetString(string_id_t const string_id);

#endif /* I18N_H */
