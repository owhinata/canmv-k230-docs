# bigcore Hello World Build

This guide walks you through cross-compiling an application for the K230 bigcore (RT-Smart / RISC-V rv64) using a CMake out-of-tree build. The procedure has been verified on real hardware (bigcore via `/dev/ttyACM1`).

## Prerequisites

- K230 SDK must be built (toolchain extracted)
- SDK placed at `k230_sdk/` in the repository root
- Host OS: x86_64 Linux
- CMake 3.16 or later

!!! note "Building the SDK"
    For K230 SDK build instructions, see [SDK Build](sdk_build.md).

## Toolchain Information

| Item | Value |
|------|-------|
| Toolchain | riscv64-unknown-linux-musl-gcc (GCC 12.0.1) |
| Architecture | rv64imafdcv |
| ABI | lp64d |
| Code model | medany |
| Compiler flags | `-march=rv64imafdcv -mabi=lp64d -mcmodel=medany` |
| RT-Smart SDK headers | `src/big/rt-smart/userapps/sdk/rt-thread/include/` |
| Linker script | `src/big/rt-smart/userapps/linker_scripts/riscv64/link.lds` (entry: 0x200000000) |
| librtthread.a | `src/big/rt-smart/userapps/sdk/rt-thread/lib/risc-v/rv64/` |
| Environment script | `src/big/rt-smart/smart-env.sh` |

## Build Steps (CMake Out-of-Tree Build)

Build your project independently without modifying the SDK source tree.

### Required SDK files (referenced, not copied)

| Type | SDK path |
|------|----------|
| Toolchain | `toolchain/riscv64-linux-musleabi_for_x86_64-pc-linux-gnu/` |
| RTT headers | `src/big/rt-smart/userapps/sdk/rt-thread/include/` |
| RTT library | `src/big/rt-smart/userapps/sdk/rt-thread/lib/risc-v/rv64/librtthread.a` |
| Linker script | `src/big/rt-smart/userapps/linker_scripts/riscv64/link.lds` |

### 1. Source code

The source code is included in the repository.

**`apps/hello_world/src/hello.c`**:

```c
#include <stdio.h>

int main(void)
{
    printf("Hello, K230 bigcore!\n");
    return 0;
}
```

### 2. CMakeLists.txt

The CMake project configuration.

**`apps/hello_world/CMakeLists.txt`**:

```cmake
cmake_minimum_required(VERSION 3.16)
project(hello_world C)

add_executable(hello_world src/hello.c)

# RT-Smart SDK includes (set by toolchain file)
target_include_directories(hello_world PRIVATE ${RTT_INCLUDE})
```

### 3. Toolchain file

Cross-compilation settings for the K230 bigcore. The SDK path is automatically resolved relative to the repository root.

**`cmake/toolchain-k230-rtsmart.cmake`**:

```cmake
# toolchain-k230-rtsmart.cmake
# Cross-compilation settings for K230 bigcore (RT-Smart)
# Produces an ELF equivalent to the SCons SDK official build

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
```

### 4. Run the build

Run from the repository root directory:

```bash
cmake -B build/hello_world -S apps/hello_world \
  -DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/toolchain-k230-rtsmart.cmake"
cmake --build build/hello_world
```

### 5. Verify the generated ELF

```bash
file build/hello_world/hello_world
```

Expected output:

```
hello_world: ELF 64-bit LSB executable, UCB RISC-V, RVC, double-float ABI, version 1 (SYSV), statically linked, not stripped
```

!!! note "Key flags (set automatically by the toolchain file)"
    - Architecture: `-march=rv64imafdcv -mabi=lp64d -mcmodel=medany`
    - Static linking: `-n --static`
    - Full RTT symbol inclusion: `-Wl,--whole-archive -lrtthread -Wl,--no-whole-archive`

## Transferring and Running on K230

### Transfer via SCP

If K230 is connected to your network, transfer the ELF to `/sharefs/` using `scp`:

```bash
scp build/hello_world/hello_world root@<K230_IP_ADDRESS>:/sharefs/hello_world
```

!!! warning "About /sharefs/"
    The correct destination is `/sharefs/hello_world`, **not** `/root/sharefs/hello_world`.
    `/sharefs/` is a vfat partition (`/dev/mmcblk1p4`) directly accessible from the bigcore.
    It is also accessible from the Linux smallcore under the same path.
    `/root/sharefs/` is a different directory and is **not** accessible from the bigcore.

### Run on the K230 bigcore (msh)

On the K230 serial console (ACM1), run the binary from the RT-Smart msh prompt:

```
msh /> /sharefs/hello_world
Hello, K230 bigcore!
```

!!! tip "Serial connection"
    - **Smallcore (Linux)**: `/dev/ttyACM0` at 115200 bps
    - **Bigcore (RT-Smart msh)**: `/dev/ttyACM1` at 115200 bps

    ```bash
    minicom -D /dev/ttyACM1 -b 115200
    ```
