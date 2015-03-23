#include <linux/interrupt.h>
#include <linux/hdreg.h>
#include <linux/cdrom.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/scatterlist.h>
#include <linux/bitmap.h>
#include <linux/list.h>

#include <xen/xen.h>
#include <xen/xenbus.h>
#include <xen/grant_table.h>
#include <xen/events.h>
#include <xen/page.h>
#include <xen/platform_pci.h>

#include <xen/interface/grant_table.h>
#include <xen/interface/io/protocols.h>

#include <asm/xen/hypervisor.h>

struct simplefront_info
{
	struct xenbus_device 	*dev;
};

static int simplefront_probe(struct xenbus_device *dev,
                          const struct xenbus_device_id *id)
{
	struct simplefront_info *info;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		xenbus_dev_fatal(dev, -ENOMEM, "allocating info structure");
		return -ENOMEM;
	}

	info->dev = dev;
	dev_set_drvdata(&dev->dev, info);
	printk("jintack %s is called. awesome\n", __func__);
	return 0;
}


static int simplefront_remove(struct xenbus_device *xbdev)
{
	return 0;
}

static int simplefront_resume(struct xenbus_device *dev)
{
	return 0;
}

/**
 * Callback received when the backend's state changes.
 */
static void simpleback_changed(struct xenbus_device *dev,
                            enum xenbus_state backend_state)
{
	struct simplefront_info *info = dev_get_drvdata(&dev->dev);
	int err;
	printk("jintack simpleback changed to state %d\n", backend_state);
	printk("jintack simplefront state is %d\n", dev->state);

	switch (backend_state) {
	
	case XenbusStateConnected:
		printk("jintack before front switch\n");
		err = xenbus_switch_state(info->dev, XenbusStateConnected);
		printk("jintack after front switch\n");
		if (err) {
			xenbus_dev_fatal(dev, err, "%s: switching to Connected state",
					dev->nodename);
			printk("jintack front is NOT connected: %d\n", err);
		} else
			printk("jintack [front] IS connected\n");
		return;
	default:
		return;
	}

}

static int simplefront_is_ready(struct xenbus_device *dev)
{
	return 0;
}
static const struct xenbus_device_id simplefront_ids[] = {
        { "vsimple" },
        { "" }
};

static struct xenbus_driver simplefront_driver = {
        .ids  = simplefront_ids,
        .probe = simplefront_probe,
        .remove = simplefront_remove,
        .resume = simplefront_resume,
        .otherend_changed = simpleback_changed,
        .is_ready = simplefront_is_ready,
};

static int __init xlsimple_init(void)
{
        int ret;
	printk("jintack %s is called. awesome\n", __func__);

        if (!xen_domain())
                return -ENODEV;

	printk("jintack %s is called second. awesome\n", __func__);
        ret = xenbus_register_frontend(&simplefront_driver);
        if (ret) {
		printk("jintack %s is called fail. awesome\n", __func__);
                return ret;
        }

	printk("jintack %s is called success. awesome\n", __func__);

        return 0;
}
module_init(xlsimple_init);


static void __exit xlsimple_exit(void)
{
        xenbus_unregister_driver(&simplefront_driver);
}
module_exit(xlsimple_exit);

MODULE_DESCRIPTION("Xen virtual simple device frontend");
MODULE_LICENSE("GPL");
