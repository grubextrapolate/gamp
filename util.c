/* this file is a part of amp software

	util.c: created by Andrew Richards

modified for use in gamp by Russ Burdick

*/

#include "util.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* die - for terminal conditions prints the error message and exits */
/*			 can not be suppressed with -q,-quiet												*/

void die(char *fmt, ...) {
	va_list ap;
	va_start(ap,fmt);
	vfprintf(stderr, fmt, ap);
        cleanup();
	exit(-1);
}


void debug(char *fmt, ...) {
#ifdef DEBUG
	va_list ap;
	va_start(ap,fmt);
	vfprintf(stderr, fmt, ap);
#endif
}
