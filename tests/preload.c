/*
 * Preload integration test for observed-jemalloc.
 *
 * This executable deliberately does not link against jemalloc. It verifies
 * that LD_PRELOAD interposes a normal glibc-versioned malloc reference and
 * that the observed-jemalloc constructor configured profiling and its hooks.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*mallctl_fn)(const char *name, void *oldp, size_t *oldlenp,
                          void *newp, size_t newlen);

static int check_owned_by_jemalloc(const char *name, void *symbol) {
    Dl_info info = {0};
    if (dladdr(symbol, &info) == 0 || info.dli_fname == NULL) {
        fprintf(stderr, "FAIL: could not resolve owner of %s=%p\n", name, symbol);
        return 1;
    }

    printf("%s=%p resolved to %s\n", name, symbol, info.dli_fname);
    if (strstr(info.dli_fname, "libjemalloc.so") == NULL) {
        fprintf(stderr, "FAIL: %s resolved outside libjemalloc\n", name);
        return 1;
    }
    return 0;
}

int main(void) {
    void *(*allocator)(size_t) = malloc;
    void (*deallocator)(void *) = free;

    if (check_owned_by_jemalloc("malloc", (void *)allocator) != 0 ||
        check_owned_by_jemalloc("free", (void *)deallocator) != 0) {
        return 1;
    }

    mallctl_fn mallctl = (mallctl_fn)dlsym(RTLD_DEFAULT, "mallctl");
    if (mallctl == NULL) {
        fprintf(stderr, "FAIL: mallctl is unavailable: %s\n", dlerror());
        return 1;
    }
    if (check_owned_by_jemalloc("mallctl", (void *)mallctl) != 0) {
        return 1;
    }

    bool active = false;
    size_t len = sizeof(active);
    int err = mallctl("prof.active", &active, &len, NULL, 0);
    if (err != 0 || !active) {
        fprintf(stderr, "FAIL: prof.active rc=%d value=%d\n", err, active);
        return 1;
    }

    void *hook = NULL;
    len = sizeof(hook);
    err = mallctl("experimental.hooks.prof_sample", &hook, &len, NULL, 0);
    if (err != 0 || hook == NULL) {
        fprintf(stderr, "FAIL: prof_sample hook rc=%d value=%p\n", err, hook);
        return 1;
    }

    hook = NULL;
    len = sizeof(hook);
    err = mallctl("experimental.hooks.prof_sample_free", &hook, &len, NULL, 0);
    if (err != 0 || hook == NULL) {
        fprintf(stderr, "FAIL: prof_sample_free hook rc=%d value=%p\n", err, hook);
        return 1;
    }

    volatile size_t allocation_size = 4096;
    void *allocation = allocator(allocation_size);
    if (allocation == NULL) {
        fputs("FAIL: allocation returned NULL\n", stderr);
        return 1;
    }
    memset(allocation, 0xab, allocation_size);
    volatile unsigned char observed = ((unsigned char *)allocation)[0];
    deallocator(allocation);
    if (observed != 0xab) {
        fputs("FAIL: allocated memory was not writable\n", stderr);
        return 1;
    }

    puts("PASS: LD_PRELOAD interposed glibc allocator references");
    return 0;
}
