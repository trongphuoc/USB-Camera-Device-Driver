/*******************************************************************************
 *  INCLUDES
 ******************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/hardirq.h>
#include <linux/bug.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
//#include<linux/usb/storage.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h> // used v4l2 registration
#include <media/v4l2-common.h> // for displaying info
//#include<linux/videodev.h>
#include <linux/videodev2.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/slab.h>    // Used for kzalloc
#include <linux/uaccess.h> // get_user and put_user operations
#include <media/v4l2-ioctl.h>
#include <media/videobuf-core.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <asm-generic/ioctl.h>
#include <linux/version.h>
#include <media/videobuf-vmalloc.h>
#include <linux/vmalloc.h>
/*******************************************************************************
 *  DEFINE
 ******************************************************************************/
#define DRIVER_AUTHOR "TrongPhuoc"
#define DRIVER_DESC "The character device driver for camera"
#define DRIVER_VERSION "1.0"

#define MAX_BUFFER      32
#define MAX_BUFFER_SIZE 10
#define QUEUE_STREAMING (1 << 0)
#define DEVICE_NAME     "UVCCamera"
#define STATUS_OK        0
#define NULL_POINTER    -1
#define INVALID_VALUE   -1
/*******************************************************************************
 *  TYPEDEF
 ******************************************************************************/
typedef enum uvc_buffer_state
{
    UVC_BUF_STATE_IDLE = 0,     /**< Buffer in idle state */
    UVC_BUF_STATE_QUEUED = 1,   /**< Buffer is queued */
    UVC_BUF_STATE_READY = 2,    /**< Buffer is ready */
    UVC_BUF_STATE_DONE = 3,     /**< Buffer is done */
    UVC_BUF_STATE_ERROR = 4,    /**< Buffer is error */
} uvc_buffer_state;

// define memory type
typedef struct CamDevBuff_T
{
    struct list_head stream;

    struct videobuf_buffer *vb;
    struct v4l2_buffer buf;
    unsigned int vmaCount;
    wait_queue_head_t wait;

    struct cam_fmt *fmt;
    uvc_buffer_state buffState;

} CamDevBuff_T;

typedef struct UVC_cam_queue_T
{
    enum v4l2_buf_type buff_type;
    void *mem;
    unsigned int flag;
    unsigned int count;

    unsigned int buff_size;
    unsigned int buff_used;

    CamDevBuff_T buffer[MAX_BUFFER];
    struct mutex mutex;
    
} UVC_cam_queue_T;


// declare video device structure
typedef struct CameraDev_T
{
    struct v4l2_device *V4L2Dev;
    struct video_device *VDev;
    struct mutex mutex;
    struct videobuf_queue *vb_vidq;
    //CamDevBuff_T *CamBuff;
    UVC_cam_queue_T *queue;
    enum v4l2_buf_type type;

} CameraDev_T;

typedef enum cam_handle_state
{
    CAM_HANDLE_ACTIVE = 0,
    CAM_HANDLE_PASSIVE = 1,
} cam_handle_state;

typedef struct CamManage
{
    CameraDev_T *camDev;
    cam_handle_state camState;

} CamManage;

struct v4l2_format *fmt;
struct video_device *CameraDev;
struct usb_device *device;
struct list_head temp1;
struct list_head mainqueue;
unsigned int mem_size = 0;
unsigned int buf_size = 0;
unsigned int buf_count = 0;

//ssize_t BufferOffset[MAX_BUFFER_SIZE];
/*
 * VMA operations.
 */
static void my_vm_open(struct vm_area_struct *vma)
{
    CamDevBuff_T *buffer = vma->vm_private_data;
    buffer->vmaCount++;
}
  
static void my_vm_close(struct vm_area_struct *vma)
{
    CamDevBuff_T *buffer = vma->vm_private_data;
    buffer->vmaCount--;
}
  
static const struct vm_operations_struct my_vm_ops = {
    .open       = my_vm_open,
    .close      = my_vm_close,
};
/************************************************************************************
                                DEVICE FILE OPERATIONS
 ************************************************************************************/

//  declare struct v4l2_file_operations

/************************************************************************************
 * @func    int openCameraDevice(struct file *)
 * 
 * @brief   when application in user space use system call open(), this function will
 *          interact with device file in kernel space
 * @param   struct file*    - a pointer point to the device file is used by application
 * @return  STATUS_OK       - open device file success
 * @return  NULL_POINTER    - Allocate memory for device is failed
 * 
 ************************************************************************************/
int openCameraDevice(struct file *);

/************************************************************************************
 * @func    int releaseCameraDevice(struct file *)

 * 
 * @brief   when application in user space use system call close(), the driver will 
 *          handle this function
 * @param   struct file*    - a pointer point to the device file is used by application
 * 
 ************************************************************************************/
int releaseCameraDevice(struct file *);
ssize_t getFrame(struct file *, char __user *, size_t, loff_t *);

