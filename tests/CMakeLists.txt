# Tests for observed-jemalloc

# ---------------------------------------------------------------------------
# Smoke test: allocate, free, verify hooks are installed
# ---------------------------------------------------------------------------
add_executable(smoke smoke.c)
target_link_libraries(smoke PRIVATE jemalloc m pthread dl)
add_test(NAME smoke COMMAND smoke)

# ---------------------------------------------------------------------------
# Preload test: verify an ordinary glibc-linked binary is interposed
# ---------------------------------------------------------------------------
add_executable(preload_test preload.c)
target_link_libraries(preload_test PRIVATE dl)
add_test(
    NAME preload
    COMMAND ${CMAKE_COMMAND} -E env
        LD_PRELOAD=$<TARGET_FILE:jemalloc>
        $<TARGET_FILE:preload_test>
)

# ---------------------------------------------------------------------------
# Weight calculation test
# ---------------------------------------------------------------------------
add_executable(weight_test weight_test.c)
target_include_directories(weight_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(weight_test PRIVATE m)
add_test(NAME weight_test COMMAND weight_test)

# ---------------------------------------------------------------------------
# USDT note presence check
# ---------------------------------------------------------------------------
add_test(
    NAME check_usdt_notes
    COMMAND ${CMAKE_SOURCE_DIR}/tests/check_usdt_notes.sh $<TARGET_FILE:jemalloc>
)
