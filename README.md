## Drivers for the CutiePi tablet 

This repository hosts modified drivers and device tree sources needed for the [CutiePi board](https://github.com/cutiepi-io/cutiepi-board), the open source Raspberry Pi Compute Module 4 carrier board. 

Current release is tested on Raspberry Pi OS (`2021-08-31`) with kernel 5.10 and 5.11. 

### MIPI Display and touch 

CutiePi tablet uses an 8-inch (800x1280) MIPI DIS TFT LCD display, it has `ILI9881C` as its LCD driver. 



A device tree overlay is also needed, which can be compiled from `Display/cutiepi-panel-overlay.dts` with following command: 

    dtc -I dts -O dtb -o cutiepi-panel.dtbo cutiepi-panel-overlay.dts

Copy the file to `/boot/overlays`, then add following configure in `config.txt`: 

    # MIPI DSI display 
    dtparam=i2c_arm=on
    dtoverlay=cutiepi-panel

### Camera 

    # camera 
    start_x=1
    gpu_mem=128
    # Uncomment for camera module v2
    #dtoverlay=imx219
    dtoverlay=ov5647

### Gyroscope 

Compile the `mpu6050-i2c5` overlay and copy it to `/boot/overlays`. 

    # Gyroscope 
    dtoverlay=i2c5,pins_10_11
    dtoverlay=mpu6050-i2c5,interrupt=27

### USB host 

    otg_mode=1

### MCU 

    # MCU reading (ttyS0)
    enable_uart=1
    dtoverlay=uart1

Make sure `#dtparam=spi=on` is commented out. 