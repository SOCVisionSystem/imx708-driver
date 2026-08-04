# Install

## Kernel module

### Using modprobe (after install)

```bash
sudo make install
sudo depmod -a
sudo modprobe imx708
```

### Manual insmod

```bash
sudo insmod build/native/module/imx708.ko
```

### Check if loaded

```bash
lsmod | grep imx708
dmesg | tail -20
```

Expected dmesg output:
```
imx708: driver loaded (version 0.1.0)
imx708 1-001a: probing Sony IMX708 sensor
imx708 1-001a: chip ID: 0x0708
imx708 1-001a: Sony IMX708 sensor probed successfully
```

## Device tree overlay

On Raspberry Pi OS:

```bash
# Copy overlay
sudo cp dts/imx708-rpi.dtbo /boot/overlays/

# Add to /boot/config.txt
echo "dtoverlay=imx708-rpi" | sudo tee -a /boot/config.txt

# Reboot
sudo reboot
```

## Userspace library

```bash
sudo make install PREFIX=/usr/local
```

This installs:
- `/usr/local/lib/libimx708.so.0.1.0`
- `/usr/local/lib/libimx708.a`
- `/usr/local/include/libimx708.h`
- `/usr/local/lib/pkgconfig/libimx708.pc`

## udev rules

The driver creates `/dev/imx7080`. To allow non-root access:

```bash
# Create udev rule
echo 'KERNEL=="imx708[0-9]*", MODE="0660", GROUP="video"' | \
    sudo tee /etc/udev/rules.d/99-imx708.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

## Unbind conflicting drivers

If the in-tree `imx708` driver is already loaded:

```bash
# Blacklist it
echo "blacklist imx708" | sudo tee /etc/modprobe.d/blacklist-imx708.conf
sudo rmmod imx708
sudo modprobe imx708  # our version
```
