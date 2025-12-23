/**
 * @file print.h
 */
#ifndef PRINT_H
#define PRINT_H

#include "standard.h"

extern bool UseColor;

/* Color codes (simple integers - MISRA Rule 10.6) */
#define PRINT_NONE 0
#define PRINT_BLUE 1
#define PRINT_GREEN 2
#define PRINT_RED 3
#define PRINT_YELLOW 4
#define PRINT_WHITE 7

/**
 * @brief Print with color
 */
void FormattedPrint(int32_t const color, pcc_t const data);

/**
 * @brief Set color
 */
void SetColor(int32_t const color);

/**
 * @brief Reset color
 */
void ResetColor(void);

#endif /* PRINT_H */
