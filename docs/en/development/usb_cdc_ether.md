# USB CDC-Ether (littlecore Linux)

Investigation results and instructions for enabling USB CDC Ethernet (ECM/RNDIS) on K230 littlecore (Linux) to establish a network connection over a USB cable.

!!! warning "Not available with the default kernel"
    All CDC Ethernet features are disabled in the default kernel configuration. A kernel rebuild is required.

## Hardware Status

The K230 has a DWC2 OTG controller that supports USB device mode.

| Item | Value |
|------|-------|
| UDC controller | `91500000.usb-otg` |
| DWC2 driver | Loaded (91500000, 91540000) |
| usbotg0 | OTG mode (device mode capable) |
| usbotg1 | Host only (`dr_mode = "host"`) |

In the device tree (`k230_canmv.dts`), `usbotg0` is enabled in OTG mode, making it usable as a USB gadget.

## Current Kernel Configuration

The USB gadget framework is enabled, but all CDC Ethernet features are disabled.

**Enabled features:**

```
CONFIG_USB_DWC2=y
CONFIG_USB_DWC2_DUAL_ROLE=y
CONFIG_USB_GADGET=y
CONFIG_USB_CONFIGFS=y
CONFIG_USB_CONFIGFS_MASS_STORAGE=y
CONFIG_USB_CONFIGFS_F_LB_SS=y
CONFIG_USB_CONFIGFS_F_HID=y
CONFIG_USB_CONFIGFS_F_UVC=y
```

**Disabled features (CDC Ethernet):**

```
# CONFIG_USB_CONFIGFS_NCM is not set
# CONFIG_USB_CONFIGFS_ECM is not set
# CONFIG_USB_CONFIGFS_ECM_SUBSET is not set
# CONFIG_USB_CONFIGFS_RNDIS is not set
# CONFIG_USB_CONFIGFS_EEM is not set
# CONFIG_USB_ETH is not set
```

## How to Enable

### 1. Modify defconfig

Target file: `k230_sdk/src/little/linux/arch/riscv/configs/k230_canmv_defconfig`

Add ECM (natively supported on Linux/macOS):

```
CONFIG_USB_CONFIGFS_ECM=y
```

If Windows support is also needed, add RNDIS:

```
CONFIG_USB_CONFIGFS_RNDIS=y
```

Append these lines after the existing USB configfs settings (after `CONFIG_USB_CONFIGFS_F_UVC=y`).

### 2. Rebuild the Kernel

Inside the SDK Docker environment:

```bash
make linux       # Rebuild Linux kernel
make build-image # Regenerate SD card image
```

!!! note "Building the SDK"
    For K230 SDK build instructions, see [SDK Build](sdk_build.md).

### 3. configfs Setup on the Device

After rebuilding the kernel, mount configfs and create the ECM gadget on the device.

```bash
#!/bin/sh
# gadget-ecm.sh — USB CDC ECM gadget setup

mount -t configfs none /sys/kernel/config

mkdir -p /sys/kernel/config/usb_gadget/g1
cd /sys/kernel/config/usb_gadget/g1

echo 0x1d6b > idVendor    # Linux Foundation
echo 0x0104 > idProduct    # Multifunction Composite Gadget

mkdir -p strings/0x409
echo "Canaan Inc." > strings/0x409/manufacturer
echo "CDC Ether" > strings/0x409/product
echo "20230618" > strings/0x409/serialnumber

mkdir -p configs/c.1/strings/0x409
echo "ECM" > configs/c.1/strings/0x409/configuration

mkdir -p functions/ecm.usb0
ln -s functions/ecm.usb0 configs/c.1/

echo 91500000.usb-otg > UDC

ifconfig usb0 192.168.7.2 netmask 255.255.255.0 up
```

### 4. Host-side Setup

After connecting the K230 to the host PC via USB cable, configure the IP address on the host.

```bash
# Linux host
sudo ifconfig usb0 192.168.7.1 netmask 255.255.255.0 up

# Test connectivity
ping 192.168.7.2
```

## ECM vs RNDIS

| Protocol | Linux | macOS | Windows | Notes |
|----------|-------|-------|---------|-------|
| ECM | No driver needed | No driver needed | Not supported | Recommended |
| RNDIS | No driver needed | Not supported | No driver needed | For Windows |

ECM alone is sufficient for Linux/macOS. Enable RNDIS as well if Windows support is needed.

## References

- Existing gadget script examples:
    - `k230_sdk/src/little/buildroot-ext/package/usb_test/src/gadget-storage.sh`
    - `k230_sdk/src/little/buildroot-ext/package/usb_test/src/gadget-hid.sh`
