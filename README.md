## Drivers for the CutiePi tablet 

This repository hosts modified drivers and device tree sources needed for the [CutiePi board](https://github.com/cutiepi-io/cutiepi-board), the open source Raspberry Pi Compute Module 4 carrier board. 

Current release is tested on Raspberry Pi OS (`2021-05-07`) and kernel 5.10 (`5.10.17-v7l+`).

### MIPI Display 

CutiePi tablet uses an 8-inch (800x1280) MIPI DIS TFT LCD display, it has `ILI9881C` as its LCD driver. 

To enable this, copy all files under `Display/drivers/gpu/drm/panel/` to the kernel source directory, build the module with `CONFIG_DRM_PANEL_NWE080=M`, and load `panel-nwe080.ko`:

    make oldconfig 
    make scripts prepare modules_prepare
    make -C . M=drivers/gpu/drm/panel/

A device tree overlay is also needed, which can be compiled from `Display/panel-nwe080.dts` with following command: 

    dtc -I dts -O dtb -o panel-nwe080.dtbo panel-nwe080.dts

Copy the file to `/boot/overlays`, then add following configure in `config.txt`: 

    # MIPI DSI display 
    dtoverlay=panel-nwe080

### Touch panel 

To enable Goodix GT9xx multitouch controller, compile the device tree overlay under `Touch`: 

    dtc -I dts -O dtb -o cutiepi_touch.dtbo cutiepi_touch.dts

And configure `config.txt` accordingly: 

    dtoverlay=i2c6
    dtoverlay=cutiepi_touch

### Camera 

    # Camera (with dt-blob.bin)
    start_x=1
    gpu_mem=128
    # Uncomment for camera module v2
    #dtoverlay=imx219
    dtoverlay=ov5647

### Gyroscope 

    # Gyroscope 
    dtoverlay=i2c5,pins_10_11

### MCU 

    # MCU reading (ttyS0)
    enable_uart=1
    dtoverlay=uart1