# RT-Smart Boot Customization

This guide explains the RT-Smart (bigcore) boot sequence and how to create an image that waits at the msh shell prompt.

## RT-Smart Boot Sequence

The K230 bigcore (RT-Smart) boots in the following order:

```
RT-Smart kernel startup
  ↓
Component initialization (INIT_BOARD_EXPORT → ... → INIT_APP_EXPORT)
  ├── finsh_system_init() → creates and starts msh shell thread "tshell"
  └── ...
  ↓
Mount ROMFS at /
  ↓
main() runs → msh_exec("/bin/init.sh")
  ↓
Application launches according to init.sh contents
```

### msh Shell Thread

The msh shell thread (`tshell`) is automatically started during component initialization (`finsh_system_init`).
If `main()` does not launch a blocking application, the msh shell becomes interactively usable via the serial console.

### main() Behavior

`main()` executes `RT_SHELL_PATH` (default: `/bin/init.sh`) via `msh_exec()`.

```c
// k230_sdk/src/big/rt-smart/kernel/bsp/maix3/applications/main.c
#ifndef RT_SHELL_PATH
#define RT_SHELL_PATH "/bin/init.sh"
#endif

int main(void)
{
    // ...
    msh_exec(RT_SHELL_PATH, strlen(RT_SHELL_PATH)+1);
    return 0;
}
```

## Default Behavior

The default `init.sh` launches the face detection demo application (`fastboot_app.elf`).

```sh
# k230_sdk/src/big/rt-smart/init.sh (default)
/bin/fastboot_app.elf /bin/test.kmodel
```

`fastboot_app.elf` starts camera, video output, and face detection, running as a blocking process.
While the msh shell still accepts input, the console output is dominated by the application.

### init.sh Copy Path

`init.sh` is embedded into the firmware image through the following path:

```
k230_sdk/src/big/rt-smart/init.sh
  ↓ (copied by make mpp)
k230_sdk/src/big/rt-smart/userapps/root/bin/init.sh
  ↓ (converted to ROMFS by mkromfs.py)
k230_sdk/src/big/rt-smart/kernel/bsp/maix3/applications/romfs.c
  ↓ (built into kernel)
RT-Smart kernel binary
```

## Creating an msh Shell-Wait Image

### Method 1: Using the Build Script

Pass `--no-fastboot` to `scripts/build_sdk.sh` to generate an image that does not launch `fastboot_app.elf` and instead waits at the msh shell prompt.

```bash
scripts/build_sdk.sh --no-fastboot
```

### Method 2: Manually Editing init.sh

Edit `init.sh` directly and replace the `fastboot_app.elf` launch line with a comment or leave it empty.

```sh
# k230_sdk/src/big/rt-smart/init.sh
# msh shell ready
```

Then rebuild the SDK.

## Partial Build

After modifying `init.sh`, you can run a full build (`make`), but the following partial build steps save time.
Run these inside the Docker container in the `k230_sdk/` directory.

```bash
# 1. MPP build — copies init.sh to userapps/root/bin/
make mpp

# 2. RT-Smart user apps + ROMFS regeneration — mkromfs.py → romfs.c
make rt-smart-apps

# 3. RT-Smart kernel rebuild — picks up updated romfs.c
make rt-smart-kernel

# 4. OpenSBI rebuild — embeds RT-Smart binary as payload
make big-core-opensbi

# 5. Final SD card image generation
make build-image
```

A full build (`make`) includes all of the above plus Linux (littlecore) and more, so partial builds are faster when only `init.sh` has changed.

## Related Files

| File | Role |
|------|------|
| `k230_sdk/src/big/rt-smart/init.sh` | Boot script source |
| `k230_sdk/src/big/rt-smart/kernel/bsp/maix3/applications/main.c` | RT-Smart main(). Calls `msh_exec(RT_SHELL_PATH)` |
| `k230_sdk/src/big/rt-smart/kernel/bsp/maix3/applications/romfs.c` | Auto-generated. Created by mkromfs.py from `userapps/root/` |
| `k230_sdk/src/big/rt-smart/kernel/bsp/maix3/rtconfig.h` | RT-Smart kernel config (`RT_USING_MSH`, etc.) |
| `k230_sdk/src/big/rt-smart/kernel/rt-thread/components/finsh/shell.c` | msh shell thread implementation |
| `k230_sdk/Makefile` | Build pipeline (`mpp` → `rt-smart-apps` → `rt-smart-kernel`) |
