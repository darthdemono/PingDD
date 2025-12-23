/**
 * @file i18n.h
 * @brief  internationalization interface for PingDD
 * @author Jubair Hasan (Joy)
 */

#ifndef I18N_H
#define I18N_H

#include "standard.h"
#include <stdbool.h>

/* String ID enumeration for type safety */
typedef enum
{
    STRING_ID_USAGE = 0U,
    STRING_ID_CONNECT_INFO_FULL,
    STRING_ID_CONNECT_INFO_IP,
    STRING_ID_CONNECT_SUCCESS,
    STRING_ID_STATS,
    STRING_ID_CONNECTION_TIMEOUT,
    STRING_ID_COUNT
} string_id_t;

/**
 * @brief Get localized string by ID
 * @param string_id String identifier
 * @return Constant string pointer (never NULL)
 */
pcc_t GetString(string_id_t const string_id);

#endif /* I18N_H */
