// ViewerFileOrder.h : shared folder-navigation ordering helpers.

#pragma once

#include <algorithm>
#include <vector>

#include "Shlwapi.h"

namespace q1view {

inline bool LessFileNameOrdinal(const CString &a, const CString &b)
{
	return CompareStringOrdinal(PathFindFileName(a), -1,
		PathFindFileName(b), -1, TRUE) == CSTR_LESS_THAN;
}

inline void CollectSortedFilePaths(const CString &pattern, std::vector<CString> &files)
{
	files.clear();

	CFileFind finder;
	BOOL working = finder.FindFile(pattern);
	while (working) {
		working = finder.FindNextFile();
		if (!finder.IsDirectory())
			files.push_back(finder.GetFilePath());
	}
	finder.Close();

	std::sort(files.begin(), files.end(), LessFileNameOrdinal);
}

} // namespace q1view
