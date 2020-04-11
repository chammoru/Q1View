#ifndef __QDEBUG_H__
#define __QDEBUG_H__

#if defined(ANDROID)
#include <stdarg.h>
#include <utils/Log.h>

#ifndef QCMN_TAG
#define QCMN_TAG    "QCommon"
#endif

inline void qcmn_alog(int prio, const char *tag, const char *msg, ...)
{
	va_list argptr;
	va_start(argptr, msg);
	__android_log_vprint(prio, tag, msg, argptr);
	va_end(argptr);
}

#define LOGERR(format, arg...) do {                            \
	qcmn_alog(ANDROID_LOG_ERROR, QCMN_TAG, "%s(%d): " format,  \
		__FUNCTION__, __LINE__, ## arg);                       \
} while (0)
#define LOGWRN(format, arg...) do {                            \
	qcmn_alog(ANDROID_LOG_WARN, QCMN_TAG, "%s(%d): " format,   \
		__FUNCTION__, __LINE__, ## arg);                       \
} while (0)
#define LOGINF(format, arg...) do {                            \
	qcmn_alog(ANDROID_LOG_INFO, QCMN_TAG, format, ## arg);     \
} while (0)
#elif defined(_WIN32)
#include <stdio.h>

#define LOGERR(format, ...) do {                               \
	fprintf(stderr, "E: %s(%d): " format "\n",                 \
		__FUNCTION__, __LINE__, __VA_ARGS__);                  \
} while (0)
#define LOGWRN(format, ...) do {                               \
	fprintf(stderr, "W: %s(%d): " format "\n",                 \
		__FUNCTION__, __LINE__, __VA_ARGS__);                  \
} while (0)
#define LOGINF(format, ...) do {                               \
	fprintf(stdout, "I: " format "\n", __VA_ARGS__);           \
} while (0)
#elif defined(__GNUC__)
#include <stdio.h>

#define LOGERR(format, arg...) do {                            \
	fprintf(stderr, "E: %s(%d): " format "\n",                 \
		__FUNCTION__, __LINE__, ## arg);                       \
} while (0)
#define LOGWRN(format, arg...) do {                            \
	fprintf(stderr, "W: %s(%d): " format "\n",                 \
		__FUNCTION__, __LINE__, ## arg);                       \
} while (0)
#define LOGINF(format, arg...) do {                            \
	fprintf(stdout, "I: " format "\n", ## arg);                \
} while (0)
#endif // defined(ANDROID)

#if defined(NDEBUG)
#define ABORT() do {} while (0)
#define LOGVBS(format, ...) do {} while (0)
#else
#include <stdlib.h>

#define ABORT() do { abort(); } while (0)
#if defined(ANDROID)
#define LOGVBS(format, arg...) do {                            \
	qcmn_alog(ANDROID_LOG_VERBOSE, QCMN_TAG, "%s(%d): " format,\
		__FUNCTION__, __LINE__, ## arg);                       \
} while (0)
#elif defined(_WIN32)

#define LOGVBS(format, ...) do {                               \
	fprintf(stdout, "V: %s(%d): " format "\n",                 \
		__FUNCTION__, __LINE__, __VA_ARGS__);                  \
} while (0)
#elif defined(__GNUC__)
#define LOGVBS(format, arg...) do {                            \
	fprintf(stdout, "V: %s(%d): " format "\n",                 \
		__FUNCTION__, __LINE__, ## arg);                       \
} while (0)
#endif // defined(ANDROID)
#endif // defined(NDEBUG)

#define QASSERT(expr) do {                                     \
	if (!(expr)) {                                             \
		LOGERR("assertion: '" #expr "' failed");               \
		ABORT();                                               \
	}                                                          \
} while (0)

#define QASSERT_R(expr, value) do {                            \
	if (!(expr)) {                                             \
		LOGERR("assertion: '" #expr "' failed");               \
		ABORT();                                               \
		return value;                                          \
	}                                                          \
} while (0)

#define QASSERT_G(expr, label) do {                            \
	if (!(expr)) {                                             \
		LOGERR("assertion: '" #expr "' failed");               \
		ABORT();                                               \
		goto label;                                            \
	}                                                          \
} while (0)

#define QASSERT_GV(expr, ret, value, label) do {               \
	if (!(expr)) {                                             \
		ret = value;                                           \
		LOGERR("assertion: '" #expr "' failed");               \
		ABORT();                                               \
		goto label;                                            \
	}                                                          \
} while (0)

#endif // __QDEBUG_H__