int openCameraDevice(struct file *fileDesc)
{
    printk(KERN_INFO "Camera device is opened \n");
    struct CamManage *CamHandle;
    struct CameraDev_T *Stream;

    // Allocate memory for CamManage
    CamHandle = (CamManage *)kzalloc(sizeof(CamManage), GFP_KERNEL);
    // get driver data from file structure fileDesc
    Stream = video_drvdata(fileDesc);
    Stream->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    Stream->queue = (UVC_cam_queue_T *)kmalloc(sizeof(UVC_cam_queue_T), GFP_KERNEL);
    if (Stream->queue == NULL)
    {
        printk(KERN_INFO "Stream is null, cannot allocate memory \n");
        return NULL_POINTER;
    }

    mutex_init(&Stream->queue->mutex);
    INIT_LIST_HEAD(&mainqueue);
    Stream->queue->buff_type = Stream->type;

    CamHandle->camDev = Stream;
    CamHandle->camState = 0;
    fileDesc->private_data = CamHandle;

    return 0;
}
int releaseCameraDevice(struct file *fileDesc)
{
    printk(KERN_INFO "Camera device is closed \n");
    return 0;
}

static ssize_t my_read(struct file *fp,char __user *buff,size_t len,loff_t *off)
{
      
    CamManage *vfh=fp->private_data;
    printk(KERN_INFO "Erfolg : Now in reading module....");
    return videobuf_read_stream(vfh->camDev->vb_vidq, buff, len, off, 1,fp->f_flags & O_NONBLOCK);
    //printk(KERN_INFO "Erfolg : Addresses : %p,%p,%d",fp,buff,len);
       
    return 0;
}

/***********************************************************************************
 *                              IOCTL FUNCTIONS
 ***********************************************************************************/

/************************************************************************************
 * @func    int CameraDeviceQueryCaps(struct file *file, void *fh, 
 *                               struct v4l2_capability *v4l2_cap);
 * 
 * @brief   when application in user space use ioctl call VIDIOC_QUERYCAP, driver will
 *          fill all field of struct v4l2_capability to show informations of device  
 * @param   struct file*  - a pointer point to the device file is used by application
 * @param   fh
 * @param   v4l2_cap      - a pointer to struct v4l2_capability, driver will get this 
 *                        pointer from user space
 * @return  STATUS_OK     - the driver filled rest of field of struct v4l2_capability
 * 
 ************************************************************************************/
int CameraDeviceQueryCaps(struct file *file, void *fh, struct v4l2_capability *v4l2_cap);

/************************************************************************************
 * @func    int CameraDeviceEnumInput(struct file *file, void *fh
 *                                      ,struct v4l2_input *inp);

 * 
 * @brief   handle the ioctl VIDIOC_ENUM_INPUT with the value of index field of struct
 *          v4l2_input
 * @param   struct file*  - a pointer point to the device file is used by application
 * @param   fh
 * @param   inp           - a pointer to struct v4l2,driver will get this pointer from  
 *                          application in user space
 * @return  STATUS_OK     - the driver filled rest of field of struct v4l2_input
 * @return  EINVAL        - the index field of struct v4l2_input is invalid
 * 
 ************************************************************************************/
int CameraDeviceEnumInput(struct file *file, void *fh, struct v4l2_input *inp);

/************************************************************************************
 * @func    int CameraDeviceSetInput(struct file *file, void *fh, unsigned int i);
 * 
 * @brief   handle the ioctl VIDIOC_S_INPUT with the value of index
 * @param   struct file*  - a pointer point to the device file is used by application
 * @param   fh
 * @param   i             - index of camera device input
 * @return  STATUS_OK     
 * @return  EINVAL        - the index field of struct v4l2_input is invalid
 * 
 ************************************************************************************/
int CameraDeviceSetInput(struct file *file, void *fh, unsigned int i);

/************************************************************************************
 * @func    int CameraDeviceEnumFormat(struct file *file, void *fh, 
 *                                      struct v4l2_fmtdesc *format);
 * 
 * @brief   handle the ioctl VIDIOC_ENUM_FMT 

 * @param   struct file*  - a pointer point to the device file is used by application
 * @param   fh
 * @param   format        - a pointer to struct v4l2_fmtdesc
 * @return  STATUS_OK     - format have appropriate index and format type
 ************************************************************************************/
int CameraDeviceEnumFormat(struct file *file, void *fh, struct v4l2_fmtdesc *format);

int CameraDeviceSetFormat(struct file *file, void *fh, struct v4l2_format *format);
/************************************************************************************
 * @func    int CameraDeviceGetFormat(struct file *file, void *fh, 
 *                                    struct v4l2_format *format);
 * 
 * @brief   handle the ioctl VIDIOC_G_FMT in application from user space
 * @param   struct file*  - a pointer point to the device file is used by application
 * @param   fh
 * @param   format        - the driver will fill rest of field in this pointer
 * @return  STATUS_OK     
 * 
 ************************************************************************************/
int CameraDeviceGetFormat(struct file *file, void *fh, struct v4l2_format *format);


int CameraDeviceGetInput(struct file *file, void *fh, unsigned int *i);

