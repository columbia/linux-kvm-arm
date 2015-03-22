#ifndef __XEN_SIMPLEIF__BACKEND__COMMON_H__
#define __XEN_SIMPLEIF__BACKEND__COMMON_H__

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/io.h>
#include <linux/rbtree.h>
#include <asm/setup.h>
#include <asm/pgalloc.h>
#include <asm/hypervisor.h>
#include <xen/grant_table.h>
#include <xen/xenbus.h>
#include <xen/interface/io/ring.h>
#include <xen/interface/io/protocols.h>

int xen_simpleif_interface_init(void);
int xen_simpleif_xenbus_init(void);

struct xen_simpleif{
	domid_t 	domid;
};

#endif /* __XEN_SIMPLEIF__BACKEND__COMMON_H__ */
