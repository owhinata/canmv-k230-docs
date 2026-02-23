# hello_world

K230 の bigコア（RT-Smart）および littleコア（Linux）向けアプリケーションを、CMake out-of-tree ビルドでクロスコンパイルして ELF を生成する手順です。同じソースコードから、ツールチェーンファイルの切り替えだけで両コアの ELF を生成できます。

## 前提条件

- K230 SDK がビルド済みであること（ツールチェーンが展開済み）
- SDK がリポジトリの `k230_sdk/` に配置されていること
- ホスト OS: x86_64 Linux
- CMake 3.16 以上

!!! note "SDK ビルドについて"
    K230 SDK のビルド手順は [SDK ビルド](sdk_build.md) を参照してください。

## ツールチェーン情報

=== "bigcore (RT-Smart)"

    | 項目 | 値 |
    |------|-----|
    | ツールチェーン | riscv64-unknown-linux-musl-gcc (GCC 12.0.1) |
    | アーキテクチャ | rv64imafdcv |
    | ABI | lp64d |
    | コードモデル | medany |
    | コンパイラフラグ | `-march=rv64imafdcv -mabi=lp64d -mcmodel=medany` |
    | RT-Smart SDK ヘッダ | `src/big/rt-smart/userapps/sdk/rt-thread/include/` |
    | リンカスクリプト | `src/big/rt-smart/userapps/linker_scripts/riscv64/link.lds` (entry: 0x200000000) |
    | librtthread.a | `src/big/rt-smart/userapps/sdk/rt-thread/lib/risc-v/rv64/` |

=== "littlecore (Linux)"

    | 項目 | 値 |
    |------|-----|
    | ツールチェーン | riscv64-unknown-linux-gnu-gcc (Xuantie GCC 10.4) |
    | アーキテクチャ | rv64imafdc |
    | ABI | lp64d |
    | コードモデル | medlow（デフォルト） |
    | コンパイラフラグ | `-march=rv64imafdc -mabi=lp64d` |
    | libc | glibc（静的リンク） |

## ビルド手順（CMake out-of-tree ビルド）

SDK のソースツリーを変更せず、独立したプロジェクトとしてビルドします。

### 1. ソースコード

ソースコードはリポジトリに含まれています。`#ifdef` によりビルド対象コアに応じてメッセージが切り替わります。

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

プロジェクトの CMake 設定です。`RTT_INCLUDE` は bigcore ツールチェーンファイルのみが定義するため、littlecore ビルドでは自動的にスキップされます。

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

### 3. ツールチェーンファイル

=== "bigcore (RT-Smart)"

    K230 bigコア向けクロスコンパイル設定です。SDK パスはリポジトリのルートからの相対パスで自動解決されます。

    **`cmake/toolchain-k230-rtsmart.cmake`**:

    ```cmake
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

    K230 littleコア向けクロスコンパイル設定です。RT-Smart 固有の設定（RTT ヘッダ、リンカスクリプト、librtthread）は不要です。

    **`cmake/toolchain-k230-linux.cmake`**:

    ```cmake
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
    ```

### 4. ビルド実行

リポジトリのルートディレクトリから実行します。

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

### 5. 生成された ELF を確認

=== "bigcore (RT-Smart)"

    ```bash
    file build/hello_world/hello_world
    ```

    期待される出力:

    ```
    hello_world: ELF 64-bit LSB executable, UCB RISC-V, RVC, double-float ABI, version 1 (SYSV), statically linked, not stripped
    ```

    !!! note "主要フラグ（toolchain ファイルが自動設定）"
        - アーキテクチャ: `-march=rv64imafdcv -mabi=lp64d -mcmodel=medany`
        - 静的リンク: `-n --static`
        - RTT シンボル全包含: `-Wl,--whole-archive -lrtthread -Wl,--no-whole-archive`

=== "littlecore (Linux)"

    ```bash
    file build/hello_world_linux/hello_world
    ```

    期待される出力:

    ```
    hello_world: ELF 64-bit LSB executable, UCB RISC-V, double-float ABI, version 1 (SYSV), statically linked, ... not stripped
    ```

    !!! note "主要フラグ（toolchain ファイルが自動設定）"
        - アーキテクチャ: `-march=rv64imafdc -mabi=lp64d`
        - 静的リンク: `--static`
        - glibc ベース

## K230 への転送と実行

=== "bigcore (RT-Smart)"

    ### SCP で転送する

    K230 がネットワークに接続されている場合は `scp` で `/sharefs/` に転送します。

    ```bash
    scp build/hello_world/hello_world root@<K230のIPアドレス>:/sharefs/hello_world
    ```

    !!! warning "/sharefs/ について"
        転送先は `/sharefs/hello_world` です（`/root/sharefs/` ではありません）。
        `/sharefs/` は bigコアから直接アクセスできる vfat パーティション（`/dev/mmcblk1p4`）で、
        K230 の Linux 側からも同じパスでアクセスできます。
        `/root/sharefs/` は別ディレクトリであり、bigコアからはアクセスできません。

    ### K230 bigコア（msh）で実行

    K230 シリアルコンソール（ACM1）から bigコアの msh プロンプトで実行します:

    ```
    msh /> /sharefs/hello_world
    Hello, K230 bigcore!
    ```

    !!! tip "シリアル接続"
        - **小コア (Linux)**: `/dev/ttyACM0`（115200 bps）
        - **bigコア (RT-Smart msh)**: `/dev/ttyACM1`（115200 bps）

        ```bash
        minicom -D /dev/ttyACM1 -b 115200
        ```

=== "littlecore (Linux)"

    ### SCP で転送する

    K230 がネットワークに接続されている場合は `scp` で `/root/` に転送します。

    ```bash
    scp build/hello_world_linux/hello_world root@<K230のIPアドレス>:/root/hello_world
    ```

    ### K230 littleコア（Linux シェル）で実行

    K230 シリアルコンソール（ACM0）から Linux シェルで実行します:

    ```
    [root@canmv ~]# /root/hello_world
    Hello, K230 littlecore!
    ```

    !!! tip "シリアル接続"
        - **小コア (Linux)**: `/dev/ttyACM0`（115200 bps）
        - **bigコア (RT-Smart msh)**: `/dev/ttyACM1`（115200 bps）

        ```bash
        minicom -D /dev/ttyACM0 -b 115200
        ```
