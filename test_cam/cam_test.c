/*
* @file     cam_test.c
* @author   Trong Phuoc
* @brief    Application in user space to control camera driver
*/
/*******************************************************************************
 *  INCLUDES
 ******************************************************************************/
#include "cam_test.h"

/*******************************************************************************
 *  VALUE DEFINITION
 ******************************************************************************/
enum ioMethod io = IO_METHOD_MMAP;
buffer *buffers;
static unsigned int n_buffers;
unsigned int frame_number = 0;
unsigned frame_count = 1;

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
static int openDevice(void)
{
    int fd;
    fd = open(DEVICE_NAME, O_RDWR | O_NONBLOCK, 0);
    if (fd < 0)
    {
        printf("Can not open camera device \n");
        return RETURN_STATUS_ERR;
    }
    else
    {
        return fd;
    }
}
/*******************************************************************************
 * @func    static int closeDevice(int fd)
 *
 * @brief   close the device file in /dev/video*       
 * @param   fd - file descriptor of device file
 * @return  RETURN_STATUS_OK  -  close success
 * @return  RETURN_STATUS_ERR -  Close device failed
 *******************************************************************************/
static int closeDevice(int fd)
{
    int ret = 0;
    if (close(fd) == -1)
    {
        printf("Can not close the device \n");
        ret = RETURN_STATUS_ERR;
    }
    printf("Device is closed \n");
    return RETURN_STATUS_OK;
}
/**********************************************************************************
 * @func     static int printCapabilities(int fd, struct v4l2_capability caps)
 * 
 * @brief    enumerate capabilities of uvc device
 * @param    fd - file descriptor when open the device
 * @param    struct v4l2_capability caps - contain all information about the 
 *           capabiliies of camera
 * @return   -1 - the I/O method is not supported by driver
 * @return    0 - query capability of device success
 * 
***********************************************************************************/
static int printCapabilities(int fd, struct v4l2_capability caps)
{

    int ret = 0;
    ret = ioctl(fd, VIDIOC_QUERYCAP, &caps);
    if (ret != 0)
    {
        printf("Query capabilities \n");
        return RETURN_STATUS_ERR;
    }

    printf("Capabilities of camera device \n");
    printf("Driver: %s \n", caps.driver);
    printf("Card: %s \n", caps.card);
    printf("Version: %d.%d \n", ((caps.version >> 16) & 0xff), ((caps.version >> 16) & 0xff));
    if (!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        printf("Device not support for capturing image \n");
        return INVALID_METHOD;
    }
    else
    {
        printf("Device capability: Capturing the image \n");
    }
    // printf("Capabilities: %08x \n", caps.capabilities);
    switch (io)
    {
    case IO_METHOD_READ:
    {
        if (!(caps.capabilities & V4L2_CAP_READWRITE))
        {
            printf("Device not support read/write mechanism \n");
            return INVALID_METHOD;
        }
        printf("Device support read/write mechanism \n");
        break;
    }
    case IO_METHOD_MMAP:
    case IO_METHOD_USRPTR:
    {
        if (!(caps.capabilities & V4L2_CAP_STREAMING))
        {
            printf("Device not support MMAP mechanism \n");
            return -1;
        }
        printf("Device support MMAP mechanism \n");
        break;
    }
    }

    return 0;
}
static int getInput(int fd, unsigned int *index)
{

    int ret = 0;
    ret = ioctl(fd, VIDIOC_G_INPUT, index);
    if (ret < 0)
    {
        printf("Can not get video input \n");
        return ret;
    }
    return ret;
}
/**********************************************************************************
 * @func     static int enumInput(int fd, struct v4l2_input input)

 * @brief    enumerate video device input
 * @param    fd - file descriptor when open the device
 * @param    input - get informations about input device
 * @return   negative number - the ioctl VIDIOC_ENUMINPUT is failed
 * @return   0 - enumerate video device input success
 * 
***********************************************************************************/
static int enumInput(int fd, struct v4l2_input input)
{
    int ret = 0;
    ret = ioctl(fd, VIDIOC_ENUMINPUT, &input);
    if (ret < 0)
    {
        printf("Enumerate input failed \n");
        return ret;
    }
    printf("Name of the device : %s \n", input.name);

    if (input.type == V4L2_INPUT_TYPE_CAMERA)
    {
        printf("Device is non-tuner video input \n");
    }
    if (input.capabilities == V4L2_IN_CAP_STD)
    {
        printf("Device supports setting the TV standard by using VIDIOC_S_STD \n");
    }
    return ret;
}

