/*
* @file     cam_test.h
* @author   Trong Phuoc
* @brief    APIs in user space to control camera driver
*/
/*******************************************************************************
 *  INCLUDES
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h> /* getopt_long() */

#include <fcntl.h> /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <linux/videodev2.h>
/*******************************************************************************
 *  DEFINE 
 ******************************************************************************/
#define DEVICE_NAME "/dev/video2"
#define RETURN_STATUS_OK    0
#define RETURN_STATUS_ERR  -1
#define IOCTL_STATUS_OK     0
#define IOCTL_ERROR        -1
#define INSUFFICENT_BUFF   -1
#define INVALID_METHOD     -1
#define MEM_MAP_FAILED     -1

/*******************************************************************************
 *  MACRO 
 ******************************************************************************/
#define CLEAR(x) memset(&x, 0, sizeof(x))

/*********************************************************************************
 * TYPEDEF
**********************************************************************************/
typedef struct buffer
{
    void *start;   /**< pointer point to address of buffer in user space*/
    size_t length; /**< the length of buffer */
} buffer;

enum ioMethod
{
    IO_METHOD_READ,   /**<  Read/Write method to exchange data with driver */
    IO_METHOD_MMAP,   /**<  MMAP method to exchange pointer to data */
    IO_METHOD_USRPTR, /**<  USER POINTER method */
};

/*******************************************************************************
 * FUNCTIONS - API
 ******************************************************************************/

/*******************************************************************************
 * @func    static int openDevice(void)
 * 
 * @brief   open the device file in /dev/video*       
 * @return  fd(file descriptor when open a file) - Success
 * @return  ERROR - Open device file is failed
 *******************************************************************************/
static int openDevice(void);

/*******************************************************************************
 * @func    static int closeDevice(int fd)
 *
 * @brief   close the device file in /dev/video*       
 * @param   fd - file descriptor of device file
 * @return  RETURN_STATUS_OK - close success
 * @return  ERROR            - Close device failed
 *******************************************************************************/
static int closeDevice(int fd);

/**********************************************************************************
 * @func     static int printCapabilities(int fd, struct v4l2_capability caps)
 * 
 * @brief    enumerate capabilities of uvc device
 * @param    fd     - file descriptor when open the device
 * @param    caps   - contain all information about the capabiliies of camera
 * @return   INVALID_METHOD    - the I/O method is not supported by driver
 * @return   RETURN_STATUS_OK  - query capability of device success
 * 
***********************************************************************************/
static int printCapabilities(int fd, struct v4l2_capability caps);

/**********************************************************************************
 * @func     static int getInput(int fd, unsigned int *index)
 * 
 * @brief    get index of camera device input
 * @param    fd     - file descriptor when open the device
 * @param    index  - a pointer to integer number that get the value of index
 * @return   IOCTL_ERROR      - the ioctl VIDIOC_G_INPUT is not success
 * @return   RETURN_STATUS_OK - query capability of device success
 * 
***********************************************************************************/
static int getInput(int fd, unsigned int *index);

/**********************************************************************************
 * @func     static int enumInput(int fd, struct v4l2_input input)

 * @brief    enumerate video device input
 * @param    fd     - file descriptor when open the device
 * @param    input  - get informations about input device
 * @return   IOCTL_ERROR      - the ioctl VIDIOC_ENUMINPUT is failed
 * @return   RETURN_STATUS_OK - enumerate video device input success
 * 
***********************************************************************************/
static int enumInput(int fd, struct v4l2_input input);

/**********************************************************************************
 * @func     static int setInput(int fd, int index)
 * 
 * @brief    set index of camera device input
 * @param    fd     - file descriptor when open the device
 * @param    input  - informations about index of input device
 * @return   IOCTL_ERROR      - the ioctl VIDIOC_ENUMINPUT is failed
 * @return   RETURN_STATUS_OK - enumerate video device input success
 * 
***********************************************************************************/
static int setInput(int fd, int index);

/**********************************************************************************
 * @func    static int setFormat(int fd, struct v4l2_format format)
 * 
 * @brief   Negotiate format of pixel with device
 * @para    fd     - file descriptor when open the device
 * @para    format - contain information about format of pixel 
 * @return  IOCTL_ERROR      - the ioctl VIDIOC_S_FMT is failed
 * @return  RETURN_STATUS_OK - setting format success
**********************************************************************************/
static int setFormat(int fd, struct v4l2_format format);

