#ifndef __XEN_PUBLIC_IO_SAMPLEIF_H__
#define __XEN_PUBLIC_IO_SAMPLEIF_H__

#include <xen/interface/io/ring.h>
#include <xen/interface/grant_table.h>

struct simpleif_request {
	uint8_t operation;
};

struct simpleif_response {
	uint64_t 	id;
	uint8_t		operation;
};


DEFINE_RING_TYPES(simpleif, struct simpleif_request, struct simpleif_response);

#endif /* __XEN_PUBLIC_IO_SAMPLEIF_H__ */