static int setInput(int fd, int index)
{
    int ret = 0;
    ret = ioctl(fd, VIDIOC_S_INPUT, &index);
    if (ret < 0)
    {
        printf("invalid device setting \n");
        return ret;
    }
    printf("Setting device success \n");
    return ret;
}
/**********************************************************************************
 * @func    static int setFormat(int fd, struct v4l2_format format)
 * 
 * @brief   Negotiate format of pixel with device
 * @para    fd - file descriptor when open the device
 * @para    format - contain information about format of pixel 
 * @return  negative number - the ioctl VIDIOC_S_FMT is failed
 * @return  0 - setting format success
**********************************************************************************/
static int setFormat(int fd, struct v4l2_format format)
{
    int ret = 0;
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = 640;
    format.fmt.pix.height = 480;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    format.fmt.pix.field = V4L2_FIELD_ANY;
    // format.fmt.pix.colorspace = V4L2_COLORSPACE_JPEG;

    ret = ioctl(fd, VIDIOC_S_FMT, &format);
    if (ret < 0)
    {
        printf("Setting format failed \n");
        return ret;
    }
    printf("Setting format success \n");
    return ret;
}
/**********************************************************************************
 * @func    static int getFormat(int fd, struct v4l2_format format)
 * 
 * @brief   get format of pixel
 * @param   fd - file descriptor when open the device
 * @param   format - contain information about format of pixel 
 * @return  negative number - the ioctl VIDIOC_G_FMT is failed
 * @return  0 - getting format success
**********************************************************************************/
static int getFormat(int fd, struct v4l2_format format)
{
    if (ioctl(fd, VIDIOC_G_FMT, &format) < 0)
    {
        printf("Getting format error \n");
        return -1;
    }
    printf("FORMAT INFORMATION : \n");

    if (format.type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
    {
        printf("Buffer of a video capture stream \n");
    }
    printf("width of pixel: %d \n", format.fmt.pix.width);
    printf("height of pixel: %d \n", format.fmt.pix.height);

    if (format.fmt.pix.field == V4L2_FIELD_ANY)
    {
        printf("Field: V4L2_FIELD_ANY \n");
    }
    if (format.fmt.pix.colorspace == V4L2_COLORSPACE_JPEG)
    {
        printf("Pixel color space: V4L2_COLORSPACE_JPEG \n");
    }
    return 0;
}
/**********************************************************************************
 * @func    static int requestBuffer(int fd, struct v4l2_requestbuffers *reqbuff)
 * 
 * @brief   Allocate device buffers
 * @para    fd - file descriptor when open the device
 * @para    struct v4l2_requestbuffers - contain information about number of buffer, 
 *          type of memory.
 * @return  ERROR - ioctl VIDIOC_REQBUFS is failed
 * @return  INSUFFICENT_BUFF - not enough buffer
 * @return  0 - request buffer success
***********************************************************************************/
static int requestBuffer(int fd, struct v4l2_requestbuffers *reqbuff)
{
    int ret = 0;
    reqbuff->count = 4;
    reqbuff->memory = V4L2_MEMORY_MMAP;
    reqbuff->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    // CLEAR(reqbuff->reserved);

    ret = ioctl(fd, VIDIOC_REQBUFS, reqbuff);
    if (ret < 0)
    {
        printf("Requesting bufer failed  \n");
        return IOCTL_ERROR;
    }
    printf("buffer count: %d \n", reqbuff->count);

    if (reqbuff->count < 2)
    {
        printf("Insufficient buffer memory on device \n");
        ret = -1;
        return INSUFFICENT_BUFF;
    }

    printf("Requesting buffer success \n");
    return ret;
}
/**********************************************************************************
 * @func    static void init_mmap_method(int fd)
 * 
 * @brief   Initialize to use MMAP method to exchange data between user space and 
 *          kernel space 
 * @param   fd: file descriptor when open the device
 * @return  ERROR - ioctl VIDIOC_QUERYBUF is failed
 * @return  MAP_FAILED - mapping memory address between user space and kernel space 
 *          is failed  
/**********************************************************************************/
static void init_mmap_method(int fd)
{
    int ret =0 ;
    struct v4l2_requestbuffers reqbuff;
    requestBuffer(fd, &reqbuff);
    printf("buffer count : %d \n", reqbuff.count);
    buffers = (buffer *)calloc(reqbuff.count, sizeof(*buffers));
    if (buffers == NULL)
    {
        printf("Allocation memory failed \n");
    }

    for (n_buffers = 0; n_buffers < reqbuff.count; n_buffers++)
    {
        struct v4l2_buffer buf;
        CLEAR(buf);

        buf.memory = V4L2_MEMORY_MMAP;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.index = n_buffers;

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == IOCTL_ERROR)
        {
            printf("query buffer failed %d \n", n_buffers);
        }
        buffers[n_buffers].length = buf.length;
        printf("Buffer offset : %d \n", buf.m.offset);
        printf("Buffer length: %zu \n", buffers[n_buffers].length);
        buffers[n_buffers].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (buffers[n_buffers].start == MAP_FAILED)
        {
            printf("Mapping memory failed: %d \n", n_buffers);
            ret = MEM_MAP_FAILED;
        }
        printf("Mapping success %d \n", n_buffers);
    }
}

