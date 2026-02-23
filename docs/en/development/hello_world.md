# hello_world

This guide walks you through cross-compiling an application for the K230 bigcore (RT-Smart) and littlecore (Linux) using a CMake out-of-tree build. The same source code produces ELFs for both cores — just switch the toolchain file.

## Prerequisites

- K230 SDK must be built (toolchain extracted)
- SDK placed at `k230_sdk/` in the repository root
- Host OS: x86_64 Linux
- CMake 3.16 or later

!!! note "Building the SDK"
    For K230 SDK build instructions, see [SDK Build](sdk_build.md).

## Toolchain Information

=== "bigcore (RT-Smart)"

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

=== "littlecore (Linux)"

    | Item | Value |
    |------|-------|
    | Toolchain | riscv64-unknown-linux-gnu-gcc (Xuantie GCC 10.4) |
    | Architecture | rv64imafdc |
    | ABI | lp64d |
    | Code model | medlow (default) |
    | Compiler flags | `-march=rv64imafdc -mabi=lp64d` |
    | libc | glibc (statically linked) |

## Build Steps (CMake Out-of-Tree Build)

Build your project independently without modifying the SDK source tree.

### 1. Source code

The source code is included in the repository. The `#ifdef` directives switch the output message depending on the target core.

**`apps/hello_world/src/hello.c`**:

```c
#include <stdio.h>

int main(void)
{
#if defined(K230_BIGCORE)
    printf("Hello, K230 bigcore!\n");
#elif defined(K230_LITTLECORE)
    printf("Hello, K230 littlecore!\n");
#else
    printf("Hello, K230!\n");
#endif
    return 0;
}
```

### 2. CMakeLists.txt

The CMake project configuration. `RTT_INCLUDE` is only defined by the bigcore toolchain file, so it is automatically skipped for littlecore builds.

**`apps/hello_world/CMakeLists.txt`**:

```cmake
cmake_minimum_required(VERSION 3.16)
project(hello_world C)

add_executable(hello_world src/hello.c)

# RT-Smart SDK includes (set by toolchain file, absent for littlecore)
if(RTT_INCLUDE)
    target_include_directories(hello_world PRIVATE ${RTT_INCLUDE})
endif()
```

### 3. Toolchain file

=== "bigcore (RT-Smart)"

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
    set(CMAKE_C_FLAGS   "${ARCH_FLAGS} -Werror -Wall -O0 -g -gdwarf-2 -n --static -DHAVE_CCONFIG_H -DK230_BIGCORE" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS "${ARCH_FLAGS} -Werror -Wall -O0 -g -gdwarf-2 -n --static -DHAVE_CCONFIG_H -DK230_BIGCORE" CACHE STRING "" FORCE)

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
    set(CMAKE_EXE_LINKER_FLAGS
        "${ARCH_FLAGS} -n --static -T${LINK_SCRIPT} -L${RTT_LIB_DIR} -L${SDK_LIB_DIR} -Wl,--whole-archive -lrtthread -Wl,--no-whole-archive -Wl,--start-group -lrtthread -Wl,--end-group"
        CACHE STRING "" FORCE)

    # Prevent CMake from searching host libraries
    set(CMAKE_FIND_ROOT_PATH ${TC_DIR}/..)
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    ```

=== "littlecore (Linux)"

    Cross-compilation settings for the K230 littlecore. No RT-Smart specific settings (RTT headers, linker script, librtthread) are needed.

    **`cmake/toolchain-k230-linux.cmake`**:

    ```cmake
    # toolchain-k230-linux.cmake
    # Cross-compilation settings for K230 littlecore (Linux / glibc)
    # Produces a statically linked ELF following the Buildroot defconfig

    set(CMAKE_SYSTEM_NAME Linux)
    set(CMAKE_SYSTEM_PROCESSOR riscv64)

    # SDK paths
    file(REAL_PATH "${CMAKE_CURRENT_LIST_DIR}/../k230_sdk" SDK_ROOT)
    set(TC_DIR ${SDK_ROOT}/toolchain/Xuantie-900-gcc-linux-5.10.4-glibc-x86_64-V2.6.0/bin)

    # Toolchain
    set(CMAKE_C_COMPILER   ${TC_DIR}/riscv64-unknown-linux-gnu-gcc)
    set(CMAKE_CXX_COMPILER ${TC_DIR}/riscv64-unknown-linux-gnu-g++)

    # Architecture flags (no RVV — follows Buildroot defconfig)
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
    ```

### 4. Run the build

Run from the repository root directory.

=== "bigcore (RT-Smart)"

    ```bash
    cmake -B build/hello_world -S apps/hello_world \
      -DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/toolchain-k230-rtsmart.cmake"
    cmake --build build/hello_world
    ```

=== "littlecore (Linux)"

    ```bash
    cmake -B build/hello_world_linux -S apps/hello_world \
      -DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/toolchain-k230-linux.cmake"
    cmake --build build/hello_world_linux
    ```

### 5. Verify the generated ELF

=== "bigcore (RT-Smart)"

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

=== "littlecore (Linux)"

    ```bash
    file build/hello_world_linux/hello_world
    ```

    Expected output:

    ```
    hello_world: ELF 64-bit LSB executable, UCB RISC-V, double-float ABI, version 1 (SYSV), statically linked, ... not stripped
    ```

    !!! note "Key flags (set automatically by the toolchain file)"
        - Architecture: `-march=rv64imafdc -mabi=lp64d`
        - Static linking: `--static`
        - glibc based

## Transferring and Running on K230

=== "bigcore (RT-Smart)"

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

=== "littlecore (Linux)"

    ### Transfer via SCP

    If K230 is connected to your network, transfer the ELF to `/root/` using `scp`:

    ```bash
    scp build/hello_world_linux/hello_world root@<K230_IP_ADDRESS>:/root/hello_world
    ```

    ### Run on the K230 littlecore (Linux shell)

    On the K230 serial console (ACM0), run the binary from the Linux shell:

    ```
    [root@canmv ~]# /root/hello_world
    Hello, K230 littlecore!
    ```

    !!! tip "Serial connection"
        - **Smallcore (Linux)**: `/dev/ttyACM0` at 115200 bps
        - **Bigcore (RT-Smart msh)**: `/dev/ttyACM1` at 115200 bps

        ```bash
        minicom -D /dev/ttyACM0 -b 115200
        ```
