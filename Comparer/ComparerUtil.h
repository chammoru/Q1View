#pragma once

#include "afx.h"

static inline CString getBaseName(CString pathName)
{
	return pathName.Mid(pathName.ReverseFind('\\') + 1);
}
