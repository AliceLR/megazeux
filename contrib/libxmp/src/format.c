/* Extended Module Player
 * Copyright (C) 1996-2016 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include "format.h"

extern const struct format_loader libxmp_loader_xm;
extern const struct format_loader libxmp_loader_mod;
extern const struct format_loader libxmp_loader_flt;
extern const struct format_loader libxmp_loader_st;
extern const struct format_loader libxmp_loader_it;
extern const struct format_loader libxmp_loader_s3m;
extern const struct format_loader libxmp_loader_stm;
extern const struct format_loader libxmp_loader_mtm;
extern const struct format_loader libxmp_loader_ult;
extern const struct format_loader libxmp_loader_amf;
extern const struct format_loader libxmp_loader_asylum;
extern const struct format_loader libxmp_loader_gdm;
extern const struct format_loader libxmp_loader_mmd1;
extern const struct format_loader libxmp_loader_mmd3;
extern const struct format_loader libxmp_loader_med2;
extern const struct format_loader libxmp_loader_med3;
extern const struct format_loader libxmp_loader_med4;
extern const struct format_loader libxmp_loader_okt;
extern const struct format_loader libxmp_loader_far;
extern const struct format_loader libxmp_loader_669;
extern const struct format_loader libxmp_loader_hmn;

extern const struct pw_format *const pw_format[];

const struct format_loader *const format_loader[NUM_FORMATS + 2] = {
	&libxmp_loader_xm,
	&libxmp_loader_mod,
	&libxmp_loader_flt,
	&libxmp_loader_st,
	&libxmp_loader_it,
	&libxmp_loader_s3m,
	&libxmp_loader_stm,
	&libxmp_loader_mtm,
	&libxmp_loader_ult,
	&libxmp_loader_amf,
	&libxmp_loader_asylum,
	&libxmp_loader_gdm,
	&libxmp_loader_mmd1,
	&libxmp_loader_mmd3,
	&libxmp_loader_med2,
	&libxmp_loader_med3,
	&libxmp_loader_med4,
	&libxmp_loader_okt,
	&libxmp_loader_far,
	&libxmp_loader_hmn,
	&libxmp_loader_669,
	NULL
};

static const char *_farray[NUM_FORMATS + 1] = { NULL };

char **format_list()
{
	int count, i;

	if (_farray[0] == NULL) {
		for (count = i = 0; format_loader[i] != NULL; i++) {

			_farray[count++] = format_loader[i]->name;
		}

		_farray[count] = NULL;
	}

	return (char **)_farray;
}
