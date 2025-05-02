#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/workqueue.h>
#include <linux/string.h> 

#include <linux/usb.h> 

#define USB_SKEL_VENDOR_ID  (0x1234)
#define USB_SKEL_PRODUCT_ID (0x5678)

static struct usb_device_id skel_table [] = {
        { USB_DEVICE(USB_SKEL_VENDOR_ID, USB_SKEL_PRODUCT_ID) },
        { }                      /* Terminating entry */
};
MODULE_DEVICE_TABLE (usb, skel_table);

static int skel_probe(struct usb_interface *interface,
    const struct usb_device_id *id) 
{
  pr_info("Probe called %d %d",id->idVendor,id->idProduct); 
  if ((id->idVendor  == skel_table[0].idVendor) && (id->idProduct == skel_table[0].idProduct)) 
      return 0; 
  return -ENODEV;  

 
}

static struct usb_driver skel_driver = {
        .name        = "Roger",
        .probe       = skel_probe,
        .disconnect  = NULL,
        .suspend     = NULL,
        .resume      = NULL,
        .pre_reset   = NULL,
        .post_reset  = NULL,
        .id_table    = skel_table,
        .supports_autosuspend = 1,
};

static int __init my_module_init(void)
{
    int result = usb_register(&skel_driver);
    pr_info("USBModule: my_module_init called"); 
    if (result < 0) {
        pr_err("usb_register failed for the %s driver. Error number %d\n",
                       skel_driver.name, result);
        return -1;
    }

    return 0; 

}


static void __exit my_module_exit(void)
{

}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Roger Pease");
MODULE_DESCRIPTION("Roger's General Kernel Module");


