/*
 * Smoke test for observed-jemalloc.
 *
 * Verifies:
 *   - malloc/free work through our library
 *   - prof.active is true (hooks activated by constructor)
 *   - prof_sample hook is installed (non-NULL)
 *
 * Run with MALLOC_CONF=lg_prof_sample:0 for deterministic sampling.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int mallctl(const char *name, void *oldp, size_t *oldlenp,
                   void *newp, size_t newlen);

int main(void) {
    /* Basic allocation works */
    void *p = malloc(128);
    assert(p != NULL);
    memset(p, 0xAB, 128);
    free(p);

    /* prof.active should be true (set by constructor) */
    bool active = false;
    size_t len = sizeof(active);
    int err = mallctl("prof.active", &active, &len, NULL, 0);
    assert(err == 0);
    assert(active == true);
    printf("prof.active = %d (OK)\n", (int)active);

    /* prof_sample hook should be non-NULL */
    void *hook = NULL;
    len = sizeof(hook);
    err = mallctl("experimental.hooks.prof_sample", &hook, &len, NULL, 0);
    assert(err == 0);
    assert(hook != NULL);
    printf("prof_sample hook = %p (OK)\n", hook);

    /* prof_sample_free hook should be non-NULL */
    hook = NULL;
    len = sizeof(hook);
    err = mallctl("experimental.hooks.prof_sample_free", &hook, &len, NULL, 0);
    assert(err == 0);
    assert(hook != NULL);
    printf("prof_sample_free hook = %p (OK)\n", hook);

    /* Allocate many times to trigger sampling */
    printf("Allocating 10000 times...\n");
    for (int i = 0; i < 10000; i++) {
        void *q = malloc(1024);
        assert(q != NULL);
        free(q);
    }

    printf("PASS\n");
    return 0;
}
