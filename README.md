## Drivers for the CutiePi tablet 

This repository hosts modified drivers and device tree sources needed for the [CutiePi board](https://github.com/cutiepi-io/cutiepi-board), the open source Raspberry Pi Compute Module 4 carrier board. 

Current release is tested on Raspberry Pi OS (`2021-08-31`) with kernel 5.10 and 5.11. 

### MIPI Display 

CutiePi tablet uses an 8-inch (800x1280) MIPI DIS TFT LCD display, it has `ILI9881C` as its LCD driver. 

The `panel-nwe080` driver is now a dkms package, and can be installed to the system using following commands: 

    # on the Pi system 
    sudo apt install dkms raspberrypi-kernel-headers 
    sudo tar xvf Display/panel-nwe080-1.0.tgz -C /usr/src/

    sudo dkms add -m panel-nwe080 -v 1.0
    sudo dkms build -m panel-nwe080 -v 1.0
    sudo dkms install -m panel-nwe080 -v 1.0

A device tree overlay is also needed, which can be compiled from `Display/panel-nwe080.dts` with following command: 

    dtc -I dts -O dtb -o panel-nwe080.dtbo panel-nwe080.dts

Copy the file to `/boot/overlays`, then add following configure in `config.txt`: 

    # MIPI DSI display 
    dtoverlay=panel-nwe080
    gpio=12=op,dh

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

### USB host 
    
    dtoverlay=dwc2,dr_mode=host

### MCU 

    # MCU reading (ttyS0)
    enable_uart=1
    dtoverlay=uart1