/************************************************************************************
 * @func    int CameraDeviceRequestBuff(struct file *file, void *fh,
 *                               struct v4l2_requestbuffers *buffer);
 * 
 * @brief   handle the ioctl  VIDIOC_REQBUFS
 * @param   struct file*  - a pointer point to the device file is used by application
 * @param   fh
 * @param   buffer        - a pointer to struct v4l2_requestbuffers, driver will allocate
 *                          buffer following information of fields in this pointer
 * @return  STATUS_OK     - allocate buffer in kernel space success
 * @return  -1            - allocate buffer failed
 * 
 ************************************************************************************/
int CameraDeviceRequestBuff(struct file *file, void *fh, struct v4l2_requestbuffers *buffer);

/************************************************************************************
 * @func    int CameraDeviceQueryBuff(struct file *file, void *fh,
 *                                     struct v4l2_buffer *buffer);

 * 
 * @brief   handle the ioctl  VIDIOC_QUERYBUF, supply information of buffer to application
 *          in user space
 * @param   struct file*  - a pointer point to the device file is used by application
 * @param   fh
 * @param   buffer        - a pointer to struct v4l2_buffer, driver will fill rest of 
 *                          field in this pointer
 * @return  STATUS_OK     
 * 
 ************************************************************************************/
int CameraDeviceQueryBuff(struct file *file, void *fh, struct v4l2_buffer *buffer);

/************************************************************************************
 * @func    int CameraDeviceQueueBuff(struct file *file, void *fh,
 *                                    struct v4l2_buffer *buffer);
 * 
 * @brief   handle the ioctl  VIDIOC_QBUF, add seperate buffer to main queue
 * 
 * @param   struct file*  - a pointer point to the device file is used by application
 * @param   fh
 * @param   buffer        - a pointer to struct v4l2_buffer
 *                          
 * @return  STATUS_OK     
 * 
 ************************************************************************************/
int CameraDeviceQueueBuff(struct file *file, void *fh, struct v4l2_buffer *buffer);

/************************************************************************************
 * @func    int CameraDeviceDequeueBuff(struct file *file, void *fh,
 *                                       struct v4l2_buffer *buffer);

 * 
 * @brief   handle the ioctl  VIDIOC_DQBUF, get buffer from main queue to transfer to 
 *          user space
 * @param   struct file*  - a pointer point to the device file is used by application
 * @param   fh
 * @param   buffer        - a pointer to struct v4l2_buffer
 *                          
 * @return  STATUS_OK     
 ************************************************************************************/
int CameraDeviceDequeueBuff(struct file *file, void *fh, struct v4l2_buffer *buffer);

/************************************************************************************
 * @func    int CameraDeviceStreamOn(struct file *file, void *fh, 
 *                                      enum v4l2_buf_type type);
 * 
 * @brief   handle the ioctl  VIDIOC_STREAMON
 * @param   struct file*  - a pointer point to the device file is used by application
 * @param   fh
 * @param   type          - type of buffer memory
 *                          
 * @return  STATUS_OK     - type of buffer valid
 * @return  -1            - type of buffer is invalid 
 ************************************************************************************/
int CameraDeviceStreamOn(struct file *file, void *fh, enum v4l2_buf_type type);

/************************************************************************************
 * @func    int CameraDeviceStreamOff(struct file *file, void *fh, 
 *                                      enum v4l2_buf_type type);
 * 
 * @brief   handle the ioctl  VIDIOC_STREAMOFF
 * @param   struct file*  - a pointer point to the device file is used by application
 * @param   fh
 * @param   type          - type of buffer memory
 *                          
 * @return  STATUS_OK     - type of buffer valid
 * @return  -1            - type of buffer is invalid 
 ************************************************************************************/
int CameraDeviceStreamOff(struct file *file, void *fh, enum v4l2_buf_type type);


/*******************************************************************************
 * IOCTL FUNCTIONS
 ******************************************************************************/
int CameraDeviceQueryCaps(struct file *file, void *fh, struct v4l2_capability *v4l2_cap)
{
    printk(KERN_INFO "Query capabilities \n");

    strcpy(v4l2_cap->driver, "CameraDriver");
    strcpy(v4l2_cap->card, "CameraDev");

    v4l2_cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING |
                             V4L2_CAP_READWRITE | V4L2_CAP_VIDEO_OUTPUT;

    v4l2_cap->version = KERNEL_VERSION(3, 14, 29);

    v4l2_cap->reserved[0] = 0;
    v4l2_cap->reserved[1] = 0;
    v4l2_cap->reserved[2] = 0;
    v4l2_cap->reserved[3] = 0;

    return 0;
}

int CameraDeviceEnumInput(struct file *file, void *fh, struct v4l2_input *inp)
{
    printk(KERN_INFO "Enum input devices \n");
    if (inp->index == 1)
    {
        strcpy(inp->name, "Camera capture");
        inp->type = V4L2_INPUT_TYPE_CAMERA;

        inp->status = V4L2_IN_ST_NO_SIGNAL;

        inp->capabilities = V4L2_IN_CAP_STD;

        inp->std = 0;

        inp->reserved[0] = 0;
        inp->reserved[1] = 0;
        inp->reserved[2] = 0;
        inp->reserved[3] = 0;
        printk(KERN_INFO "Enum input success \n");
        return 0;
    }
    else
    {
        printk(KERN_INFO "Enum input failed \n");
        return EINVAL;
    }
}