/**********************************************************************************
 * @func    static int enumFormat(int fd)
 * 
 * @brief   Enumerate format of the device
 * @param   fd - file descriptor when open the device
 * @return  ERROR - ioctl VIDIOC_ENUM_FMT is failed
 * @return  0 - enumerate format success
 
***********************************************************************************/
static int enumFormat(int fd)
{
    struct v4l2_fmtdesc formatCap;
    int ret;
    formatCap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    formatCap.index = 0;
    ret = 0;

    ret = ioctl(fd, VIDIOC_ENUM_FMT, &formatCap);
    printf("------------------> Format cap %d <-------------------- \n", formatCap.index);
    printf("flags : %d \n", formatCap.flags);
    // printf("Width: %d \n", formatCap.pixelformat)
    printf("Description : %s \n", formatCap.description);
    printf("Pixel format: %d \n", formatCap.pixelformat);
    return ret;
}

/**********************************************************************************
 * @func  static void deviceInit(int fd)
 * 
 * @brief: Initialize the camera device to get information of the device and set 
 *         some requirement depend on users
 * @param: fd - file descriptor when open the device
 * 
***********************************************************************************/
static void deviceInit(int fd)
{
    struct v4l2_capability caps;
    struct v4l2_format fmt;
    struct v4l2_input input;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;

    unsigned int index;
    unsigned int min;
    CLEAR(input);
    /**************************** Query and set up driver *************************/
    // printf capabilities of device
    printCapabilities(fd, caps);
    // enum format
    enumFormat(fd);
    // get input
    getInput(fd, &index);
    // enumerates input
    input.index = index;
    enumInput(fd, input);
    // set input
    setInput(fd, index);
    /*************************** Negotiate format with driver *************************/
    // set format
    //setFormat(fd, fmt);
    // get format
    //getFormat(fd, format);
    // enumerate format
    CLEAR(fmt);

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    fmt.fmt.pix.width = 640;                     //replace
    fmt.fmt.pix.height = 480;                    //replace
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV; //replace
    // printf("pixel format before set: %d \n", fmt.fmt.pix.pixelformat);
    fmt.fmt.pix.colorspace = V4L2_COLORSPACE_DEFAULT;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    
    if (-1 == ioctl(fd, VIDIOC_S_FMT, &fmt))
    {
        printf("Set format failed \n");
    }

    /* Note VIDIOC_S_FMT may change width and height. */
    
    /* Preserve original settings as set by v4l2-ctl for example */
    if (-1 == ioctl(fd, VIDIOC_G_FMT, &fmt))
    {
        printf("Get format failed \n");
    }
    printf("width of pixel: %d \n", fmt.fmt.pix.width);
    printf("height of pixel: %d \n", fmt.fmt.pix.height);

    printf("Field: %d \n", fmt.fmt.pix.field);
    printf("Pixel format %d: \n", fmt.fmt.pix.pixelformat);
    printf("Colorspace: %d \n", fmt.fmt.pix.colorspace);

    switch (io)
    {
    case IO_METHOD_READ:
        // init_read(fmt.fmt.pix.sizeimage);
        break;

    case IO_METHOD_MMAP:
        init_mmap_method(fd);
        break;

    case IO_METHOD_USRPTR:
        break;
    }

    // init mapping method
    // init_mmap_method(fd);
    // init_read(fmt.fmt.pix.sizeimage);
}