/**********************************************************************************
 * @func    static int getFormat(int fd, struct v4l2_format format)
 * 
 * @brief   get format of pixel
 * @param   fd      - file descriptor when open the device
 * @param   format  - contain information about format of pixel 
 * @return  IOCTL_ERROR      - the ioctl VIDIOC_G_FMT is failed
 * @return  RETURN_STATUS_OK - getting format success
**********************************************************************************/
static int getFormat(int fd, struct v4l2_format format);

/**********************************************************************************
 * @func    static int requestBuffer(int fd, struct v4l2_requestbuffers *reqbuff)
 * 
 * @brief   Allocate device buffers
 * @param   fd      - file descriptor when open the device
 * @param   reqbuff - contain information about number of buffer, type of memory.
 * @return  IOCTL_ERROR      - ioctl VIDIOC_REQBUFS is failed
 * @return  INSUFFICENT_BUFF - not enough buffer
 * @return  RETURN_STATUS_OK - request buffer success
***********************************************************************************/
static int requestBuffer(int fd, struct v4l2_requestbuffers *reqbuff);

/**********************************************************************************
 * @func    static void init_mmap_method(int fd)
 * 
 * @brief   Initialize to use MMAP method to exchange data between user space and 
 *          kernel space 
 * @param   fd: file descriptor when open the device
 * @return  IOCTL_ERROR - ioctl VIDIOC_QUERYBUF is failed
 * @return  MAP_FAILED  - mapping memory address between user space and kernel space 
 *          is failed  
/**********************************************************************************/
static void init_mmap_method(int fd);

/**********************************************************************************
 * @func    static int enumFormat(int fd)
 * 
 * @brief   Enumerate format of the device
 * @param   fd - file descriptor when open the device
 * @return  IOCTL_ERROR      - ioctl VIDIOC_ENUM_FMT is failed
 * @return  RETURN_STATUS_OK - enumerate format success
 
***********************************************************************************/
static int enumFormat(int fd);

/**********************************************************************************
 * @func  static void deviceInit(int fd)
 * 
 * @brief: Initialize the camera device to get information of the device and set 
 *         some requirement depend on users
 * @param: fd - file descriptor when open the device
 * 
***********************************************************************************/
static void deviceInit(int fd);

/**********************************************************************************
 * @func  static void deviceUninit();
 * 
 * @brief: Uninitialize the camera device 
 * 
***********************************************************************************/
static void deviceUninit();

/**********************************************************************************
 * @func    static int startCapturing(int fd)
 * 
 * @brief   Start capturing image from camera device
 * @param   fd - file descriptor when open the device
 * @return  IOCTL_ERROR      - ioctl VIDIOC_QBUF is failed
 * @return  RETURN_STATUS_OK - Success
 
***********************************************************************************/
static int startCapturing(int fd);

/**********************************************************************************
 * @func    static void stopCapturing(int fd)
 * 
 * @brief   Stop capturing image from camera device
 * @param   fd - file descriptor when open the device
 * @return  IOCTL_ERROR      - ioctl VIDIOC_STREAMOFF is failed
 * @return  RETURN_STATUS_OK - success
***********************************************************************************/
static void stopCapturing(int fd);

/**********************************************************************************
 * @func    static void processImage(const void *pointer, int size)
 * 
 * @brief   Write data captured from device to file .raw
 * @param   fd      - file descriptor when open the device
 * @param   size    - the size of frame is wrote into raw file
 * *******************************************************************************/
static void processImage(const void *pointer, int size);

/**********************************************************************************
 * @func    static int readFrame(int fd)
 * 
 * @brief   Get data(frame) from kernel space 
 * @param   fd      - file descriptor when open the device
 * @return  IOCTL_ERROR      - ioctl VIDIOC_DQBUF or VIDIOC_QBUF is failed
 *                            (user read kernel log to get detail error)
 * @return  RETURN_STATUS_OK - Success
 * *******************************************************************************/
static int readFrame(int fd);

/**********************************************************************************
 * @func    static void mainloop(int fd)
 * 
 * @brief   loop program use I/O multiplexing method to prevent infinity loop when
 *          open an device file that not exist or not really to use
 * @param   fd      - file descriptor when open the device
 *
 * *******************************************************************************/
static void mainloop(int fd);
