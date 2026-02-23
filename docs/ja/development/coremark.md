# CoreMark ベンチマーク

[CoreMark](https://github.com/eembc/coremark) は EEMBC が提供する CPU ベンチマークです。`apps/coremark/` では CMake FetchContent で upstream ソースを自動取得し、**カスタムソースファイルなし**（CMakeLists.txt のみ）で Big/Little 両コア向けにビルドできます。

## 前提条件

- K230 SDK がビルド済みであること（ツールチェーンが展開済み）
- SDK がリポジトリの `k230_sdk/` に配置されていること
- ホスト OS: x86_64 Linux
- CMake 3.16 以上
- インターネット接続（初回の FetchContent 取得時のみ）

!!! note "SDK ビルドについて"
    K230 SDK のビルド手順は [SDK ビルド](sdk_build.md) を参照してください。

## 設計

### posix/ port 選択理由

CoreMark の upstream `posix/` port は以下の設定で動作します:

- `USE_CLOCK=0`, `HAS_TIME_H=1` → `clock_gettime(CLOCK_REALTIME)` でタイミング計測
- `SEED_METHOD=SEED_ARG` → コマンドライン引数でシード設定（引数なしで自動検出）
- `MEM_METHOD=MEM_MALLOC` → 標準ヒープ割り当て

これは K230 SDK の CoreMark 実装（`k230_sdk/src/big/unittest/testcases/coremark/`）と同方式です。musl (RT-Smart) / glibc (Linux) の両方で動作します。

### カスタムソースファイル不要の理由

- `CORETIMETYPE` は `core_portme.c` 内でのみ使用（`core_main.c` では `CORE_TICKS`=`clock_t` を使用）
- posix port の `core_portme.c` は K230 SDK の実装とほぼ同一
- 設定差分（ITERATIONS 等）は実行時引数または CMake `-D` で対応可能
- `FLAGS_STR` のみ CMake から定義（upstream Makefile でも `-D` で渡す設計）

### -O2 上書き

RT-Smart toolchain は `-O0` を設定しますが、ベンチマークとして意味がありません。GCC は最後の `-O` フラグを採用するため、toolchain の `-O0` の後に `-O2` を追加することで `-O2` が有効になります。Linux toolchain は既に `-O2` のため影響ありません。

### タイミング精度

RT-Smart の `clock_gettime(CLOCK_REALTIME)` は tick ベース（`RT_TICK_PER_SECOND=1000`）で **分解能 1ms** です。CoreMark は約 10 秒間実行されるため、1ms 分解能での測定誤差は約 0.01% と十分な精度です。

`-lrt` は不要です。musl / modern glibc ともに `clock_gettime` は libc に含まれています。

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

FetchContent でビルド時に CoreMark ソースを自動取得するため、手動でのソース配置は不要です。`coremark_src_SOURCE_DIR` に展開されたソースから、コアファイル 5 つと `posix/core_portme.c` をビルドします。

## ビルド手順

リポジトリのルートディレクトリから実行します。

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

### 生成された ELF を確認

=== "bigcore (RT-Smart)"

    ```bash
    file build/coremark/coremark
    ```

    期待される出力:

    ```
    coremark: ELF 64-bit LSB executable, UCB RISC-V, RVC, double-float ABI, version 1 (SYSV), statically linked, ...
    ```

=== "littlecore (Linux)"

    ```bash
    file build/coremark_linux/coremark
    ```

    期待される出力:

    ```
    coremark: ELF 64-bit LSB executable, UCB RISC-V, RVC, double-float ABI, version 1 (SYSV), statically linked, ...
    ```

## K230 への転送と実行

=== "bigcore (RT-Smart)"

    ### SCP で転送する

    ```bash
    scp build/coremark/coremark root@<K230のIPアドレス>:/sharefs/coremark
    ```

    !!! warning "/sharefs/ について"
        転送先は `/sharefs/coremark` です（`/root/sharefs/` ではありません）。
        `/sharefs/` は bigコアから直接アクセスできる vfat パーティション（`/dev/mmcblk1p4`）で、
        K230 の Linux 側からも同じパスでアクセスできます。

    ### K230 bigコア（msh）で実行

    ```
    msh /> /sharefs/coremark
    2K performance run parameters for coremark.
    CoreMark Size    : 666
    ...
    CoreMark 1.0 : xxx.xx / GCC12.0.1 ... / STACK
    ```

    !!! tip "シリアル接続"
        - **小コア (Linux)**: `/dev/ttyACM0`（115200 bps）
        - **bigコア (RT-Smart msh)**: `/dev/ttyACM1`（115200 bps）

        ```bash
        minicom -D /dev/ttyACM1 -b 115200
        ```

=== "littlecore (Linux)"

    ### SCP で転送する

    ```bash
    scp build/coremark_linux/coremark root@<K230のIPアドレス>:/root/coremark
    ```

    ### K230 littleコア（Linux シェル）で実行

    ```
    [root@canmv ~]# /root/coremark
    2K performance run parameters for coremark.
    CoreMark Size    : 666
    ...
    CoreMark 1.0 : xxx.xx / GCC10.2.0 ... / STACK
    ```

    !!! tip "シリアル接続"
        - **小コア (Linux)**: `/dev/ttyACM0`（115200 bps）
        - **bigコア (RT-Smart msh)**: `/dev/ttyACM1`（115200 bps）

        ```bash
        minicom -D /dev/ttyACM0 -b 115200
        ```

## 実行オプション

CoreMark は引数なしで実行すると、デフォルト設定（ITERATIONS 自動検出、SEED 自動選択）で動作します。

```
coremark <seed1> <seed2> <seed3> <iterations>
```

| 引数 | 説明 | デフォルト |
|------|------|-----------|
| seed1, seed2, seed3 | ベンチマークの入力シード | 自動選択（validation/performance/profile） |
| iterations | 実行回数 | 自動（最低 10 秒間実行される値） |

例: 50000 イテレーションで実行:

```bash
/sharefs/coremark 0 0 0x66 50000
```

## 将来の拡張

### マルチスレッド対応

マルチスレッドで CoreMark を実行する場合:

1. `apps/coremark/src/core_portme.c` に upstream の `posix/core_portme.c` をコピー
2. CMakeLists.txt のソースパスを `src/core_portme.c` に変更
3. CMake で `-DMULTITHREAD=4 -DUSE_PTHREAD=1` を追加し、`target_link_libraries` に `pthread` を追加