static int startCapturing(int fd)
{
    unsigned int i;
    int ret;
    enum v4l2_buf_type type;
    switch (io)
    {
    case IO_METHOD_READ:
    {
        printf("Using read method \n");
        break;
    }
    case IO_METHOD_USRPTR:
    {
        break;
    }
    case IO_METHOD_MMAP:
    {
        printf("MMAP method \n");
        for (i = 0; i < n_buffers; i++)
        {
            struct v4l2_buffer buf;
            CLEAR(buf);
            buf.memory = V4L2_MEMORY_MMAP;
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.index = i;

            if (ioctl(fd, VIDIOC_QBUF, &buf) < 0)
            {
                printf("queue buffer failed %d \n", i);
            }
        }
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(fd, VIDIOC_STREAMON, &type) < 0)
        {
            printf("Streaming on error \n");
        }
        printf("Start capturing \n");
        break;
    }
    }
    return ret;
}

static void stopCapturing(int fd)
{
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(fd, VIDIOC_STREAMOFF, &type) < 0)
    {
        printf("Stream off error \n");
    }
    printf("Stop capturing \n");
}
/*************************************   processImage  ******************************************
 * @desc: write frame data from memory mapped into raw files 
 * @para: fd: file descriptor when open the device
 * @para: size: the size of frame is wrote into raw file
 * 
*************************************************************************************************/
static void processImage(const void *pointer, int size)
{
    frame_number++;
    char filename[15];
    sprintf(filename, "frame-%d.raw", frame_number);
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL)
    {
        printf("Can not open file \n");
    }
    fflush(fp);
    fwrite(pointer, size, 1, fp);
    fclose(fp);
}
/*************************************     readFrame    ******************************************
 * @desc: read frames from queue 
 * @para: fd: file descriptor when open the device
 * 
*************************************************************************************************/
static int readFrame(int fd)
{
    struct v4l2_buffer buf;
    //unsigned int i;
    int ret = 0;
    switch (io)
    {
    case IO_METHOD_READ:
    {
        break;
    }
    case IO_METHOD_USRPTR:
    {
        break;
    }
    case IO_METHOD_MMAP:
    {
        CLEAR(buf);
        printf("Reading frame use mmap method \n");
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.reserved = 0;
        buf.reserved2 = 0;

        if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0)
        {
            printf("Dequeue buffer failed \n");
            ret = -1;
        }
        printf("ReadFrame: %d \n", buf.bytesused);
        assert(buf.index < n_buffers);
        processImage(buffers[buf.index].start, buf.bytesused);

        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0)
        {
            printf("Queue buffer failed \n");
            ret = -1;
        }
        break;
    }
    }
    return ret;
}

static void mainloop(int fd)
{
    unsigned int count;
    count = frame_count;
    while (count > 0)
    {
        for (;;)
        {
            fd_set SetofFileDescriptor;
            struct timeval timeout;
            int result;

            FD_ZERO(&SetofFileDescriptor);
            FD_SET(fd, &SetofFileDescriptor);

            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            result = select(fd + 1, &SetofFileDescriptor, NULL, NULL, &timeout);
            if (result < 0)
            {
                printf("Error in select function \n");
            }
            else if (result == 0)
            {
                printf("Time out \n");
            }
            else
            {
                readFrame(fd);
                break;
            }
        }
        count--;
    }
}

static void deviceUninit()
{
    unsigned int i;
    switch (io)
    {
    case IO_METHOD_READ:
    {
        free(buffers[0].start);
        break;
    }
    case IO_METHOD_USRPTR:
    {
        for (i = 0; i < n_buffers; ++i)
            free(buffers[i].start);
        break;
    }
    case IO_METHOD_MMAP:
    {
        for (i = 0; i < n_buffers; i++)
        {
            if (munmap(buffers[i].start, buffers[i].length) < 0)
            {
                printf("Unmap failed %d \n", i);
            }
        }
        break;
    }
    }
    free(buffers);
    printf("Device is de init \n");
}

