#ifndef __SCL_H__
#define __SCL_H__

typedef enum {
	EOK,     /* everything ok */
	EMEM,    /* memory problems (like ENOMEM) */
	EINPUT,  /* unexpected input value */
	EDISK,   /* some disk operation failed */
	ECONFIG, /* some problem with scl configuration */
    ERUN,    /* some problem with running external program */
    ERPMLIB, /* some occured in rpm library */
    ESYS,    /* unexpected system error, e. g. unexpected fail of function from stdlib */
} scl_rc;

#endif
