/*
 * Unit test for the Horvitz-Thompson weight calculation.
 *
 * Validates that weighted_bytes correctly estimates application-requested
 * bytes for various allocation sizes and sampling intervals.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Replicate the weight calculation from adapter.c so we can test it
 * in isolation without linking the full library.
 */
static uint64_t calculate_weighted_bytes(size_t requested_size,
                                         size_t usable_size,
                                         uint64_t sampling_interval) {
    if (sampling_interval == 0) {
        return requested_size;
    }

    double p = -expm1(-(double)usable_size / (double)sampling_interval);
    if (p <= 0.0) {
        return requested_size;
    }

    return (uint64_t)((double)requested_size / p + 0.5);
}

static void test_small_allocation(void) {
    /*
     * requested=17, usable=32, interval=512KiB
     * p = 1 - exp(-32/524288) ~= 0.0000610
     * weight ~= 17 / 0.0000610 ~= 278525
     */
    uint64_t interval = (uint64_t)1 << 19;
    uint64_t w = calculate_weighted_bytes(17, 32, interval);

    /* Verify it is in the right ballpark */
    assert(w > 200000);
    assert(w < 400000);

    /* Verify the estimator property: p * weight ~= requested */
    double p = -expm1(-32.0 / (double)interval);
    double estimated = p * (double)w;
    assert(fabs(estimated - 17.0) < 1.0);

    printf("  small alloc: requested=17, usable=32, weight=%lu (OK)\n",
           (unsigned long)w);
}

static void test_large_allocation(void) {
    /*
     * requested=1000000, usable=1048576, interval=512KiB
     * p = 1 - exp(-1048576/524288) = 1 - exp(-2) ~= 0.8647
     * weight ~= 1000000 / 0.8647 ~= 1156418
     */
    uint64_t interval = (uint64_t)1 << 19;
    uint64_t w = calculate_weighted_bytes(1000000, 1048576, interval);

    double p = -expm1(-1048576.0 / (double)interval);
    double estimated = p * (double)w;
    assert(fabs(estimated - 1000000.0) < 2.0);

    printf("  large alloc: requested=1000000, usable=1048576, weight=%lu (OK)\n",
           (unsigned long)w);
}

static void test_exact_interval(void) {
    /*
     * When usable_size == interval:
     * p = 1 - exp(-1) ~= 0.6321
     * weight = requested / 0.6321
     */
    uint64_t interval = 1024;
    uint64_t w = calculate_weighted_bytes(1000, 1024, interval);

    double p = -expm1(-1.0);
    double estimated = p * (double)w;
    assert(fabs(estimated - 1000.0) < 1.0);

    printf("  interval-sized: requested=1000, usable=1024, weight=%lu (OK)\n",
           (unsigned long)w);
}

static void test_very_large_relative(void) {
    /*
     * usable >> interval: p approaches 1.0, weight approaches requested.
     */
    uint64_t interval = 1024;
    uint64_t w = calculate_weighted_bytes(10000000, 10485760, interval);

    /* p is essentially 1.0, so weight should be very close to requested */
    assert(w >= 9999999 && w <= 10000001);

    printf("  very large: requested=10000000, usable=10485760, weight=%lu (OK)\n",
           (unsigned long)w);
}

static void test_zero_interval(void) {
    /* Edge case: interval=0 should return requested as-is */
    uint64_t w = calculate_weighted_bytes(42, 64, 0);
    assert(w == 42);
    printf("  zero interval: weight=%lu (OK)\n", (unsigned long)w);
}

static void test_small_interval(void) {
    /*
     * lg_prof_sample=0 means interval=1.
     * Every allocation is sampled. p ~= 1 - exp(-usable/1).
     * For any usable >= 1, p is very close to 1.
     * weight ~= requested.
     */
    uint64_t w = calculate_weighted_bytes(100, 128, 1);
    assert(w >= 99 && w <= 101);
    printf("  interval=1: requested=100, weight=%lu (OK)\n",
           (unsigned long)w);
}

int main(void) {
    printf("Weight calculation tests:\n");
    test_small_allocation();
    test_large_allocation();
    test_exact_interval();
    test_very_large_relative();
    test_zero_interval();
    test_small_interval();
    printf("ALL PASSED\n");
    return 0;
}
