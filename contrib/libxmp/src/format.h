#ifndef LIBXMP_FORMAT_H
#define LIBXMP_FORMAT_H

#include <stdio.h>
#include "common.h"
#include "hio.h"

struct format_loader {
	const char *name;
	int (*const test)(HIO_HANDLE *, char *, const int);
	int (*const loader)(struct module_data *, HIO_HANDLE *, const int);
};

const char *const *format_list(void);

#ifndef LIBXMP_CORE_PLAYER
#define NUM_FORMATS 22
#define NUM_PW_FORMATS 43
#endif

#endif

