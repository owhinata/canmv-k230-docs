# bigコア Hello World ビルド

K230 bigコア（RT-Smart / RISC-V rv64）向けアプリケーションを、CMake out-of-tree ビルドでクロスコンパイルして ELF を生成する手順です。実機（bigコア `/dev/ttyACM1`）での動作を確認しています。

## 前提条件

- K230 SDK がビルド済みであること（ツールチェーンが展開済み）
- SDK がリポジトリの `k230_sdk/` に配置されていること
- ホスト OS: x86_64 Linux
- CMake 3.16 以上

!!! note "SDK ビルドについて"
    K230 SDK のビルド手順は [SDK ビルド](sdk_build.md) を参照してください。

## ツールチェーン情報

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
| 環境設定スクリプト | `src/big/rt-smart/smart-env.sh` |

## ビルド手順（CMake out-of-tree ビルド）

SDK のソースツリーを変更せず、独立したプロジェクトとしてビルドします。

### 必要な SDK ファイル（参照のみ、コピー不要）

| 種別 | SDK パス |
|------|----------|
| ツールチェーン | `toolchain/riscv64-linux-musleabi_for_x86_64-pc-linux-gnu/` |
| RTT ヘッダ | `src/big/rt-smart/userapps/sdk/rt-thread/include/` |
| RTT ライブラリ | `src/big/rt-smart/userapps/sdk/rt-thread/lib/risc-v/rv64/librtthread.a` |
| リンカスクリプト | `src/big/rt-smart/userapps/linker_scripts/riscv64/link.lds` |

### 1. ソースコード

ソースコードはリポジトリに含まれています。

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

プロジェクトの CMake 設定です。

**`apps/hello_world/CMakeLists.txt`**:

```cmake
cmake_minimum_required(VERSION 3.16)
project(hello_world C)

add_executable(hello_world src/hello.c)

# RT-Smart SDK includes (set by toolchain file)
target_include_directories(hello_world PRIVATE ${RTT_INCLUDE})
```

### 3. ツールチェーンファイル

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

### 4. ビルド実行

リポジトリのルートディレクトリから実行します:

```bash
cmake -B build/hello_world -S apps/hello_world \
  -DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/toolchain-k230-rtsmart.cmake"
cmake --build build/hello_world
```

### 5. 生成された ELF を確認

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

## K230 への転送と実行

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
