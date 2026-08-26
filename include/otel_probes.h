/*
 * OTel heap-profiling probe interface.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef OTEL_PROBES_H
#define OTEL_PROBES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Emit otel_memory:alloc USDT probe.
 *
 * Arguments:
 *   user           - pointer returned to the application
 *   size           - application-requested allocation size in bytes
 *   weighted_bytes - Horvitz-Thompson estimate of application-requested bytes
 */
void otel_probe_alloc(void *user, uint64_t size, uint64_t weighted_bytes);

/*
 * Emit otel_memory:free USDT probe.
 *
 * Arguments:
 *   ptr - pointer being freed (same value previously passed to otel_memory:alloc)
 */
void otel_probe_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* OTEL_PROBES_H */
