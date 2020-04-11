#ifndef __SMP_COMMON_H__
#define __SMP_COMMON_H__

enum SmpError {
	SMP_OK               =         0,
	SMP_FAIL             =        -1,
	SMP_UNKNOWN          =        -1, // the same as the fail case
	SMP_NO_MEMORY        =      -101,
	SMP_UNSUPPORTED      =      -102,
	SMP_WOULD_BLOCK      =      -103,
	SMP_INVALID_REQ      =      -104,
	SMP_INVALID_RESOURCE =      -105,
};

#endif /* __SMP_COMMON_H__ */
