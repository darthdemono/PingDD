/**
 * @file print.c
 * @brief Terminal‑palette colored output (ANSI, cross‑platform)
 */

#include "print.h"
#include <stdio.h>

bool UseColor = true;

/* Map logical color to terminal ANSI code (palette-based) */
static const char *Print_GetAnsiCode(int32_t color)
{
    switch (color)
    {
    case PRINT_BLUE:
        /* Bright cyan from terminal palette */
        return "\x1b[96m";
    case PRINT_GREEN:
        /* Bright green */
        return "\x1b[92m";
    case PRINT_RED:
        /* Bright red */
        return "\x1b[91m";
    case PRINT_YELLOW:
        /* Bright yellow (optional) */
        return "\x1b[93m";
    case PRINT_WHITE:
        /* Bright white */
        return "\x1b[97m";
    default:
        return "";
    }
}

void FormattedPrint(int32_t const color, pcc_t const data)
{
    const char *code = NULL;

    if (data == NULL)
    {
        return;
    }

    if (!UseColor)
    {
        (void)printf("%s", data);
        return;
    }

    code = Print_GetAnsiCode(color);
    if (code[0] != '\0')
    {
        (void)printf("%s%s\x1b[0m", code, data);
    }
    else
    {
        (void)printf("%s", data);
    }
}

void SetColor(int32_t const color)
{
    const char *code = NULL;

    if (!UseColor)
    {
        return;
    }

    code = Print_GetAnsiCode(color);
    if (code[0] != '\0')
    {
        (void)printf("%s", code);
    }
}

void ResetColor(void)
{
    if (UseColor)
    {
        (void)printf("\x1b[0m");
    }
}
