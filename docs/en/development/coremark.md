# CoreMark Benchmark

[CoreMark](https://github.com/eembc/coremark) is a CPU benchmark provided by EEMBC. `apps/coremark/` uses CMake FetchContent to automatically fetch the upstream source and builds for both Big/Little cores with **no custom source files** — only a CMakeLists.txt.

## Prerequisites

- K230 SDK must be built (toolchain extracted)
- SDK placed at `k230_sdk/` in the repository root
- Host OS: x86_64 Linux
- CMake 3.16 or later
- Internet connection (only for the initial FetchContent download)

!!! note "Building the SDK"
    For K230 SDK build instructions, see [SDK Build](sdk_build.md).

## Design

### Why the posix/ port

The upstream CoreMark `posix/` port operates with the following settings:

- `USE_CLOCK=0`, `HAS_TIME_H=1` → uses `clock_gettime(CLOCK_REALTIME)` for timing
- `SEED_METHOD=SEED_ARG` → seed configurable via command-line arguments (auto-detect when no arguments)
- `MEM_METHOD=MEM_MALLOC` → standard heap allocation

This is the same approach used by the K230 SDK's CoreMark implementation (`k230_sdk/src/big/unittest/testcases/coremark/`). It works with both musl (RT-Smart) and glibc (Linux).

### Why no custom source files are needed

- `CORETIMETYPE` is only used within `core_portme.c` (`core_main.c` uses `CORE_TICKS`=`clock_t`)
- The posix port's `core_portme.c` is nearly identical to the K230 SDK implementation
- Configuration differences (ITERATIONS, etc.) can be handled via runtime arguments or CMake `-D`
- Only `FLAGS_STR` needs to be defined from CMake (upstream Makefile also passes it via `-D`)

### -O2 override

The RT-Smart toolchain sets `-O0`, which is meaningless for benchmarking. Since GCC uses the last `-O` flag, appending `-O2` after the toolchain's `-O0` effectively enables `-O2`. The Linux toolchain already uses `-O2`, so this has no effect there.

### Timing accuracy

RT-Smart's `clock_gettime(CLOCK_REALTIME)` is tick-based (`RT_TICK_PER_SECOND=1000`) with a **resolution of 1ms**. Since CoreMark runs for approximately 10 seconds, the measurement error at 1ms resolution is about 0.01% — more than sufficient.

`-lrt` is not needed — both musl and modern glibc include `clock_gettime` in libc.

## CMakeLists.txt

**`apps/coremark/CMakeLists.txt`**:

```cmake
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
```

FetchContent automatically downloads the CoreMark source at build time, so no manual source placement is needed. It builds the 5 core files plus `posix/core_portme.c` from the sources extracted into `coremark_src_SOURCE_DIR`.

## Build Steps

Run from the repository root directory.

=== "bigcore (RT-Smart)"

    ```bash
    cmake -B build/coremark -S apps/coremark \
      -DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/toolchain-k230-rtsmart.cmake"
    cmake --build build/coremark
    ```

=== "littlecore (Linux)"

    ```bash
    cmake -B build/coremark_linux -S apps/coremark \
      -DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/toolchain-k230-linux.cmake"
    cmake --build build/coremark_linux
    ```

### Verify the generated ELF

=== "bigcore (RT-Smart)"

    ```bash
    file build/coremark/coremark
    ```

    Expected output:

    ```
    coremark: ELF 64-bit LSB executable, UCB RISC-V, RVC, double-float ABI, version 1 (SYSV), statically linked, ...
    ```

=== "littlecore (Linux)"

    ```bash
    file build/coremark_linux/coremark
    ```

    Expected output:

    ```
    coremark: ELF 64-bit LSB executable, UCB RISC-V, RVC, double-float ABI, version 1 (SYSV), statically linked, ...
    ```

## Transferring and Running on K230

=== "bigcore (RT-Smart)"

    ### Transfer via SCP

    ```bash
    scp build/coremark/coremark root@<K230_IP_ADDRESS>:/sharefs/coremark
    ```

    !!! warning "About /sharefs/"
        The correct destination is `/sharefs/coremark`, **not** `/root/sharefs/coremark`.
        `/sharefs/` is a vfat partition (`/dev/mmcblk1p4`) directly accessible from the bigcore.
        It is also accessible from the Linux smallcore under the same path.

    ### Run on the K230 bigcore (msh)

    ```
    msh /> /sharefs/coremark
    2K performance run parameters for coremark.
    CoreMark Size    : 666
    ...
    CoreMark 1.0 : xxx.xx / GCC12.0.1 ... / STACK
    ```

    !!! tip "Serial connection"
        - **Smallcore (Linux)**: `/dev/ttyACM0` at 115200 bps
        - **Bigcore (RT-Smart msh)**: `/dev/ttyACM1` at 115200 bps

        ```bash
        minicom -D /dev/ttyACM1 -b 115200
        ```

=== "littlecore (Linux)"

    ### Transfer via SCP

    ```bash
    scp build/coremark_linux/coremark root@<K230_IP_ADDRESS>:/root/coremark
    ```

    ### Run on the K230 littlecore (Linux shell)

    ```
    [root@canmv ~]# /root/coremark
    2K performance run parameters for coremark.
    CoreMark Size    : 666
    ...
    CoreMark 1.0 : xxx.xx / GCC10.2.0 ... / STACK
    ```

    !!! tip "Serial connection"
        - **Smallcore (Linux)**: `/dev/ttyACM0` at 115200 bps
        - **Bigcore (RT-Smart msh)**: `/dev/ttyACM1` at 115200 bps

        ```bash
        minicom -D /dev/ttyACM0 -b 115200
        ```

## Runtime Options

CoreMark runs with default settings (auto-detect ITERATIONS, auto-select SEED) when executed without arguments.

```
coremark <seed1> <seed2> <seed3> <iterations>
```

| Argument | Description | Default |
|----------|-------------|---------|
| seed1, seed2, seed3 | Benchmark input seeds | Auto-selected (validation/performance/profile) |
| iterations | Number of iterations | Auto (minimum value for ~10 seconds execution) |

Example: run with 50000 iterations:

```bash
/sharefs/coremark 0 0 0x66 50000
```

## Future Extensions

### Multi-threaded support

To run CoreMark in multi-threaded mode:

1. Copy the upstream `posix/core_portme.c` to `apps/coremark/src/core_portme.c`
2. Change the source path in CMakeLists.txt to `src/core_portme.c`
3. Add `-DMULTITHREAD=4 -DUSE_PTHREAD=1` in CMake and add `pthread` to `target_link_libraries`
