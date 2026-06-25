/**
 * @file print.c
 * @brief Terminal color output implementation.
 *
 * Implements the functions declared in print.h.
 */

#include "print.h"

#include <stdio.h>

bool UseColor = true;

static const char *Print_GetAnsiCode(int32_t color) {
  switch (color) {
  case PRINT_BLUE:
    return "\x1b[96m";
  case PRINT_GREEN:
    return "\x1b[92m";
  case PRINT_RED:
    return "\x1b[91m";
  case PRINT_YELLOW:
    return "\x1b[93m";
  case PRINT_WHITE:
    return "\x1b[97m";
  default:
    return "";
  }
}

void FormattedPrint(int32_t const color, pcc_t const data) {
  const char *code = NULL;

  if (data == NULL) {
    return;
  }

  if (!UseColor) {
    (void)printf("%s", data);
    return;
  }

  code = Print_GetAnsiCode(color);
  if (code[0] != '\0') {
    (void)printf("%s%s\x1b[0m", code, data);
  } else {
    (void)printf("%s", data);
  }
}

void ResetColor(void) {
  if (UseColor) {
    (void)printf("\x1b[0m");
  }
}
