# toolchain-k230-linux.cmake
# K230 littleコア (Linux / glibc) 向けクロスコンパイル設定
# Buildroot defconfig に準拠した静的リンク ELF を生成する

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR riscv64)

# SDK paths
file(REAL_PATH "${CMAKE_CURRENT_LIST_DIR}/../k230_sdk" SDK_ROOT)
set(TC_DIR ${SDK_ROOT}/toolchain/Xuantie-900-gcc-linux-5.10.4-glibc-x86_64-V2.6.0/bin)

# Toolchain
set(CMAKE_C_COMPILER   ${TC_DIR}/riscv64-unknown-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER ${TC_DIR}/riscv64-unknown-linux-gnu-g++)

# Architecture flags (RVV なし — Buildroot defconfig に準拠)
set(ARCH_FLAGS "-march=rv64imafdc -mabi=lp64d")

# Compile flags
set(CMAKE_C_FLAGS   "${ARCH_FLAGS} -Wall -O2 -g --static -DK230_LITTLECORE" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "${ARCH_FLAGS} -Wall -O2 -g --static -DK230_LITTLECORE" CACHE STRING "" FORCE)

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS "${ARCH_FLAGS} --static" CACHE STRING "" FORCE)

# Prevent CMake from searching host libraries
set(CMAKE_FIND_ROOT_PATH ${TC_DIR}/..)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
