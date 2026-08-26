cmake_minimum_required(VERSION 3.20)
project(observed-jemalloc VERSION 5.3.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
set(JEMALLOC_VERSION "5.3.1" CACHE STRING "jemalloc release version")
set(JEMALLOC_SHA256 "3826bc80232f22ed5c4662f3034f799ca316e819103bdc7bb99018a421706f92"
    CACHE STRING "SHA-256 of jemalloc release tarball")
set(JEMALLOC_LG_PROF_SAMPLE "19" CACHE STRING
    "Base-2 log of mean sampling interval in bytes (default: 19 = 512 KiB)")

# ---------------------------------------------------------------------------
# Fetch and build jemalloc
# ---------------------------------------------------------------------------
include(cmake/FetchJemalloc.cmake)

# ---------------------------------------------------------------------------
# Adapter library
# ---------------------------------------------------------------------------
add_library(otel_jemalloc_adapter OBJECT
    src/adapter.c
    src/otel_probes.c
)
target_include_directories(otel_jemalloc_adapter PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${JEMALLOC_INCLUDE_DIR}
)
add_dependencies(otel_jemalloc_adapter jemalloc_build)

# ---------------------------------------------------------------------------
# Final shared library
# ---------------------------------------------------------------------------
# Link jemalloc's PIC archive together with our adapter into one .so.
add_library(jemalloc SHARED
    $<TARGET_OBJECTS:otel_jemalloc_adapter>
)

target_link_libraries(jemalloc PRIVATE
    ${JEMALLOC_PIC_LIBRARY}
    m      # for expm1
    dl
    pthread
)

set_target_properties(jemalloc PROPERTIES
    OUTPUT_NAME "jemalloc"
    VERSION "${PROJECT_VERSION}"
    SOVERSION "2"
    LINK_FLAGS "-Wl,--version-script=${CMAKE_SOURCE_DIR}/symbols.lds"
    LINK_DEPENDS "${CMAKE_SOURCE_DIR}/symbols.lds"
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)
