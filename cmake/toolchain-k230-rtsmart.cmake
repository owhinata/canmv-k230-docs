# toolchain-k230-rtsmart.cmake
# K230 bigコア (RT-Smart) 向けクロスコンパイル設定
# SCons SDK正規ビルドと同等のELFを生成する

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR riscv64)

# SDK paths
file(REAL_PATH "${CMAKE_CURRENT_LIST_DIR}/../k230_sdk" SDK_ROOT)
set(TC_DIR ${SDK_ROOT}/toolchain/riscv64-linux-musleabi_for_x86_64-pc-linux-gnu/bin)
set(USERAPPS ${SDK_ROOT}/src/big/rt-smart/userapps)
set(RTT_SDK ${USERAPPS}/sdk)
set(MPP_SDK ${SDK_ROOT}/src/big/mpp/userapps)

# Toolchain
set(CMAKE_C_COMPILER   ${TC_DIR}/riscv64-unknown-linux-musl-gcc)
set(CMAKE_CXX_COMPILER ${TC_DIR}/riscv64-unknown-linux-musl-g++)

# Architecture flags (matches riscv64.py DEVICE)
set(ARCH_FLAGS "-mcmodel=medany -march=rv64imafdcv -mabi=lp64d")

# Compile flags: match SCons BuildApplication() output
# -n --static: NMAGIC + static linking (from riscv64.py STATIC_FLAGS)
# -DHAVE_CCONFIG_H: SDK convention for musl config detection
set(CMAKE_C_FLAGS   "${ARCH_FLAGS} -Werror -Wall -O0 -g -gdwarf-2 -n --static -DHAVE_CCONFIG_H" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "${ARCH_FLAGS} -Werror -Wall -O0 -g -gdwarf-2 -n --static -DHAVE_CCONFIG_H" CACHE STRING "" FORCE)

# RT-Smart SDK includes (matches SCons sdk/rt-thread/SConscript)
set(RTT_INCLUDE
    ${RTT_SDK}/rt-thread/include
    ${RTT_SDK}/rt-thread/components/dfs
    ${RTT_SDK}/rt-thread/components/drivers
    ${RTT_SDK}/rt-thread/components/finsh
    ${RTT_SDK}/rt-thread/components/net
    ${USERAPPS}
)

# Library paths
set(RTT_LIB_DIR ${RTT_SDK}/rt-thread/lib/risc-v/rv64)
set(SDK_LIB_DIR ${RTT_SDK}/lib/risc-v/rv64)
set(LINK_SCRIPT ${USERAPPS}/linker_scripts/riscv64/link.lds)

# MPP (for media apps)
set(MPP_INCLUDE ${MPP_SDK}/api)
set(MPP_LIB_DIR ${MPP_SDK}/lib)

# Linker flags: exact match of SCons BuildApplication() link command
# -n --static: NMAGIC static (riscv64.py STATIC_FLAGS)
# -T link.lds: RT-Smart user space linker script (entry 0x200000000)
# --whole-archive -lrtthread: include ALL symbols from librtthread.a
# --start-group / --end-group: resolve circular library dependencies
set(CMAKE_EXE_LINKER_FLAGS
    "${ARCH_FLAGS} -n --static -T${LINK_SCRIPT} -L${RTT_LIB_DIR} -L${SDK_LIB_DIR} -Wl,--whole-archive -lrtthread -Wl,--no-whole-archive -Wl,--start-group -lrtthread -Wl,--end-group"
    CACHE STRING "" FORCE)

# Prevent CMake from searching host libraries
set(CMAKE_FIND_ROOT_PATH ${TC_DIR}/..)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
