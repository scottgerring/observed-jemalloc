/*
 * OTel heap-profiling USDT probe emission.
 *
 * Defines the otel_memory:alloc and otel_memory:free probe sites using the SystemTap/libbpf
 * USDT header.  These probes follow the otel_memory USDT contract defined in
 * the OTel eBPF profiler design doc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "otel_probes.h"

#include <sys/sdt.h>

void otel_probe_alloc(void *user, uint64_t size, uint64_t weighted_bytes) {
    DTRACE_PROBE3(otel_memory, alloc, user, size, weighted_bytes);
}

void otel_probe_free(void *ptr) {
    DTRACE_PROBE1(otel_memory, free, ptr);
}
