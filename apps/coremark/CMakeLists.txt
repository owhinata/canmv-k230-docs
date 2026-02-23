cmake_minimum_required(VERSION 3.16)
project(coremark C)

# --- Fetch upstream CoreMark source ---
include(FetchContent)
FetchContent_Declare(
    coremark_src
    GIT_REPOSITORY https://github.com/eembc/coremark.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(coremark_src)
if(NOT coremark_src_POPULATED)
    FetchContent_Populate(coremark_src)
endif()

# --- Build the CoreMark benchmark ---
add_executable(coremark
    ${coremark_src_SOURCE_DIR}/core_list_join.c
    ${coremark_src_SOURCE_DIR}/core_main.c
    ${coremark_src_SOURCE_DIR}/core_matrix.c
    ${coremark_src_SOURCE_DIR}/core_state.c
    ${coremark_src_SOURCE_DIR}/core_util.c
    ${coremark_src_SOURCE_DIR}/posix/core_portme.c
)

target_include_directories(coremark PRIVATE
    ${coremark_src_SOURCE_DIR}
    ${coremark_src_SOURCE_DIR}/posix
)

# Benchmark-meaningful optimization
# (-O2 overrides RT-Smart toolchain's -O0; GCC uses the last -O flag)
target_compile_options(coremark PRIVATE -O2)

# Suppress -Werror from RT-Smart toolchain for upstream code
target_compile_options(coremark PRIVATE -Wno-error)

# FLAGS_STR: required by posix/core_portme.h for benchmark report output
target_compile_definitions(coremark PRIVATE "FLAGS_STR=\"-O2\"")

# RT-Smart SDK includes (set by bigcore toolchain, absent for littlecore)
if(RTT_INCLUDE)
    target_include_directories(coremark PRIVATE ${RTT_INCLUDE})
endif()
