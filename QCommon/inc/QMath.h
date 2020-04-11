#ifndef __QMATH_H__
#define __QMATH_H__

#include <math.h>

#define qisinf(A)         ((A) == HUGE_VAL)
#define ROUND2I(x)        ((int)((x) + 0.5f))
#define CEIL2I(x)         ((x) - int(x) > 0 ? int((x) + 1) : int(x))

#endif /* __QMATH_H__ */
