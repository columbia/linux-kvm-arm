
#include <stdarg.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <xen/events.h>
#include <xen/grant_table.h>
#include "common.h"

struct backend_info {
        struct xenbus_device    *dev;
	struct xen_simpleif        *simpleif;
        struct xenbus_watch     backend_watch;
};

static struct kmem_cache *xen_simpleif_cachep;
static void backend_changed(struct xenbus_watch *watch,
                            const char **vec, unsigned int len);
static int xen_simpleback_remove(struct xenbus_device *dev);

/* TODO */
static void xen_simpleif_free(void)
{
}

int __init xen_simpleif_interface_init(void)
{
	xen_simpleif_cachep = kmem_cache_create("simpleif_cache",
						sizeof(struct xen_simpleif),
						0, 0, NULL);

	if (!xen_simpleif_cachep)
		return -ENOMEM;

	return 0;
}

static const struct xenbus_device_id xen_simpleback_ids[] = {
        { "vsimple" },
        { "" }
};

static struct xen_simpleif *xen_simpleif_alloc(domid_t domid)
{
	struct xen_simpleif *simpleif;
	simpleif = kmem_cache_zalloc(xen_simpleif_cachep, GFP_KERNEL);
	if (!simpleif)
		return ERR_PTR(-ENOMEM);

	simpleif->domid = domid;
	return simpleif;

}

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

	be->simpleif = xen_simpleif_alloc(dev->otherend_id);
	if (IS_ERR(be->simpleif)) {
		err = PTR_ERR(be->simpleif);
		be->simpleif = NULL;
		xenbus_dev_fatal(dev, err, "creating block interface");
		goto fail;
	}

	/* setup back pointer */
	be->simpleif->be = be;

        err = xenbus_watch_pathfmt(dev, &be->backend_watch, backend_changed,
                                   "%s/%s", dev->nodename, "simple-device");	
        if (err) {
		printk("jintack dev->nodename is %s, but fail\n", dev->nodename);
                goto fail;
	}

	printk("jintack dev->nodename is %s, and sucess\n", dev->nodename);
        err = xenbus_switch_state(dev, XenbusStateInitWait);
        if (err)
                goto fail;

	printk("jintack backend state is XenbusStateInitWait\n");

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

static int xen_simpleif_map(struct xen_simpleif *simpleif, unsigned long shared_page,
                         unsigned int evtchn)
{
	int err;
	struct simpleif_sring *sring;
	if (simpleif == NULL)
		printk("jintack hahah simpleif s null\n");

	if (simpleif->irq)
		return 0;

	if (simpleif->be->dev == NULL)
		printk("jintack hahah dev is null\n");
	if (simpleif->simple_ring == NULL)
		printk("jintack hahah simple_ring is null\n");

	err = xenbus_map_ring_valloc(simpleif->be->dev, shared_page, &simpleif->simple_ring);
	if (err < 0)
		return err;
	
	sring = (struct simpleif_sring *)simpleif->simple_ring;
	if (sring == NULL)
		printk("jintack hahah sring is null\n");
	BACK_RING_INIT(&simpleif->simple_back_ring, sring, PAGE_SIZE);

	err = bind_interdomain_evtchn_to_irqhandler(simpleif->domid, evtchn,
			xen_simpleif_be_int, 0,
			"simpleif-backend", simpleif);
	if (err < 0) {
		xenbus_unmap_ring_vfree(simpleif->be->dev, simpleif->simple_ring);
		simpleif->simple_back_ring.sring = NULL;
		return err;
	}
	simpleif->irq = err;
	return 0;
}

static int connect_ring(struct backend_info *be)
{

	struct xenbus_device *dev = be->dev;
	unsigned long ring_ref;
	unsigned int evtchn;
	int err;

	err = xenbus_gather(XBT_NIL, dev->otherend, "ring-ref", "%lu",
	                            &ring_ref, "event-channel", "%u", &evtchn, NULL);

	if (err) {
		xenbus_dev_fatal(dev, err,
				"reading %s/ring-ref and event-channel",
				dev->otherend);
		return err;
	}

	err = xen_simpleif_map(be->simpleif, ring_ref, evtchn);
	if (err) {
		xenbus_dev_fatal(dev, err, "mapping ring-ref %lu port %u",
				ring_ref, evtchn);
		return err;
	}

	return 0;
}

/*
 * Callback received when the frontend's state changes.
 */
static void frontend_changed(struct xenbus_device *dev,
                             enum xenbus_state frontend_state)
{

        struct backend_info *be = dev_get_drvdata(&dev->dev);
        int err;

	printk("jintack %s state %d\n", __func__, frontend_state);
        switch (frontend_state) {
        case XenbusStateInitialising:
                if (dev->state == XenbusStateClosed) {
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

                err = connect_ring(be);
		err = 0;
                if (err)
                        break;

		err = xenbus_switch_state(dev, XenbusStateConnected);
		if (err) {
			xenbus_dev_fatal(dev, err, "%s: switching to Connected state",
					dev->nodename);
			printk("jintack backed is NOT connected\n");
		} else
			printk("jintack backed is connected\n");
                break;

        case XenbusStateClosing:
                xenbus_switch_state(dev, XenbusStateClosing);
                break;

        case XenbusStateClosed:
		//TODO
                //xen_simpleif_disconnect(be->simpleif);
                xenbus_switch_state(dev, XenbusStateClosed);
                if (xenbus_dev_is_online(dev))
                        break;
                /* fall through if not online */
        case XenbusStateUnknown:
                /* implies xen_simpleif_disconnect() via xen_simplebk_remove() */
                device_unregister(&dev->dev);
                break;

        default:
                xenbus_dev_fatal(dev, -EINVAL, "saw state %d at frontend",
                                 frontend_state);
                break;
        }
}

static void backend_changed(struct xenbus_watch *watch,
                            const char **vec, unsigned int vec_size)
{

	struct backend_info *be = container_of(watch,
                                               struct backend_info,
                                               backend_watch);
        char *str;
        unsigned int len;
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