int CameraDeviceSetInput(struct file *file, void *fh, unsigned int i)
{
    if (i == 1)
    {
        printk(KERN_INFO "Device has been set \n");
    }
    else
    {
        printk(KERN_INFO "Invalid ID of device \n");
        return INVALID_VALUE;
    }

    return 0;
}

int CameraDeviceGetInput(struct file *file, void *fh, unsigned int *index)
{
    struct v4l2_input input;
    input.index = 1;
    *(index) = input.index;
    printk(KERN_INFO "Getting input success, index: %d \n ", *(index));
}
int CameraDeviceEnumFormat(struct file *file, void *fh, struct v4l2_fmtdesc *format)
{
    if (format->index == 1 && format->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
    {
        format->flags = V4L2_FMT_FLAG_COMPRESSED;
    }
    return STATUS_OK;
}

int CameraDeviceSetFormat(struct file *file, void *fh, struct v4l2_format *v4l2_fmt)
{
    fmt = (struct v4l2_format *)kmalloc(sizeof(struct v4l2_format), GFP_KERNEL);
    if (fmt == NULL)
    {
        printk(KERN_INFO "CameraDeviceSetFormat: Can not set format for fmt \n");
        return -1;
    }
    fmt->type = v4l2_fmt->type;
    fmt->fmt.pix.height = v4l2_fmt->fmt.pix.height;
    fmt->fmt.pix.width = v4l2_fmt->fmt.pix.width;
    fmt->fmt.pix.field = v4l2_fmt->fmt.pix.field;
    fmt->fmt.pix.pixelformat = v4l2_fmt->fmt.pix.pixelformat;
    fmt->fmt.pix.colorspace = v4l2_fmt->fmt.pix.colorspace;
    printk(KERN_INFO "Set format successfully: %d \n", v4l2_fmt->type);
    return 0;
}
int CameraDeviceGetFormat(struct file *file, void *fh, struct v4l2_format *format)
{
    fmt->type = 1; // V4L2_BUF_TYPE_VIDEO_OUTPUT
    fmt->fmt.pix.width = 640;
    fmt->fmt.pix.height = 480;
    fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_BGR24;
    fmt->fmt.pix.field = V4L2_FIELD_ANY;
    fmt->fmt.pix.sizeimage = (fmt->fmt.pix.width * fmt->fmt.pix.height);
    fmt->fmt.pix.colorspace = V4L2_COLORSPACE_JPEG;

    *(format) = *(fmt);
    return STATUS_OK;
}
int CameraDeviceRequestBuff(struct file *file, void *fh, struct v4l2_requestbuffers *buffer)
{
    int size;
    int count;
    void *mem1;
    int vid_limit = 16;
    int i;
    CamManage *Cam = file->private_data;
    CameraDev_T *stream;
    stream = (CameraDev_T *)kmalloc(sizeof(CameraDev_T), GFP_KERNEL);
    if (stream == NULL)
    {
        printk(KERN_INFO "REQUEST BUFF: Cannot allocate memory for stream \n ");
        return -1;
    }
    stream = Cam->camDev;

    mutex_init(&stream->mutex);
    mutex_init(&stream->queue->mutex);
    INIT_LIST_HEAD(&mainqueue);

    stream->queue->buff_type = buffer->type;

    printk(KERN_INFO "Buffer infor: size of buffer: %d \n", buffer->count);
    printk(KERN_INFO "Buffer infor: memory: %d \n", buffer->memory);
    printk(KERN_INFO "Buffer infor: type of buffer: %d \n", buffer->type);
    printk(KERN_INFO "Requesting buffer \n");

    if (buffer->type != stream->type || buffer->memory != V4L2_MEMORY_MMAP)
    {
        printk(KERN_INFO "REQUEST BUFF: Different kind of buffer or memory method \n");
        return -EINVAL;
    }
    mutex_lock(&stream->mutex);
    printk(KERN_INFO "REQUEST BUFF: lock on mutex stream... \n");

    mutex_lock(&stream->queue->mutex);
    printk(KERN_INFO "REQUEST BUFF: lock on mutex queue... \n");

    size = 640 * 480*3;
    count = buffer->count;
    while (size * count > vid_limit * 1024 * 1024)
    {
        (count)--;
    }
    size = PAGE_ALIGN(size);


    mem1 = (void*)vmalloc_32(count*size);
    if (mem1 == NULL)
    {
        printk(KERN_INFO "REQUEST BUFF: Allocate memory failed \n");
        mutex_unlock(&stream->queue->mutex);
        mutex_unlock(&stream->mutex);
        return -1;
    }

    stream->queue->mem = mem1;
    // mem_size = (unsigned int)mem1;
    // printk(KERN_INFO "stream->queue->mem=%d \n", (int)mem1);
    // printk(KERN_INFO "stream->queue->mem=%p \n",(int)mem1);
    for (i = 0; i < buffer->count; i++)
    {
        memset(&stream->queue->buffer[i], 0, sizeof(stream->queue->buffer[i]));
        stream->queue->buffer[i].buf.index = i;
        stream->queue->buffer[i].buf.m.offset = i * size;
        printk(KERN_INFO "REQUEST BUFF: buffer offset %d \t i=%d \n",stream->queue->buffer[i].buf.m.offset,i);
        stream->queue->buffer[i].buf.length = size;
        stream->queue->buffer[i].buf.type = stream->queue->buff_type;
        stream->queue->buffer[i].buf.field = V4L2_FIELD_NONE;
        stream->queue->buffer[i].buf.memory = V4L2_MEMORY_MMAP;
        stream->queue->buffer[i].buf.flags = 0; // V4L2_BUF_FLAG_QUEUED
        stream->queue->buffer[i].buffState = UVC_BUF_STATE_IDLE;
        //BufferOffset[i] = stream->queue->buffer[i].buf.m.offset;
         //init_waitqueue_head(&stream->queue->buffer[i].wait);
         INIT_LIST_HEAD(&stream->queue->buffer[i].stream);
    }
    
    stream->queue->count = count;
    stream->queue->buff_size = size;
    buffer->count = count;

    buf_count = count;
    buf_size = size;

    mutex_unlock(&stream->queue->mutex);
    printk(KERN_INFO "REQUEST BUFF: Unlock on mutex queue... \n");
    mutex_unlock(&stream->mutex);
    printk(KERN_INFO "REQUEST BUFF: Unlock on mutex stream... \n");

    return 0;
}
// Query the status of buffer after allocted with the REQUESTBUFF ioctl function
// Define the location of buffer in the kernel space

int CameraDeviceQueryBuff(struct file *file, void *fh, struct v4l2_buffer *buffer_query)
{
    CamManage *Cam = file->private_data;
    CameraDev_T *Stream;
    Stream = Cam->camDev;
    CamDevBuff_T *buff;

    mutex_init(&Stream->queue->mutex);
    buff = (CamDevBuff_T *)kmalloc(sizeof(CamDevBuff_T), GFP_KERNEL);
    if (buff == NULL)
    {
        printk(KERN_INFO "Cannot allocated memory \n");
        return -1;
    }
    buff = &Stream->queue->buffer[buffer_query->index];
    if (buffer_query->index > Stream->queue->count)
    {
        printk(KERN_INFO "Invalid index \n");
        return -1;
    }
    printk(KERN_INFO "Querying buffer...... \n");
    mutex_lock(&Stream->queue->mutex);

    memcpy(buffer_query, &buff->buf, sizeof(struct v4l2_buffer));

    switch (buff->buffState)
    {
    case UVC_BUF_STATE_DONE:
    {
        buffer_query->flags |= V4L2_BUF_FLAG_DONE;
        break;
    }
    case UVC_BUF_STATE_READY:
    case UVC_BUF_STATE_QUEUED:
    {
        buffer_query->flags |= V4L2_BUF_FLAG_QUEUED;
        break;
    }
    }

    mutex_unlock(&Stream->queue->mutex);
    printk(KERN_INFO "Query buffer successful ...... \n");
    return 0;
}

int CameraDeviceQueueBuff(struct file *file, void *fh, struct v4l2_buffer *buff)
{
    CamManage *Cam = file->private_data;
    CameraDev_T *Stream;
    Stream = Cam->camDev;

    CamDevBuff_T *buf;
    int ret = 0;
    buf = (CamDevBuff_T *)kmalloc(sizeof(CamDevBuff_T), GFP_KERNEL);
    if (buf == NULL)
    {
        printk(KERN_INFO "Cannot allocated buf memory \n");
        ret = -1;
        return ret;
    }
    mutex_init(&Stream->queue->mutex);
    
    Stream->queue->buff_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    mutex_lock(&Stream->queue->mutex);
    printk(KERN_INFO "Lock on queue... \n");

    buf = &Stream->queue->buffer[buff->index];
    
    INIT_LIST_HEAD(&buf->stream);
   // INIT_LIST_HEAD(&buf->queue);
    INIT_LIST_HEAD(&mainqueue);
    INIT_LIST_HEAD(&temp1);
    // INIT_LIST_HEAD(&buf->)
    

    printk(KERN_INFO "Init list buf->stream \n");

    buf->buffState = UVC_BUF_STATE_DONE;

    list_add_tail(&buf->stream, &mainqueue);
    temp1=mainqueue;
    printk(KERN_INFO "Adding in main queue \n");

    mutex_unlock(&Stream->queue->mutex);
    printk(KERN_INFO "Unlock on queue \n");

    return STATUS_OK;
}

int CameraDeviceDequeueBuff(struct file *file, void *fh, struct v4l2_buffer *buffer)
{
    CamManage *Cam = file->private_data;
    CameraDev_T *Stream;
    Stream = Cam->camDev;
    int ret = 0;
    CamDevBuff_T *buff;
    buff = (CamDevBuff_T *)kmalloc(sizeof(CamDevBuff_T), GFP_KERNEL);
    if (buff == NULL)
    {
        ret = -1;
        printk(KERN_INFO "Cannot allocate memory for buff \n");
        return ret;
    }
    
    mutex_init(&Stream->queue->mutex);
    mutex_lock(&Stream->queue->mutex);

    

    // INIT_LIST_HEAD(&Stream->queue->mainqueue);
    printk(KERN_INFO "DEQUEUE: Lock on mutex \n");
    
    if (list_empty(&mainqueue))
    {
        printk(KERN_INFO "DEQUEUE: queue is empty \n");
        mainqueue = temp1;
    }
    
    buff = list_first_entry(&mainqueue, CamDevBuff_T, stream);
    printk(KERN_INFO "Buffer state in DQBUFF--%d buffer index %d \n", buff->buffState, buff->buf.index);
    
    switch (buff->buffState)
    {
    case UVC_BUF_STATE_ERROR:
    {
        printk(KERN_INFO "DEQUEUE: Buffer state error \n");
        ret = -EIO;
        mutex_unlock(&Stream->queue->mutex);
        printk(KERN_INFO "DEQUEUE : Unlock on mutex \n");
        return ret;
    }
    case UVC_BUF_STATE_DONE:
    {
        printk(KERN_INFO "DEQUEUE: Buffer state done \n");
        buff->buffState = UVC_BUF_STATE_IDLE;
        break;
    }
    case UVC_BUF_STATE_QUEUED:
    case UVC_BUF_STATE_READY:
    default:
    {
        printk(KERN_INFO "DEQUEUE: Buffer state queued or ready \n");
        ret = -1;
        mutex_unlock(&Stream->queue->mutex);
        printk(KERN_INFO "DEQUEUE : Unlock on mutex \n");
    }
    }
    
    
    // memcpy(buffer, &buff->buf, sizeof(struct v4l2_buffer));
    *buffer = buff->buf;
    //list_del(&buff->stream);
    // buffer->bytesused = buff->buf.bytesused;
    printk(KERN_INFO "DEQUEUE: bytesused: %d \n", buff->buf.bytesused);
    /*
    switch (buff->buffState)
    {
    case UVC_BUF_STATE_DONE:
    {
        buffer->flags |= V4L2_BUF_FLAG_DONE;
        break;
    }
    case UVC_BUF_STATE_QUEUED:
    case UVC_BUF_STATE_READY:
    {
        buffer->flags |= V4L2_BUF_FLAG_QUEUED;
        break;
    }
    default:
    {
        break;
    }
    }
    */
    mutex_unlock(&Stream->queue->mutex);
    printk(KERN_INFO "DEQUEUE : Unlock on mutex \n");
    return ret;
}

int CameraDeviceStreamOn(struct file *file, void *fh, enum v4l2_buf_type type)
{
    CamManage *Cam = file->private_data;
    CameraDev_T *Stream;
    Stream = Cam->camDev;
    Stream->queue = (UVC_cam_queue_T *)kmalloc(sizeof(UVC_cam_queue_T), GFP_KERNEL);
    mutex_init(&Stream->queue->mutex);
    printk(KERN_INFO "In stream on : %d", Stream->type);

    if (type != Stream->type)
    {
        printk(KERN_INFO "Invalid type of streaming on \n");
        return -1;
    }
    mutex_lock(&Stream->mutex);
    printk(KERN_INFO " STREAM ON :  mutex lock \n");

    mutex_lock(&Stream->queue->mutex);
    printk(KERN_INFO "STREAM ON: Mutex lock on stream->queue \n");

    Stream->queue->flag |= QUEUE_STREAMING;
    printk(KERN_INFO "STREAM ON: Flags changed \n");

    mutex_unlock(&Stream->queue->mutex);
    printk(KERN_INFO "STREAM ON: Mutex unlock on Stream->queue \n");

    mutex_unlock(&Stream->mutex);
    printk(KERN_INFO "STREAM ON: Mutex unlock on Stream \n");
    return 0;
}

int CameraDeviceStreamOff(struct file *file, void *fh, enum v4l2_buf_type type)
{
    CamManage *Cam = file->private_data;
    CameraDev_T *Stream;
    Stream = Cam->camDev;

    Stream->queue = (UVC_cam_queue_T *)kmalloc(sizeof(UVC_cam_queue_T), GFP_KERNEL);

    mutex_init(&Stream->queue->mutex);
    printk(KERN_INFO "In stream off \n");

    if (type != Stream->type)
    {
        printk(KERN_INFO " Invalid type of stream of \n");
        return -1;
    }
    mutex_lock(&Stream->mutex);
    printk(KERN_INFO " STREAM OFF :  mutex lock \n");

    mutex_lock(&Stream->queue->mutex);
    printk(KERN_INFO "STREAM OFF: Mutex lock on stream->queue \n");

    Stream->queue->flag = ~QUEUE_STREAMING;

    mutex_unlock(&Stream->queue->mutex);
    printk(KERN_INFO "STREAM OFF: Mutex unlock on stream->queue \n");

    mutex_unlock(&Stream->mutex);
    printk(KERN_INFO "STREAM OFF: Mutex unlock on stream \n");
    return 0;
}

static struct v4l2_ioctl_ops ioctl_operation =
{
        .vidioc_querycap    = CameraDeviceQueryCaps,
        .vidioc_s_input     = CameraDeviceSetInput,
        .vidioc_enum_input  = CameraDeviceEnumInput,
        .vidioc_g_input     = CameraDeviceGetInput,
        .vidioc_s_fmt_vid_cap = CameraDeviceSetFormat,
        .vidioc_g_fmt_vid_cap = CameraDeviceGetFormat,
        .vidioc_reqbufs     = CameraDeviceRequestBuff,
        .vidioc_querybuf    = CameraDeviceQueryBuff,
        .vidioc_qbuf        = CameraDeviceQueueBuff,
        .vidioc_dqbuf       = CameraDeviceDequeueBuff,
        .vidioc_streamon    = CameraDeviceStreamOn,
        .vidioc_streamoff   = CameraDeviceStreamOff,

};
/************************************************************************************
 * @func    static int Mapper(struct UVC_cam_queue_T *queue, 
 *                          struct vm_area_struct *vmaStruct)
 * 
 * @brief   mapping buffer from kernel space to buffer in user space
 * 
 ************************************************************************************/
static int Mapper(struct UVC_cam_queue_T *queue, struct vm_area_struct *vmaStruct)
{
    struct CamDevBuff_T *buff;
    struct page *Page;
    unsigned long address, start, size;
    unsigned int i;
    int ret = 0;
    //queue = kmalloc(sizeof(UVC_cam_queue_T), GFP_KERNEL);
    buff = (CamDevBuff_T *)kmalloc(sizeof(CamDevBuff_T), GFP_KERNEL);
    if (buff == NULL)
    {
        ret = -1;
        printk(KERN_INFO "Can not allocated memory \n");
        return ret;
    }

    mutex_init(&queue->mutex);

    start = vmaStruct->vm_start;
    size = vmaStruct->vm_end - vmaStruct->vm_start;
    printk(KERN_INFO "Mapper: size = %d \n", size);
    mutex_lock(&queue->mutex);
    printk(KERN_INFO "Mutex lock on queue \n");

    vmaStruct->vm_flags |= VM_IO;
    printk(KERN_INFO "Mapper: vm_Start : %lu \t vm_end: %d  \t msize : %d \n", 
                vmaStruct->vm_start, vmaStruct->vm_end, (unsigned int)queue->mem);
    printk(KERN_INFO "Mapper: buffer count: %d \n", queue->count);
    printk(KERN_INFO "Page size: %lu \n ", PAGE_SIZE);

    for (i = 0; i < queue->count; i++)
    {
        buff = &queue->buffer[i];
        printk(KERN_INFO "mapper: buffer_offset ... %d  vma_pgoff : %lu",buff->buf.m.offset,vmaStruct->vm_pgoff);
        if ((buff->buf.m.offset >> PAGE_SHIFT) == vmaStruct->vm_pgoff)
        {
            printk(KERN_INFO "mapper: breaking out of for loop ... %d \n", i);
            break;
        }
    }

    address = (unsigned long)queue->mem + buff->buf.m.offset;
    printk(KERN_INFO "Mem location: %p \n", queue->mem);
    printk(KERN_INFO "Mem location: %d \n", queue->mem);

    while (size > 0)
    {
        Page = vmalloc_to_page((void *)address);
        if (Page == NULL)
        {
            printk(KERN_INFO "Mapper page is null \n");
            mutex_unlock(&queue->mutex);
            ret = -1;
            return ret;
        }
        ret = vm_insert_page(vmaStruct, start, Page);

        if (ret < 0)
        {
            printk(KERN_INFO "Insert page failed \n");
            mutex_unlock(&queue->mutex);
            ret = -1;
            return ret;
        }
        start += PAGE_SIZE;
        address += PAGE_SIZE;
        size -= PAGE_SIZE;
    }
    vmaStruct->vm_ops = &my_vm_ops;
    vmaStruct->vm_private_data = buff;
    my_vm_open(vmaStruct);
    mutex_unlock(&queue->mutex);
    printk(KERN_INFO "Unlock on queue mutex \n");

    return ret;
}

int MyMapper(struct file *fileDesc, struct vm_area_struct *vmaStruct)
{
    int ret;

    CamManage *Cam = (CamManage *)fileDesc->private_data;
    CameraDev_T *Stream = Cam->camDev;

    printk(KERN_INFO "in my mapperr...");
    if (vmaStruct == NULL)
    {
        printk(KERN_INFO "Vma is null \n");
        return 0;
    }
    ret = Mapper(Stream->queue, vmaStruct);
    return ret;
}

/************************************************************************************
                                 OS SPECIFICS
 ************************************************************************************/
static struct v4l2_device v4l2_device =
{
        .name = "CamDev",
};

static struct v4l2_file_operations v4l2_fops =
{
        .owner  = THIS_MODULE,
        .open   = openCameraDevice,
        // .read   = my_read,
        .release        = releaseCameraDevice,
        .unlocked_ioctl = video_ioctl2,
        .mmap           = MyMapper,
};

static struct video_device video_dev =
{
        .name = DEVICE_NAME,
        .vfl_dir = VFL_DIR_RX,
        .minor = -1,
        .fops = &v4l2_fops,
        .ioctl_ops = &ioctl_operation,
        .release = video_device_release,
        .v4l2_dev = &v4l2_device,
        .lock = NULL,
        .dev_parent = NULL,
};

static struct usb_device_id mydev_table[] = 
{
    {USB_DEVICE(0x1908, 0x2311)}, {}
    
};
MODULE_DEVICE_TABLE(usb, mydev_table);      /**< Register id of device with usb core*/

/************************************************************************************
 * @func    static void UVCCamDisconnect(struct usb_interface *interface)
 * 
 * 
 * @brief   this function is call when remove usb camera 
 * 
 ************************************************************************************/
static void UVCCamDisconnect(struct usb_interface *interface)
{
    //unregister and unallocate memory for device when unload kernel module
    printk(KERN_INFO "UVC device is removed \n");
    printk(KERN_INFO "Interface camera No.%d now is disconected \n", interface->cur_altsetting->desc.bInterfaceNumber);
    printk(KERN_INFO "Exit \n");
}

/************************************************************************************
 * @func    static void UVCCamDisconnect(struct usb_interface *interface)
 * 
 * 
 * @brief   Allocate memory and create device file in /dev/video*
 * 
 ************************************************************************************/
static int UVCCamProbe(struct usb_interface *interface, const struct usb_device_id *id)
{
    struct usb_host_interface *interfaceDesc;
    CameraDev_T *cam_dev;
    int ret;
    interfaceDesc = interface->cur_altsetting;
     device = kmalloc(sizeof(struct usb_device),GFP_KERNEL);
    if(device == NULL)
    {
        printk(KERN_INFO "UVCCamProbe: Allocate memory for usb device failed \n");
        ret = -1;
        return ret;
    }
    device = interface_to_usbdev(interface);
    cam_dev = kmalloc(sizeof(CameraDev_T), GFP_KERNEL);
    printk(KERN_INFO "Probe: UVC device (%04X, %04X) plugged \n", id->idVendor, id->idProduct);
    if(cam_dev == NULL)
    {
        ret = -1;
        printk(KERN_INFO "Can not allocate memory for cam_dev \n");
        return ret;
    }
    // register module with kernel
    CameraDev = video_device_alloc();
    if (CameraDev == NULL)
    {
        ret = -1;
        printk(KERN_INFO "Cannot allocate memory for device  !!! \n");
        kfree(cam_dev);
        return ret;
    }
    printk(KERN_INFO "Allocate memory for device success !!! \n");
    *CameraDev = video_dev;
    //CameraDev->dev = device->dev;

    ret = video_register_device(CameraDev, VFL_TYPE_GRABBER, 0);
    if (ret < 0)
    {
        ret = -1;
        printk(KERN_INFO "Cannot register video device \n");
        video_device_release(CameraDev);
        kfree(cam_dev);
        return ret;
    }

    video_set_drvdata(CameraDev, cam_dev); 
    cam_dev->VDev = CameraDev;
    CameraDev->dev.init_name = "USB_Cam_dev";
    printk(KERN_INFO "v4l2 device number: %d \n", CameraDev->num);

    ret = v4l2_device_register(&CameraDev->dev, CameraDev->v4l2_dev);
    if (ret < 0)
    {
        printk(KERN_INFO "v4l2 registation failed \n");
        video_device_release(CameraDev);
        kfree(cam_dev);
        return ret;
    }

    cam_dev->V4L2Dev = CameraDev->v4l2_dev;

    v4l2_info(CameraDev->v4l2_dev, "V4L2 registered as: %d \t %s \t %d \t %d \n", CameraDev->num, CameraDev->name,
              MAJOR(CameraDev->dev.devt), MINOR(CameraDev->dev.devt));
    printk(KERN_INFO " Camera interface no.  %d now probed: (%04X:%04X)\n",\
            interfaceDesc->desc.bInterfaceNumber, device->descriptor.idVendor,device->descriptor.idProduct);
    printk(KERN_INFO " Video device registered successfully !! \n");

    return ret;
}


static struct usb_driver USB_Driver= 
{
    .name       = "UVC driver",
    .probe      = UVCCamProbe,
    .disconnect = UVCCamDisconnect, 
    .id_table   = mydev_table,
};
// module init
static int __init cam_driver_init(void)
{
    
    int ret;
    ret = usb_register(&USB_Driver);
    if(ret < 0)
    {
        printk(KERN_INFO "Cannot register usb device \n");
        return ret;
    }
    printk(KERN_INFO "Register device success \n");
    return ret;
}
// module exit
static void __exit cam_driver_exit(void)
{
    usb_deregister(&USB_Driver);
    v4l2_device_unregister(CameraDev->v4l2_dev);
    video_unregister_device(CameraDev);
    video_device_release(CameraDev);
    printk(KERN_INFO "Exit \n");
}
module_init(cam_driver_init);
module_exit(cam_driver_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DRIVER_DESC);