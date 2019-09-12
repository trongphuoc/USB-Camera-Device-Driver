# USB-Camera-Device-Driver
1)Kernel Module: this folder contain driver for usb camera. 
To build this driver as kernel module in Legato platform please visit at website:
https://docs.legato.io/18_06/conceptsKernelModule.html
https://docs.legato.io/18_06/getStartedKO.html

************Note*****************
Customize linux kernel for v4l2 driver:
To install this kernel module, we need to customize some fearture of 3.14.29ltsi-yocto-standard by menuconfig. Follow this instruction
to use menuconfig: 
https://github.com/mangOH/mangOH/tree/master/experimental/waveshare_eink/linux_kernel_modules
https://mangoh.io/tutorials-advanced
To enable v4l2 driver by menuconfig:
a) Enable multimedia support
b) Enable Camera/video grabbers support
c) Enable Media controller API
d) V4L2 sub-device userspace API
e) Enable Media USB adapters 
f) Build dUSB Video Class(UVC) as built-in
=> save and build kernel to apply new feature.

2)Application in user space
To build an application in Legato platform, please visit these source:
https://docs.legato.io/18_05/getStartedHW.html
https://docs.legato.io/18_05/getStartedApps.html

The application in this repo is develop in basic linux system, if you want to build application in Legato system, you should adjust 
the main function to appropriate with format of an application in Legato .
