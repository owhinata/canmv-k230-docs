# rtt-ctrl — Control RT-Smart from Linux

`rtt-ctrl` is a utility for remotely executing msh shell commands on RT-Smart (bigcore) from Linux (littlecore).
It sends commands to the `rc_server` inside the RT-Smart kernel via IPCM (Inter-Processor Communication) and receives the execution results.

## How It Works

### Architecture

```
Linux (littlecore)                    RT-Smart (bigcore)
┌──────────────┐                     ┌──────────────┐
│ rtt-ctrl     │  IPCM port 7       │ rc_server    │
│ (user app)   │ ──────────────────→ │ (kernel)     │
│              │ ←────────────────── │              │
└──────┬───────┘   /dev/ipcm_user   └──────┬───────┘
       │                                    │
       │                              msh_exec(cmd)
       │                                    │
  response (ret code)              RT-Smart msh shell
```

### Communication Flow

1. Linux side: `rtt-ctrl` command opens `/dev/ipcm_user`
2. Connects to RT-Smart's `rc_server` via IPCM port 7
3. Sends command string as `RC_CMD_EXEC` message (max 128 bytes)
4. RT-Smart side: `rc_server` executes the command via `msh_exec()`
5. Returns the result (return code) as a response

### Enabling

`scripts/build_sdk.sh` enables `RT_USING_RTT_CTRL` by default.

```bash
# Default (rtt-ctrl enabled)
scripts/build_sdk.sh

# To disable rtt-ctrl
scripts/build_sdk.sh --no-rtt-ctrl
```

Internally, this adds `#define RT_USING_RTT_CTRL` to the defconfig source (`configs/common_rttlinux.config`).
During build, `menuconfig_to_code.sh` copies this file to `rtconfig.h`, so editing `rtconfig.h` directly will be overwritten.
When undefined, a weak symbol stub is used and `"no rtt ctrl device library!"` is displayed.

### Source Code

| File (SDK path) | Role |
|-----------------|------|
| `src/common/cdk/kernel/ipcm/class/rtt-ctrl/rc_command.h` | Command protocol definitions (`RC_CMD_EXEC`, etc.) |
| `src/common/cdk/kernel/ipcm/class/rtt-ctrl/rc_common.h` | Common header |
| `src/common/cdk/kernel/ipcm/class/rtt-ctrl/rt-smart/rc_server.c` | RT-Smart side server implementation |
| `src/common/cdk/kernel/ipcm/class/rtt-ctrl/linux/rc_client.c` | Linux side client implementation |
| `src/common/cdk/kernel/ipcm/class/rtt-ctrl/server_ipc.c` | RT-Smart side IPC handling |
| `src/common/cdk/kernel/ipcm/class/rtt-ctrl/client_ipc.c` | Linux side IPC handling |
| `src/big/rt-smart/kernel/bsp/maix3/board/ipcm/rtt_ctrl_init.c` | RT-Smart kernel initialization |

## Usage

### Basic

```bash
rtt-ctrl "command"
```

### Examples

```bash
# List RT-Smart threads
rtt-ctrl "list_thread"

# Show memory usage
rtt-ctrl "free"

# List available msh commands
rtt-ctrl "help"
```

### Notes

- Command string must be **128 bytes or less**
- Connection timeout: approx. 1 second
- Command response timeout: approx. 1 second
- IPCM driver must be loaded (`/dev/ipcm_user` must exist)

## Related Files

| File | Role |
|------|------|
| `scripts/build_sdk.sh` | `RT_USING_RTT_CTRL` enablement logic |
| `k230_sdk/src/big/rt-smart/kernel/bsp/maix3/configs/common_rttlinux.config` | Defconfig source (copied to `rtconfig.h` during build) |
| `k230_sdk/src/big/rt-smart/kernel/bsp/maix3/rtconfig.h` | RT-Smart kernel configuration (auto-generated) |
