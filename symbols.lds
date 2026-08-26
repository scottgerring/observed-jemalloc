/*
 * Linker version script for observed-jemalloc.
 *
 * Exports standard allocator symbols and jemalloc extensions without assigning
 * them a custom symbol version. This is required for LD_PRELOAD interposition:
 * glibc-linked applications request malloc/free using GLIBC symbol versions,
 * and a custom JEMALLOC version would not satisfy those references.
 * All adapter internals (otel_*, hooks, etc.) are hidden.
 */
{
  global:
    /* POSIX / C standard */
    malloc;
    free;
    calloc;
    realloc;
    posix_memalign;
    aligned_alloc;
    malloc_usable_size;
    memalign;
    valloc;
    pvalloc;

    /* GNU extensions */
    mallopt;
    mallinfo;
    mallinfo2;
    malloc_info;
    malloc_stats;
    malloc_trim;

    /* jemalloc non-standard API */
    mallocx;
    dallocx;
    rallocx;
    sallocx;
    nallocx;
    sdallocx;
    xallocx;
    mallctl;
    mallctlnametomib;
    mallctlbymib;
    malloc_stats_print;
    malloc_message;

    /* jemalloc configuration */
    je_malloc_conf;

  local:
    *;
};
