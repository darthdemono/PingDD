/**
 * @file config.h
 * @brief Configuration-file loading for PingDD.
 *
 * PingDD reads an optional INI-style configuration file before parsing the
 * command line, so persistent defaults can be set once. Precedence is:
 *
 *   built-in defaults  <  config file  <  command-line flags
 *
 * The file is a simple list of `key = value` lines. Blank lines and lines
 * beginning with `#` or `;` are ignored. Keys mirror the long command-line
 * options (e.g. `timeout`, `protocol`, `color`). Unknown keys are a hard error
 * so typos are not silently ignored.
 */
#ifndef PINGDD_CONFIG_H
#define PINGDD_CONFIG_H

#include "arguments.h"
#include "standard.h"

/**
 * @brief Resolve the default configuration-file path (`~/.pingdd`).
 *
 * @param[out] out      Buffer that receives the path.
 * @param[in]  out_size Size of @p out in bytes.
 *
 * @retval true  A home directory was found and the path was written.
 * @retval false No home directory in the environment; @p out is set empty.
 */
bool Config_DefaultPath(char *const out, size_t const out_size);

/**
 * @brief Load a configuration file into an @ref arguments_t.
 *
 * Values found in the file overwrite the current contents of @p arguments, so
 * call this after initialising defaults and before parsing the command line.
 *
 * @param[in]     path      Path to the configuration file.
 * @param[in,out] arguments Argument struct to populate.
 * @param[in]     required  When true, a missing file is an error; when false,
 *                          a missing file is silently ignored (used for the
 *                          implicit `~/.pingdd`).
 *
 * @retval SUCCESS             File loaded (or absent and not required).
 * @retval PINGDD_INVALID_ARGS Parse error or a required file was missing.
 */
int32_t Config_Load(pcc_t const path, arguments_t *const arguments,
                    bool const required);

#endif /* PINGDD_CONFIG_H */
