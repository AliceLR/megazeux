#ifndef NDS_RAM_OPTS_H
#define NDS_RAM_OPTS_H

#define INSECURE		1
#define ONLY_MSPACES    	1
#define HAVE_MORECORE		0
#define HAVE_MMAP		0
#define MSPACES			1
#define LACKS_SYS_MMAN_H 	1
#define LACKS_SCHED_H		1

#define malloc_getpagesize	4096
#undef  DEBUG

#endif
