## Drivers for the CutiePi tablet 

This repository hosts modified drivers and device tree sources needed for the [CutiePi board](https://github.com/cutiepi-io/cutiepi-board), the open source Raspberry Pi Compute Module 4 carrier board. 

Current release is based on Raspbian Buster and kernel 5.10.0. 

### MIPI Display 

CutiePi tablet chose to use an 8-inch (800x1280) MIPI DIS TFT LCD display, it has `JD9366` as its LCD driver. 

To enable this, copy all files under `Display/drivers/gpu/drm/panel/` to kernel source directory, build the module with `CONFIG_DRM_PANEL_JD9366=M`, then load `panel-jd9366`. 

A device tree overlay is also needed, compile `Display/panel-jd9366.dts` with following command: 

    dtc -I dts -O dtb -o panel-jd9366.dtbo panel-jd9366.dts

Copy the file to `/boot/overlays`, and configure in `config.txt`: 

    dtoverlay=vc4-kms-v3d-pi4 
    dtoverlay=panel-jd9366
    ignore_lcd=1
    gpio=12=op,dh 

### Touch panel 

To enable Goodix GT9xx multitouch controller, compile the device tree overlay under `Touch`: 

    dtc -I dts -O dtb -o cutiepi_touch.dtbo cutiepi_touch.dts

And configure `config.txt` accordingly: 

    dtoverlay=i2c6
    dtoverlay=cutiepi_touch
