
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
        if (err) {
		printk("jintack dev->nodename is %s, but fail\n", dev->nodename);
                goto fail;
	}

	printk("jintack dev->nodename is %s, and sucess\n", dev->nodename);
/*
        err = xenbus_switch_state(dev, XenbusStateInitWait);
        if (err)
                goto fail;

	printk("jintack backend state is XenbusStateInitWait\n");
*/

        return 0;

fail:
        xen_simpleback_remove(dev);
        return err;
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
	printk("jintack %s state %d\n", __func__, frontend_state);
#if 0
        struct backend_info *be = dev_get_drvdata(&dev->dev);
        int err;
        DPRINTK("%s", xenbus_strstate(frontend_state));

        switch (frontend_state) {
        case XenbusStateInitialising:
                if (dev->state == XenbusStateClosed) {
                        pr_info(DRV_PFX "%s: prepare for reconnect\n",
                                dev->nodename);
                        xenbus_switch_state(dev, XenbusStateInitWait);
                }
                break;

        case XenbusStateInitialised:
        case XenbusStateConnected:
                /*
                 * Ensure we connect even when two watches fire in
                 * close succession and we miss the intermediate value
                 * of frontend_state.
                 */
                if (dev->state == XenbusStateConnected)
                        break;

                /*
                 * Enforce precondition before potential leak point.
                 * xen_blkif_disconnect() is idempotent.
                 */
                err = xen_blkif_disconnect(be->blkif);
                if (err) {
                        xenbus_dev_fatal(dev, err, "pending I/O");
                        break;
                }
                err = connect_ring(be);
                if (err)
                        break;
                xen_update_blkif_status(be->blkif);
                break;

        case XenbusStateClosing:
                xenbus_switch_state(dev, XenbusStateClosing);
                break;

        case XenbusStateClosed:
                xen_blkif_disconnect(be->blkif);
                xenbus_switch_state(dev, XenbusStateClosed);
                if (xenbus_dev_is_online(dev))
                        break;
                /* fall through if not online */
        case XenbusStateUnknown:
                /* implies xen_blkif_disconnect() via xen_blkbk_remove() */
                device_unregister(&dev->dev);
                break;

        default:
                xenbus_dev_fatal(dev, -EINVAL, "saw state %d at frontend",
                                 frontend_state);
                break;
        }
#endif
}

static void backend_changed(struct xenbus_watch *watch,
                            const char **vec, unsigned int vec_size)
{

	struct backend_info *be = container_of(watch,
                                               struct backend_info,
                                               backend_watch);
	struct xenbus_device *dev = be->dev;
        char *str;
        unsigned int len;
	int err;
	printk("jintack %s is called.. sweet!\n", __func__);
	printk("jintack %s, nodename: %s\n", __func__, be->dev->nodename);

	str = xenbus_read(XBT_NIL, be->dev->nodename, "simple-device", &len);
        if (IS_ERR(str))
                return;
        if (len == sizeof("connected")-1 && !memcmp(str, "connected", len)) {
                /* Not interested in this watch anymore. */
		unregister_xenbus_watch(&be->backend_watch);
		kfree(be->backend_watch.node);
		be->backend_watch.node = NULL;
        }
        kfree(str);

	err = xenbus_switch_state(dev, XenbusStateConnected);
        if (err) {
                xenbus_dev_fatal(dev, err, "%s: switching to Connected state",
                                 dev->nodename);
		printk("jintack backed is NOT connected\n");
	} else
		printk("jintack backed is connected\n");

        return;
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
