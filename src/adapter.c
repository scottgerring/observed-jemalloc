/*
 * observed-jemalloc adapter
 *
 * Installs OTel heap-profiling USDT hooks into stock jemalloc's experimental
 * prof_sample / prof_sample_free callbacks.  Runs entirely from a shared-object
 * constructor -- no application-side initialization is needed.
 *
 * Usage:
 *   LD_PRELOAD=/path/to/libjemalloc.so.2 ./application
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  /* ssize_t */

#include "otel_probes.h"

/* jemalloc mallctl interface (linked from libjemalloc_pic.a) */
extern int mallctl(const char *name, void *oldp, size_t *oldlenp,
                   void *newp, size_t newlen);

/* ---------------------------------------------------------------------------
 * Sampling interval (cached at init)
 * ---------------------------------------------------------------------------
 * lg_prof_sample is the base-2 log of the mean sampling interval in bytes.
 * Default: 19 (512 KiB).  Overridable via MALLOC_CONF=lg_prof_sample:N.
 *
 * v1: read once during constructor.  Changing the interval at runtime via
 * prof.reset is not supported and will produce incorrect weights.
 */
static uint64_t g_sampling_interval;
static bool g_debug;
static atomic_uint g_alloc_debug_count;
static atomic_uint g_free_debug_count;

static void debugf(const char *format, ...) {
    if (!g_debug) {
        return;
    }

    char message[512];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    if (length <= 0) {
        return;
    }
    if ((size_t)length > sizeof(message)) {
        length = (int)sizeof(message);
    }
    ssize_t written = write(STDERR_FILENO, message, (size_t)length);
    (void)written;
}

/* ---------------------------------------------------------------------------
 * Weight calculation
 * ---------------------------------------------------------------------------
 * The OTel heap signal describes application behavior, so both `size` and
 * `weighted_bytes` represent bytes requested by the application.
 *
 * Jemalloc selects samples using its usable (size-class) allocation size
 * rather than the application-requested size.  Therefore usable_size is used
 * here solely to calculate this allocation's inclusion probability.  It is
 * never reported in the USDT event:
 *
 *     p              = 1 - exp(-usable_size / sampling_interval)
 *     weighted_bytes = requested_size / p
 *
 * This is a Horvitz-Thompson estimator of total application-requested bytes.
 */
static inline uint64_t calculate_weighted_bytes(size_t requested_size,
                                                size_t usable_size) {
    if (g_sampling_interval == 0) {
        return requested_size;
    }

    double p = -expm1(-(double)usable_size / (double)g_sampling_interval);
    if (p <= 0.0) {
        return requested_size;
    }

    return (uint64_t)((double)requested_size / p + 0.5);
}

/* ---------------------------------------------------------------------------
 * Hook callbacks
 * --------------------------------------------------------------------------- */

/*
 * Called by jemalloc when an allocation is selected as a sample.
 *
 * Signature matches jemalloc's prof_sample_hook_t:
 *   void (*)(const void *ptr, size_t size, void **bt, unsigned bt_len, size_t usize)
 */
static void otel_jemalloc_prof_sample(const void *ptr, size_t size,
                                      void **backtrace,
                                      unsigned backtrace_length,
                                      size_t usize) {
    (void)backtrace;
    (void)backtrace_length;

    uint64_t weighted = calculate_weighted_bytes(size, usize);
    unsigned count = atomic_fetch_add_explicit(
        &g_alloc_debug_count, 1, memory_order_relaxed);
    if (count < 8) {
        debugf("observed-jemalloc: alloc hook #%u ptr=%p requested=%zu "
               "usable=%zu weighted=%llu\n",
               count + 1, ptr, size, usize, (unsigned long long)weighted);
    }
    otel_probe_alloc((void *)ptr, (uint64_t)size, weighted);
}

/*
 * Called by jemalloc when a previously-sampled allocation is freed.
 *
 * Signature matches jemalloc's prof_sample_free_hook_t:
 *   void (*)(const void *ptr, size_t usize)
 */
