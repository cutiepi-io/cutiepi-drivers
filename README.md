## Drivers for the CutiePi tablet 

This repository hosts modified drivers and device tree sources needed for the [CutiePi board](https://github.com/cutiepi-io/cutiepi-board), the open source Raspberry Pi Compute Module 3 carrier board. 

Current release is based on Raspbian Buster and kernel 4.19.50. 

### WiFi 

The CutiePi board has `RTL8723BS` module built-in, simply enable its staging kernel module (`CONFIG_RTL8723BS=M`), and re-configure `sdio` overlay to GPIO34-GPIO39 in `config.txt`: 

    dtoverlay=sdio,sdio_overclock=25,gpios_34_39,poll_once=off

Firmware file (`rtl8723bs_nic.bin`) is provided by the `firmware-realtek` package from Raspbian. 

### MIPI Display 

CutiePi tablet chose to use an 8-inch (800x1280) MIPI DIS TFT LCD display, it has `JD9366` as its LCD driver. 

To enable this, copy all files under `Display/drivers/gpu/drm/panel/` to kernel source directory, build the module with `CONFIG_DRM_PANEL_JD9366=M`, then load `panel-jd9366`. 

A device tree overlay is also needed, compile `Display/panel-jd9366.dts` with following command: 

    dtc -I dts -O dtb -o panel-jd9366.dtbo panel-jd9366.dts

Copy the file to `/boot/overlays`, and configure in `config.txt`: 

    dtoverlay=vc4-kms-v3d
    dtoverlay=panel-jd9366
    ignore_lcd=1
    gpio=12=op,dh 
    
### Touch panel 

To enable Goodix GT9xx multitouch controller, compile the device tree overlay under `Touch`: 

    dtc -I dts -O dtb -o cutiepi_touch.dtbo cutiepi_touch.dts

And configure `config.txt` accordingly: 

    dtoverlay=i2c0-bcm2708,sda0_pin=0,scl0_pin=1
    dtoverlay=cutiepi_touch
