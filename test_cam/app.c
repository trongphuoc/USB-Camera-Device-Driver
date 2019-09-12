/*
* @file     cam_test.c
* @author   Trong Phuoc
* @brief    Application in user space to control camera driver
*/
/*******************************************************************************
 *  INCLUDES
 ******************************************************************************/
#include "cam_test.h"


void main()
{
    //opening the device
    int fd;
    fd = openDevice();
    // init device
    deviceInit(fd);
    //capturing
    startCapturing(fd);
    // main loop to get raw data
    mainloop(fd);
    // readFrame(fd);
    // stop capturing
    stopCapturing(fd);
    // release device
    deviceUninit();
    // close device
    closeDevice(fd);
}