static void otel_jemalloc_prof_sample_free(const void *ptr, size_t usize) {
    unsigned count = atomic_fetch_add_explicit(
        &g_free_debug_count, 1, memory_order_relaxed);
    if (count < 8) {
        debugf("observed-jemalloc: free hook #%u ptr=%p usable=%zu\n",
               count + 1, ptr, usize);
    }
    otel_probe_free((void *)ptr);
}

/*
 * No-op backtrace hook.  Prevents jemalloc from performing in-process stack
 * unwinding on sampled allocations.  The eBPF profiler captures stacks
 * externally when the USDT fires.
 *
 * Signature matches jemalloc's prof_backtrace_hook_t:
 *   void (*)(void **frames, unsigned *length, unsigned max_length)
 */
static void otel_jemalloc_noop_backtrace(void **frames, unsigned *length,
                                         unsigned max_length) {
    (void)frames;
    (void)max_length;
    *length = 0;
}

/* ---------------------------------------------------------------------------
 * Constructor: installs hooks and activates profiling
 * ---------------------------------------------------------------------------
 * Runs before main().  jemalloc is compiled with:
 *   prof:true,prof_active:false
 *
 * Sequence:
 *   1. Install no-op backtrace hook.
 *   2. Install alloc sample hook.
 *   3. Install free sample hook.
 *   4. Cache the sampling interval.
 *   5. Set prof.active = true.
 */
__attribute__((constructor))
static void otel_jemalloc_init(void) {
    g_debug = getenv("OTEL_JEMALLOC_DEBUG") != NULL;
    debugf("observed-jemalloc: constructor entered, pid=%d\n", getpid());

    /* 1. No-op backtrace */
    typedef void (*prof_backtrace_hook_t)(void **, unsigned *, unsigned);
    prof_backtrace_hook_t bt_hook = otel_jemalloc_noop_backtrace;
    int err = mallctl("experimental.hooks.prof_backtrace",
                      NULL, NULL, &bt_hook, sizeof(bt_hook));
    debugf("observed-jemalloc: install backtrace hook=%p rc=%d\n",
           (void *)(uintptr_t)bt_hook, err);

    /* 2. Sample hook */
    typedef void (*prof_sample_hook_t)(const void *, size_t, void **,
                                       unsigned, size_t);
    prof_sample_hook_t sample_hook = otel_jemalloc_prof_sample;
    err = mallctl("experimental.hooks.prof_sample",
                  NULL, NULL, &sample_hook, sizeof(sample_hook));
    debugf("observed-jemalloc: install alloc hook=%p rc=%d\n",
           (void *)(uintptr_t)sample_hook, err);

    /* 3. Free hook */
    typedef void (*prof_sample_free_hook_t)(const void *, size_t);
    prof_sample_free_hook_t free_hook = otel_jemalloc_prof_sample_free;
    err = mallctl("experimental.hooks.prof_sample_free",
                  NULL, NULL, &free_hook, sizeof(free_hook));
    debugf("observed-jemalloc: install free hook=%p rc=%d\n",
           (void *)(uintptr_t)free_hook, err);

    /* 4. Cache sampling interval */
    ssize_t lg_prof_sample = 19; /* default */
    size_t len = sizeof(lg_prof_sample);
    err = mallctl("prof.lg_sample", &lg_prof_sample, &len, NULL, 0);
    g_sampling_interval = (uint64_t)1 << (unsigned)lg_prof_sample;
    debugf("observed-jemalloc: prof.lg_sample=%zd interval=%llu rc=%d\n",
           lg_prof_sample, (unsigned long long)g_sampling_interval, err);

    /* 5. Activate profiling */
    bool active = true;
    err = mallctl("prof.active", NULL, NULL, &active, sizeof(active));
    debugf("observed-jemalloc: set prof.active=true rc=%d\n", err);

    bool actual_active = false;
    len = sizeof(actual_active);
    err = mallctl("prof.active", &actual_active, &len, NULL, 0);
    debugf("observed-jemalloc: read prof.active=%d rc=%d\n",
           actual_active, err);
}
