/**
 * @file mbedtls_pingdd_config.h
 * @brief PingDD overlay on top of the default mbedTLS configuration.
 *
 * Included via -DMBEDTLS_USER_CONFIG_FILE after mbedTLS's own default config.
 * PingDD drives the TLS stack over its own sockets, so the OS-coupled helper
 * modules (BSD sockets, timing, filesystem, persistent key storage) are turned
 * off. This keeps the vendored library portable across every PingDD target
 * (Linux x86/ARM, Windows x86/ARM64) with no extra platform glue.
 */
#ifndef MBEDTLS_PINGDD_CONFIG_H
#define MBEDTLS_PINGDD_CONFIG_H

/* We provide our own send/recv BIO, so the bundled socket layer is unused. */
#undef MBEDTLS_NET_C

/* No gettimeofday/QueryPerformanceCounter wrappers needed from mbedTLS. */
#undef MBEDTLS_TIMING_C

/* No filesystem access: certificates are passed in memory, never by path. */
#undef MBEDTLS_FS_IO

/* Persistent PSA key storage needs a filesystem; volatile keys are enough. */
#undef MBEDTLS_PSA_CRYPTO_STORAGE_C
#undef MBEDTLS_PSA_ITS_FILE_C

#endif /* MBEDTLS_PINGDD_CONFIG_H */
