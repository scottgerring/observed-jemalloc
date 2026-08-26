# FetchJemalloc.cmake
#
# Downloads, verifies, configures, and builds jemalloc using its native
# autotools build system.  Produces libjemalloc_pic.a for linking into
# the final shared object.

include(ExternalProject)
include(FetchContent)

set(JEMALLOC_URL
    "https://github.com/jemalloc/jemalloc/releases/download/${JEMALLOC_VERSION}/jemalloc-${JEMALLOC_VERSION}.tar.bz2")

set(JEMALLOC_PREFIX "${CMAKE_BINARY_DIR}/jemalloc")
set(JEMALLOC_SOURCE_DIR "${JEMALLOC_PREFIX}/src/jemalloc_build")
set(JEMALLOC_INSTALL_DIR "${JEMALLOC_PREFIX}")

ExternalProject_Add(jemalloc_build
    URL "${JEMALLOC_URL}"
    URL_HASH SHA256=${JEMALLOC_SHA256}
    PREFIX "${JEMALLOC_PREFIX}"
    INSTALL_DIR "${JEMALLOC_INSTALL_DIR}"
    CONFIGURE_COMMAND
        <SOURCE_DIR>/configure
            --prefix=<INSTALL_DIR>
            --enable-prof
            --disable-shared
            --enable-static
            --with-jemalloc-prefix=
            --disable-cxx
            "--with-malloc-conf=prof:true,prof_active:false,lg_prof_sample:${JEMALLOC_LG_PROF_SAMPLE}"
    BUILD_COMMAND make -j$ENV{NPROC} build_lib_static
    INSTALL_COMMAND make install_lib_static install_include
    BUILD_IN_SOURCE TRUE
)

# Paths to consume after jemalloc_build completes
set(JEMALLOC_PIC_LIBRARY
    "${JEMALLOC_INSTALL_DIR}/lib/libjemalloc_pic.a")
set(JEMALLOC_INCLUDE_DIR
    "${JEMALLOC_INSTALL_DIR}/include")

# Note: the caller (CMakeLists.txt) must add_dependencies on jemalloc_build
# after defining the adapter target.
