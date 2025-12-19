/**
 * @file arguments.h - FIXED (no duplicate defines)
 */
#ifndef ARGUMENTS_H
#define ARGUMENTS_H

#include "standard.h" /* Has DEFAULT_TIMEOUT already! */

/* NO MORE DEFAULT_TIMEOUT here! */

/**
 * @brief Command-line arguments structure
 */
typedef struct
{
    uint16_t Port;
    uint32_t Timeout;
    int32_t Count; /* -1 = infinite */
    bool Continuous;
    bool UseColor;
    pcc_t Destination;
    int32_t Type;
} arguments_t;

/* Function declarations */
void PrintBanner(void);
void PrintUsage(void);
int32_t ProcessArguments(int32_t const argc, char *const *const argv, arguments_t *const arguments);
void PrintError(pcc_t const message);

#endif /* ARGUMENTS_H */
