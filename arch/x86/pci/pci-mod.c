#define pr_fmt(fmt) "tut: " fmt

/*
Simple PCI driver - 
modified "https://gist.github.com/levex/cd78f50565d2e5d6ceeb"
*/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/printk.h>

static const struct pci_device_id pcidevtbl[] = {

	/* put the vendor id and the device id
         * we supplied to QEMU?
	 */
	{ 0x1337, 0x0001, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ } /* terminate */

};

/* this is the function which gets called when the pci core
 * sees a device that is registered in the @pcidevtbl struct
 */
static int virttest_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	printk("[VIRTTEST] probing device\n");
	return 0;
}

static void virttest_pci_remove(struct pci_dev *pdev) {
	pr_debug("unloaded driver\n");
}

/* a simple PCI driver */
static struct pci_driver tut_pci_driver = {
	.name = "tutorial driver",
	.id_table = pcidevtbl,
	.probe = virttest_pci_probe,
	.remove = virttest_pci_remove,

};

/* called when we are insmod'd! */
static int __init virttest_pci_init(void)
{
	int rc;
	printk("[VIRTTEST] module loaded, registering pci driver.\n");

	/* this is how we turn this module into a PCI driver */
	rc = pci_register_driver(&tut_pci_driver);
	if (rc) {
		pr_err("failed to register driver.\n");
		return rc;
	}

	return 0;
}

static void __exit virttest_pci_exit(void)
{
	pr_debug("driver unloaded :-(\n");
	return;
}

module_init(virttest_pci_init);
module_exit(virttest_pci_exit);
