
#include <stdarg.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <xen/events.h>
#include <xen/grant_table.h>
#include "common.h"

struct backend_info {
        struct xenbus_device    *dev;
        struct xenbus_watch     backend_watch;
};

static void backend_changed(struct xenbus_watch *watch,
                            const char **vec, unsigned int len);
static int xen_simpleback_remove(struct xenbus_device *dev);

int __init xen_simpleif_interface_init(void)
{
/*
	xen_simpleif_cachep = kmem_cache_create("sampleif_cache",
						sizeof(struct xen_sampleif),
						0, 0, NULL);

	if (!xen_simpleif_cachep)
		return -ENOMEM;

*/
	return 0;
}

static const struct xenbus_device_id xen_simpleback_ids[] = {
        { "vsimple" },
        { "" }
};

/*
 * Entry point to this code when a new device is created.  Allocate the basic
 * structures, and watch the store waiting for the hotplug scripts to tell us
 * the device's physical major and minor numbers.  Switch to InitWait.
 */
static int xen_simpleback_probe(struct xenbus_device *dev,
                           const struct xenbus_device_id *id)
{

	printk("jintack %s is called.. sweet!\n", __func__);
	return 0;
/*
	int err;
        struct backend_info *be = kzalloc(sizeof(struct backend_info),
                                          GFP_KERNEL);
	printk("jintack %s is called.. sweet!\n", __func__);
        if (!be) {
                xenbus_dev_fatal(dev, -ENOMEM,
                                 "allocating backend structure");
                return -ENOMEM;
        }
 	be->dev = dev;
        dev_set_drvdata(&dev->dev, be);

        err = xenbus_watch_pathfmt(dev, &be->backend_watch, backend_changed,
                                   "%s/%s", dev->nodename, "simple-device");	
        if (err)
                goto fail;

        err = xenbus_switch_state(dev, XenbusStateInitWait);
        if (err)
                goto fail;

        return 0;

fail:
        xen_simpleback_remove(dev);
        return err;
*/
}

static int xen_simpleback_remove(struct xenbus_device *dev)
{
/*
        struct backend_info *be = dev_get_drvdata(&dev->dev);

        if (be->backend_watch.node) {
                unregister_xenbus_watch(&be->backend_watch);
                kfree(be->backend_watch.node);
                be->backend_watch.node = NULL;
        }

        dev_set_drvdata(&dev->dev, NULL);

        kfree(be->mode);
        kfree(be);
*/
        return 0;
}
/*
 * Callback received when the frontend's state changes.
 */
static void frontend_changed(struct xenbus_device *dev,
                             enum xenbus_state frontend_state)
{
}

static void backend_changed(struct xenbus_watch *watch,
                            const char **vec, unsigned int len)
{
}
static struct xenbus_driver xen_simpleback_driver = {
        .ids  = xen_simpleback_ids,
        .probe = xen_simpleback_probe,
        .remove = xen_simpleback_remove,
        .otherend_changed = frontend_changed
};

int xen_simpleif_xenbus_init(void)
{
	return xenbus_register_backend(&xen_simpleback_driver);
}
