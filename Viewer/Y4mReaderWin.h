#pragma once

#include "RawFrameLayout.h"
#include "qimage_cs.h"
#include <sstream>
#include <climits>
#include <cstdlib>
#include <utility>
#include <cerrno>

namespace q1view {
struct Y4mInfo {
	int width = 0, height = 0;
	QIMAGE_CS colorSpace = QIMAGE_CS_YUV420;
	double fps = 0;
	RawFrameLayout layout;
};

inline bool HasY4mSignature(CFile& file) {
	const auto position = file.GetPosition();
	char signature[9] = {};
	const bool result = file.Read(signature, sizeof(signature)) == sizeof(signature) &&
		memcmp(signature, "YUV4MPEG2", sizeof(signature)) == 0;
	file.Seek(position, CFile::begin);
	return result;
}

inline bool HasY4mSignature(const CString& path) {
	CFile file; CFileException error;
	if (!file.Open(path, CFile::modeRead | CFile::shareDenyNone | CFile::typeBinary, &error)) return false;
	return HasY4mSignature(file);
}

inline bool ReadY4mLine(CFile& file, std::string& line) {
	const auto start = file.GetPosition();
	char buffer[4096];
	const UINT count = file.Read(buffer, sizeof(buffer));
	const char* end = static_cast<const char*>(memchr(buffer, '\n', count));
	if (!end) return false;
	line.assign(buffer, size_t(end - buffer));
	file.Seek(start + (end - buffer) + 1, CFile::begin);
	if (!line.empty() && line.back() == '\r') line.pop_back();
	return true;
}

inline bool ParseY4m(CFile& file, Y4mInfo& result) {
	file.Seek(0, CFile::begin);
	std::string line, token, color;
	if (!ReadY4mLine(file, line)) return false;
	std::istringstream header(line);
	header >> token;
	if (token != "YUV4MPEG2") return false;
	Y4mInfo info;
	auto positive = [](const std::string& value, int& output) {
		char* end = nullptr; errno = 0;
		const long number = strtol(value.c_str(), &end, 10);
		if (errno || end == value.c_str() || *end || number <= 0 || number > INT_MAX) return false;
		output = int(number); return true;
	};
	while (header >> token) {
		if (token.size() < 2) continue;
		const auto value = token.substr(1);
		if (token[0] == 'W' && !positive(value, info.width)) return false;
		if (token[0] == 'H' && !positive(value, info.height)) return false;
		if (token[0] == 'C') color = value;
		if (token[0] == 'F') {
			int numerator = 0, denominator = 0;
			const auto colon = value.find(':');
			if (colon == std::string::npos || !positive(value.substr(0, colon), numerator) ||
				!positive(value.substr(colon + 1), denominator)) return false;
			info.fps = double(numerator) / denominator;
		}
	}
	if (info.width <= 0 || info.height <= 0) return false;
	const ULONGLONG pixels = ULONGLONG(info.width) * info.height;
	const ULONGLONG chroma = ULONGLONG((info.width + 1LL) / 2) * ((info.height + 1LL) / 2);
	ULONGLONG bytes = 0;
	if (color.empty() || color == "420" || color == "420jpeg" || color == "420mpeg2" || color == "420paldv")
		bytes = pixels + 2 * chroma;
	else if (color == "420p10") { info.colorSpace = QIMAGE_CS_YUV420P10LE; bytes = 2 * (pixels + 2 * chroma); }
	else if (color == "mono") { info.colorSpace = QIMAGE_CS_GRAYSCALE; bytes = pixels; }
	else return false; // Other Y4M formats retain the existing decoded-video fallback.
	if (!bytes || bytes > INT_MAX || pixels > INT_MAX / 6) return false;
	info.layout.payloadBytes = UINT(bytes);
	const auto length = file.GetLength();
	while (file.GetPosition() < length) {
		if (!ReadY4mLine(file, line) || (line != "FRAME" && line.compare(0, 6, "FRAME ") != 0)) return false;
		const auto offset = file.GetPosition();
		if (length - offset < bytes) break; // Ignore an incomplete trailing frame while a file is being written.
		if (info.layout.offsets.size() >= LONG_MAX) return false;
		info.layout.offsets.push_back(offset);
		file.Seek(offset + bytes, CFile::begin);
	}
	if (info.layout.offsets.empty()) return false;
	result = std::move(info);
	file.Seek(0, CFile::begin);
	return true;
}
}
