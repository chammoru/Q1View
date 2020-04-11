#pragma once

#ifdef _DEBUG
#include <locale.h>

// unicode
#define WTRACE(format, ...) do { \
	setlocale(LC_ALL, "Korean"); \
	wchar_t function[256]; \
	int nLen = MultiByteToWideChar(CP_UTF8, 0, __FUNCTION__, -1, NULL, 0); \
	MultiByteToWideChar(CP_UTF8, 0, __FUNCTION__, -1, function, nLen); \
	TRACE(_T("%s(%d): ") _T(format), function, __LINE__, __VA_ARGS__); \
} while (0)

// ascii
#define ATRACE(format, ...) do { \
	TRACE("%s(%d): " format, __FUNCTION__, __LINE__, __VA_ARGS__); \
} while (0)
#else
#define WTRACE(format, ...) do {} while (0)
#define ATRACE(format, ...) do {} while (0)
#endif

#ifdef  UNICODE
#define MTRACE WTRACE
#else
#define MTRACE ATRACE
#endif

