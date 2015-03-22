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

static int simplefront_probe(struct xenbus_device *dev,
                          const struct xenbus_device_id *id)
{
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
