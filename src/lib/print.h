/**
 * @file print.h
 * @brief Terminal output formatting interface.
 *
 * Declares functions used to print text with optional terminal colors and to
 * control the current terminal color state.
 */
#ifndef PRINT_H
#define PRINT_H

#include "standard.h"

/**
 * @brief Global flag that enables or disables colored output.
 *
 * When set to false, all print functions operate without emitting color escape
 * codes.
 */
extern bool UseColor;

/* Color codes */
#define PRINT_NONE 0
#define PRINT_BLUE 1
#define PRINT_GREEN 2
#define PRINT_RED 3
#define PRINT_YELLOW 4
#define PRINT_WHITE 7

/**
 * @brief Print a string using an optional color.
 *
 * If @ref UseColor is false, the string is printed without any color
 * formatting.
 *
 * @param[in] color Color selector (one of the PRINT_* macros).
 * @param[in] data  String to print.
 *
 * @note If @p data is NULL, nothing is printed.
 */
void FormattedPrint(int32_t const color, pcc_t const data);

/**
 * @brief Set the current terminal text color.
 *
 * If @ref UseColor is false, this function does nothing.
 *
 * @param[in] color Color selector (one of the PRINT_* macros).
 */
void SetColor(int32_t const color);

/**
 * @brief Reset the terminal text color to the default.
 *
 * If @ref UseColor is false, this function does nothing.
 */
void ResetColor(void);

#endif /* PRINT_H */
