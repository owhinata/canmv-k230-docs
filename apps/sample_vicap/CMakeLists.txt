cmake_minimum_required(VERSION 3.16)
project(sample_vicap C)

# Source files (copied from SDK sample_vicap + sample_vo)
add_executable(sample_vicap
    src/sample_vicap.c
    src/vo_test_case.c
)

# SDK sample code has warnings — disable -Werror for this target
target_compile_options(sample_vicap PRIVATE -Wno-error)

# RT-Smart SDK includes (set by toolchain file)
if(RTT_INCLUDE)
    target_include_directories(sample_vicap PRIVATE ${RTT_INCLUDE})
endif()

# MPP SDK paths (set by toolchain file)
if(NOT MPP_LIB_DIR)
    message(FATAL_ERROR "MPP_LIB_DIR not set. Use the bigcore toolchain file.")
endif()

# MPP include paths
file(REAL_PATH "${CMAKE_CURRENT_LIST_DIR}/../../k230_sdk" _SDK_ROOT)
set(_MPP_ROOT ${_SDK_ROOT}/src/big/mpp)
target_include_directories(sample_vicap PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src          # local headers (vo_test_case.h)
    ${_MPP_ROOT}/include                     # k_module.h, k_type.h, k_errno.h
    ${_MPP_ROOT}/include/comm                # k_vicap_comm.h, k_vo_comm.h, etc.
    ${_MPP_ROOT}/include/ioctl               # k_connector_ioctl.h, etc.
    ${_MPP_ROOT}/userapps/api                # mpi_*_api.h
)

# Link all MPP libraries (matches SDK mpp.mk wildcard behavior).
# The linker only pulls in .o files it actually needs, so unused
# archives do not affect binary size.
file(GLOB _MPP_LIBS "${MPP_LIB_DIR}/lib*.a")
list(TRANSFORM _MPP_LIBS REPLACE ".*/lib([^/]*)\\.a$" "-l\\1")
target_link_libraries(sample_vicap PRIVATE
    -L${MPP_LIB_DIR} -Wl,--start-group ${_MPP_LIBS} -Wl,--end-group
)
