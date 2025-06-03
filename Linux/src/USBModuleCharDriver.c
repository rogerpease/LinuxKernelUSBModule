#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/workqueue.h>
#include <linux/string.h> 
#include <linux/usb/cdc.h> 

// Sources/references: 
//
// https://android.googlesource.com/kernel/msm/+/android-lego-6.0.1_r0.6/drivers/i2c/busses/i2c-tiny-usb.c

//
// How I built this:
//
//    Get a Seeeduino Xiao SAMD21 (about $10 on Amazon). 
//    Install Arduino GUI and, with it, install AdaFruit TinyUSB Library 3.6.0
//    Make sure your Arduino GUI sees your Xiao device and its COM or /dev/tty port. 
//    File -> Examples -> AdaFruit TinyUSB Library (may be towards bottom) -> Vendor -> i2c_tiny_usb_adapter
//    Flash the device. 
//    Connect the device to your linux host. 
// 
//    linux% insmod usbmon 
//    linux% cat /sys/kernel/debug/usb/usbmon/3u 
//    linux% i2cdetect /dev/i2c-0        should scan the I2C bus of the Xiao (might try connecting an LCD1602 to give it something to connect to). 
//    From the trace from above you should see lines like  

// ffff9e00c0fb4180 1657276607 S Ci:3:008:0 s c1 01 0000 0000 0004 4 <
// ffff9e00c0fb4180 1657277850 C Ci:3:008:0 0 4 = 0100ff8e
// ffff9e00c0fb4180 1658688197 S Co:3:008:0 s 41 07 0000 0008 0000 0       Checks i2c a 
// ffff9e00c0fb4180 1659084781 C Co:3:008:0 0 0
// ffff9e00c0fb4180 1659084918 S Ci:3:008:0 s c1 03 0000 0000 0001 1 <
// ffff9e00c0fb4180 1659086047 C Ci:3:008:0 0 1 = 01
// ffff9e00c0fb4180 1659086236 S Co:3:008:0 s 41 07 0000 0009 0000 0
// ffff9e00c0fb4180 1659087182 C Co:3:008:0 0 0
// ffff9e00c0fb4180 1659087214 S Ci:3:008:0 s c1 03 0000 0000 0001 1 <
// ffff9e00c0fb4180 1659089366 C Ci:3:008:0 0 1 = 02
// 
//    In the i2c-tiny-usb Arduino program, after i2c_usb.begin() in setup() add:    TinyUSBDevice.setID(0xaaaa,0x0b0b);
//
//    Recompile and reflash. You may need to short the bootloader pins to force the Xiao into bootloader mode.
// 
//    Blacklist the i2c-tiny-usb driver with a file in /etc/modprobe.d 
//    linux% make clean; make 
//    linux% insmod USBModuleCharDriver 
//    Plug USB device in (or remove/plug in)
//    linux% lsusb           (and verify the aaaa:0b0b appears). 
//    linux% Test.py. 
//


#include <linux/usb.h> 

#define DRIVER_NAME "USBModuleCharDriver "
#define DEVICE_NAME "simpleusb"

#define USB_SKEL_VENDOR_ID  (0xaaaa)
#define USB_SKEL_PRODUCT_ID (0x0b0b)

#define COMMAND_LEN        (64)
#define RESULT_BUFFER_LEN  (256)

//
// In a "real" implementation I would of course build an array or linked list of these so 
// multiple users could manage their own i2c queries. 
// This would be worth doing for a shared resource like a GPU.  
// 

typedef struct { 

  int fh_index; 
  int minorDevice;
  int byteStartIndex;  

  char command[COMMAND_LEN];  

  char resultBuffer[RESULT_BUFFER_LEN];
  int resultBeginIndex; 
  int resultEndIndex; 


} MyFHPrivateData_t; 


typedef struct { 

    struct usb_device *dev; 
    char probed; 
    int fh_open; 
    spinlock_t lock; 

    struct class *my_class;
    int major_number; 
    struct device *my_device;

} my_usb_device_t; 

my_usb_device_t * my_usb_device; 




int setup_char_driver(void);
void destroy_char_driver(void);
static int charDriverFileOpen(struct inode *inode, struct file *filp);


