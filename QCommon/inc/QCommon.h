#ifndef __QCOMMON_H__
#define __QCOMMON_H__

#define ARRAY_SIZE(x)     (sizeof(x) / sizeof((x)[0]))
#define ROUNDUP_8(n)      (((n) + 7) & ~7)
#define ROUNDUP_DWORD(n)  (((n) + 3) & ~3) // 3 <= sizeof(DWORD) - 1
#define ROUNDUP_EVEN(n)   (((n) + 1) & ~1) // 1 <= 2 /*even*/ - 1

#define QSQR(a)           ((a) * (a))
#define QCUBE(a)          (QSQR(a) * (a))

#define QMAX(a,b)         ((a) >= (b) ? (a) : (b))
#define QMIN(a,b)         ((a) <= (b) ? (a) : (b))

typedef unsigned char  qu8;
typedef unsigned short qu16;
typedef unsigned int   qu32;

#include <string.h>
#define QSWAP(x, y)        do {                                    \
		qu8 temp[sizeof(x) == sizeof(y) ? (signed)sizeof(x) : -1]; \
		memcpy(temp, &y       , sizeof(x));                        \
		memcpy(&y  , &x       , sizeof(x));                        \
		memcpy(&x  , temp     , sizeof(x));                        \
	} while(0)

#endif /* __QCOMMON_H__ */
