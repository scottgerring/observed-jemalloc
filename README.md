# observed-jemalloc

A drop-in `libjemalloc.so` that emits OTel heap-profiling USDTs when jemalloc samples allocations. Designed for `LD_PRELOAD` use with an external eBPF profiler. No application code changes are required for dynamically linked applications that use the public allocator API.

Linux only (glibc, aarch64 and x86_64). Static executables and programs that bypass public allocator symbols cannot be interposed with `LD_PRELOAD`.

## Usage

```bash
LD_PRELOAD=/path/to/libjemalloc.so.2 ./your-application
```

Profiling is active immediately.

## Configuration

Sampling frequency is controlled by `lg_prof_sample`, the base-2 logarithm of the mean number of bytes between samples. The default is `19` (one sample per ~512 KiB allocated).

```bash
# Sample more frequently (one per ~128 KiB)
MALLOC_CONF=lg_prof_sample:17 LD_PRELOAD=./libjemalloc.so.2 ./app

# Sample less frequently (one per ~4 MiB)
MALLOC_CONF=lg_prof_sample:22 LD_PRELOAD=./libjemalloc.so.2 ./app
```

Set `OTEL_JEMALLOC_DEBUG=1` to log hook setup and the first sampled allocations and frees.

## USDTs emitted

| Probe | Arguments |
|-------|-----------|
| `otel_memory:alloc` | `void *ptr, uint64_t size, uint64_t weighted_bytes` |
| `otel_memory:free`  | `void *ptr` |

- `ptr` - the pointer returned to (or freed by) the application.
- `size` - the number of bytes the application requested.
- `weighted_bytes` - an unbiased estimate of how many requested bytes this sample represents. In expectation, its sum recovers the true allocation volume.

## How it works

Stock jemalloc 5.3.1 is built with its built-in profiling subsystem enabled. A shared-object constructor installs hooks into jemalloc's sampling path before `main` runs:

- When jemalloc selects an allocation as a sample, `otel_memory:alloc` fires.
- When a previously-sampled allocation is freed, `otel_memory:free` fires.
- jemalloc's in-process stack unwinding is disabled; the eBPF profiler captures stacks externally when the USDT fires.

## Building

Requirements: CMake 3.20+, a C compiler, autoconf, `systemtap-sdt-dev` (for `<sys/sdt.h>`).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Output: `build/libjemalloc.so.2`.

## License

Apache-2.0