static struct usb_device_id my_usb_table [] = {
        { USB_DEVICE(USB_SKEL_VENDOR_ID, USB_SKEL_PRODUCT_ID) },
        { }                      /* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, my_usb_table);

// A device can offer multiple interfaces. This is why we see two of them when 

#define MAX_PACKET_SIZE_INT (16) 
#define MAX_PACKET_SIZE (64) 

int issue_urb(char *,MyFHPrivateData_t *); 

//
// Gets called once per *interface* when a USB device is plugged in. 
//
static int my_usb_probe(struct usb_interface *interface, const struct usb_device_id *id) 
{
    if ((id->idVendor  == USB_SKEL_VENDOR_ID) && 
        (id->idProduct == USB_SKEL_PRODUCT_ID)) 
    { 

        if (my_usb_device->my_device == NULL) { 
            if (setup_char_driver() < 0)  
                pr_err("Could not set up char driver.");  
         }

        struct usb_host_interface *iface_desc;
        struct usb_endpoint_descriptor *epd;
        unsigned int i;

        struct usb_device *dev = interface_to_usbdev(interface); 
        my_usb_device->dev = dev;  

        iface_desc = interface->cur_altsetting;

        for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
  
       	     epd = &iface_desc->endpoint[i].desc;

             char *dir = NULL ; 
             if (usb_endpoint_dir_in(epd))      { dir = "IN";  } 
             if (usb_endpoint_dir_out(epd))     { dir = "OUT"; }

             char *type; 
             if (usb_endpoint_xfer_control(epd)) type = "ctrl";
             if (usb_endpoint_xfer_bulk(epd))    type = "bulk"; 
             if (usb_endpoint_xfer_isoc(epd))    type = "isoc"; 
             if (usb_endpoint_xfer_int(epd))     type = "int"; 
   
             pr_info("Endpoint %d %s %s",i,type,dir);  
             pr_info("  bLength %d  bDescriptorType %d",epd->bLength,epd->bDescriptorType);  
             pr_info("  bEndpointAddress %d bmAttributes %d wMaxPacketSize %d", epd->bEndpointAddress,  epd->bmAttributes,  epd->wMaxPacketSize);  
              
        } 

        my_usb_device->probed = 1; 
    }

 
    return 0; 
 
}

// Gets called when user application calls os.open

static int charDriverFileOpen(struct inode *inode, struct file *filp)
{
    int count = 0; 

    if (my_usb_device->fh_open) { pr_err(DRIVER_NAME "Device Already open"); return -ENOSPC; } 

    my_usb_device->fh_open = 1; 

    MyFHPrivateData_t * myFHPrivateData;
    
    myFHPrivateData = (MyFHPrivateData_t *) kzalloc(sizeof(MyFHPrivateData_t),GFP_KERNEL); 
    myFHPrivateData->minorDevice = iminor(inode); 
    myFHPrivateData->byteStartIndex = 0; 
    myFHPrivateData->resultBeginIndex = 0; 
    myFHPrivateData->resultEndIndex = 0; 

    filp->private_data = (void *) myFHPrivateData; 
  
    return 0;

}

// Gets called when user application calls os.close 

static int charDriverFileClose(struct inode *inode, struct file *filp)
{

    my_usb_device->fh_open = 0; 
    kfree(filp->private_data); 
    return 0;

}

// Gets called when user application calls os.read() from a system device they opened. 

ssize_t charDriverFileRead(struct file *filp, char * buf, size_t byteCount, loff_t * byteOffset)
{

    MyFHPrivateData_t * myFHPrivateData = filp->private_data;
    int bytesWrittenThisCall = 0; 
    char c; 

    while ((bytesWrittenThisCall < byteCount) && (myFHPrivateData->resultBeginIndex != myFHPrivateData->resultEndIndex))
    { 
        c = myFHPrivateData->resultBuffer[myFHPrivateData->resultBeginIndex++];
        if (myFHPrivateData->resultBeginIndex == RESULT_BUFFER_LEN) myFHPrivateData->resultBeginIndex = 0; 
        put_user(c,buf + bytesWrittenThisCall++); 
    }  

    *byteOffset += bytesWrittenThisCall;
    pr_info("charDriverFileRead() ended %d bytes written",bytesWrittenThisCall); 
    return bytesWrittenThisCall; 

}

// Gets called when user application calls os.write(). 
// This is where we issue commands to the device. 
// byteOffset is essentially a 'reminder' pointer in case a write happens at multiple points. 

ssize_t charDriverFileWrite (struct file *filp, 
                             const char * buf, size_t byteCount, loff_t * byteOffset)
{

    MyFHPrivateData_t * myFHPrivateData = filp->private_data;
    char c; 
    ssize_t bytesWritten = 0; 

    memset(myFHPrivateData->command,0,COMMAND_LEN);
    while ((bytesWritten < byteCount) && (bytesWritten < COMMAND_LEN))
    { 
        get_user(c,buf+bytesWritten); 
        myFHPrivateData->command[bytesWritten++] = c;
    }  

    issue_urb(myFHPrivateData->command,myFHPrivateData);

    return bytesWritten; 

}

// Table of things to do when the *character* device is open. 

static const struct file_operations my_cdev_fops = {
	.owner          = THIS_MODULE,
        .write          = charDriverFileWrite,  
        .read           = charDriverFileRead,  
	.open           = charDriverFileOpen,
	.release        = charDriverFileClose,

};


int setup_char_driver() {

   // Allocate a major device number. Otherwise everyone would use 228 or 240. 

   pr_err(DRIVER_NAME "Setting up char device"); 
   my_usb_device->major_number = register_chrdev(0, DEVICE_NAME, &my_cdev_fops);
   if (my_usb_device->major_number < 0)
   {
       pr_err("Failed to register char device"); 
       return my_usb_device->major_number; 
   }

   // Allocate a class so you can see the driver's information in /sys/class/DEVICE_NAME. 

   my_usb_device->my_class = class_create(DEVICE_NAME);

   if (IS_ERR(my_usb_device->my_class))
   {
       unregister_chrdev(my_usb_device->major_number,DEVICE_NAME); 
       pr_err(DRIVER_NAME "Failed to create class"); 
       return PTR_ERR(my_usb_device->my_class);  
   } 

   // Now create the actual minor device and entry in /dev. 
   pr_info(DRIVER_NAME"Creating device"); 

   my_usb_device->my_device = device_create(my_usb_device->my_class, 
                         NULL, MKDEV(my_usb_device->major_number,0), NULL, DEVICE_NAME);

   if (IS_ERR(my_usb_device->my_device)) {
        class_destroy(my_usb_device->my_class);
        my_usb_device->my_class = NULL;
        unregister_chrdev(my_usb_device->major_number, DEVICE_NAME);
        printk(KERN_ALERT "Failed to create device\n");
        return PTR_ERR(my_usb_device->my_device);
   }

   return 0;
} 

void destroy_char_driver() {

   if (my_usb_device->my_class == NULL) return; 
   
   device_destroy   (my_usb_device->my_class,MKDEV(my_usb_device->major_number,0)); 

   class_destroy    (my_usb_device->my_class);

   unregister_chrdev(my_usb_device->major_number, DEVICE_NAME);

   my_usb_device->my_class = NULL; 

}

static void my_usb_disconnect(struct usb_interface *interface)
{
    destroy_char_driver(); 
}

static struct usb_driver skel_driver = {
        .name        = "Roger",
        .probe       = my_usb_probe,
        .disconnect  = my_usb_disconnect,
        .suspend     = NULL,
        .resume      = NULL,
        .pre_reset   = NULL,
        .post_reset  = NULL,
        .id_table    = my_usb_table,
        .supports_autosuspend = 1,
};

int issue_urb(char * usbmonline,MyFHPrivateData_t *myFHPrivateData)
{

    char * data; 
    if (!my_usb_device->probed) { pr_err(DRIVER_NAME "Not registered"); return 0; }

    char sendCallBack = usbmonline[0]; 
    char urbType      = usbmonline[2]; 
    char urbDir       = usbmonline[3]; 
    char urbignore[5]; 
    char setup; 

    int bus           = 0;
    int device        = 0;
    int request       = 0;
    int requesttype   = 0;
    int requestvalue  = 0;
    int requestindex  = 0;
    int requestlength = 0;
    int endpoint      = 0; 

//                            v Write starting here 
//                              
//ffff9e00c0fb4180 1657276607 S Ci:3:008:0 s c1 01 0000 0000 0004 4 <
//ffff9e00c0fb4180 1657277850 C Ci:3:008:0 0 4 = 0100ff8e


    if ((sendCallBack == 'S') && (urbType == 'C'))
    {
        // https://docs.kernel.org/usb/usbmon.html
        sscanf(usbmonline+2,"%c%c:%d:%d:%d "
                            "%c %x %d %d %d %d",
                                  &urbignore[0],&urbignore[1],&bus,&device,&endpoint,
                                  &setup,&requesttype,&request,&requestvalue,&requestindex,&requestlength); 
    }
    else
        pr_err(DRIVER_NAME " Unimplemented %s",usbmonline);
    
    // usb_control_msg(struct usb_device *dev, unsigned int pipe, __u8 request,
    //     __u8 requesttype, __u16 value, __u16 index, void *data,
    //     __u16 size, int timeout)

    pr_debug(DRIVER_NAME "%c %c%c ep%d req:%d reqtype:%x requestvalue:%d rindex:%d",sendCallBack,urbType,urbDir,endpoint,request,requesttype,requestvalue,requestindex); 
    
    int status; 
    int pipe;

    if      ((urbType == 'C') && (urbDir == 'i')) pipe = usb_rcvctrlpipe(my_usb_device->dev,endpoint); 
    else if ((urbType == 'C') && (urbDir == 'o')) pipe = usb_sndctrlpipe(my_usb_device->dev,endpoint); 
    else { pr_err(DRIVER_NAME "unknown urb and command %c %c: {r|w} c ",urbType,urbDir); return 1; }  

    data = kzalloc(64,GFP_KERNEL);
    if (!data) pr_err(DRIVER_NAME "TRY URBs data not allocated"); 

    status = usb_control_msg(my_usb_device->dev, pipe, request,  requesttype, requestvalue, requestindex,data,requestlength,2000); 
    
    unsigned int datacpu = be32_to_cpu((unsigned int) *((unsigned int *) data));
   
    char fmt[10];
    sprintf(fmt,"0x%%%d.%dx\n",requestlength*2,requestlength*2); 
    char result[40];
    int resultlen = sprintf(result,(const char *) fmt,datacpu);

    for (int i = 0; i < resultlen;i++) 
    {

       if (((myFHPrivateData->resultEndIndex+1)%RESULT_BUFFER_LEN) == myFHPrivateData->resultBeginIndex) break; 
       myFHPrivateData->resultBuffer[myFHPrivateData->resultEndIndex] = result[i];
       myFHPrivateData->resultEndIndex = (myFHPrivateData->resultEndIndex+1) % RESULT_BUFFER_LEN; 

    }
    pr_info("USB Bulk msg returned status %d resultdata %x resultlen %d",status,datacpu,resultlen); 

    kfree(data); 

    if (status == 0) {return 1;} else return 0; 
}


static int __init my_module_init(void)
{

    // Really not sure why but kzalloc is not zeroing-out things. 
    my_usb_device = (my_usb_device_t *) kmalloc(sizeof(my_usb_device),GFP_KERNEL); 
 
    if (my_usb_device == NULL)
    {
       pr_info("myusb allocation error");  
       return -ENOMEM; 
    } 

    my_usb_device->fh_open           = 0; 
    my_usb_device->probed            = 0; 
    my_usb_device->my_class          = NULL; 
    my_usb_device->my_device         = NULL; 

    spin_lock_init(&my_usb_device->lock);

    int result = usb_register(&skel_driver);

    if (result < 0) {
        pr_err("usb_register failed for the %s driver. Error number %d\n",
                       skel_driver.name, result);
        return -1;
    }


    pr_info("USBModule: my_module_init done"); 
    return 0; 

}


static void __exit my_module_exit(void)
{

   if (my_usb_device->my_class != NULL) 
       destroy_char_driver();

   usb_deregister(&skel_driver);

   kfree(my_usb_device);

}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Roger Pease");
MODULE_DESCRIPTION("Roger's General Kernel Module");


