#include "../Viewer/VideoOrientation.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <windows.h>

static void Require(bool condition, const char* text)
{
	if (!condition) {
		std::fprintf(stderr, "FAIL: %s\n", text);
		std::exit(1);
	}
}

static void Be32(std::vector<unsigned char>& bytes, uint32_t value)
{
	bytes.push_back(static_cast<unsigned char>(value >> 24));
	bytes.push_back(static_cast<unsigned char>(value >> 16));
	bytes.push_back(static_cast<unsigned char>(value >> 8));
	bytes.push_back(static_cast<unsigned char>(value));
}

static void Be64(std::vector<unsigned char>& bytes, uint64_t value)
{
	Be32(bytes, static_cast<uint32_t>(value >> 32));
	Be32(bytes, static_cast<uint32_t>(value));
}

static uint32_t Type(const char* text)
{
	return (uint32_t(uint8_t(text[0])) << 24) | (uint32_t(uint8_t(text[1])) << 16) |
		(uint32_t(uint8_t(text[2])) << 8) | uint32_t(uint8_t(text[3]));
}

static std::vector<unsigned char> Box(const char* type, const std::vector<unsigned char>& payload,
	bool extended = false)
{
	std::vector<unsigned char> box;
	if (extended) {
		Be32(box, 1);
		Be32(box, Type(type));
		Be64(box, 16 + payload.size());
	} else {
		Be32(box, static_cast<uint32_t>(8 + payload.size()));
		Be32(box, Type(type));
	}
	box.insert(box.end(), payload.begin(), payload.end());
	return box;
}

static void Append(std::vector<unsigned char>& destination, const std::vector<unsigned char>& source)
{
	destination.insert(destination.end(), source.begin(), source.end());
}

static std::vector<unsigned char> Track(int degrees, bool video, bool version1 = false)
{
	std::vector<unsigned char> tkhd(version1 ? 52 : 40, 0);
	tkhd[0] = version1 ? 1 : 0;
	int32_t a = 65536, b = 0, c = 0, d = 65536;
	if (degrees == 90) { a = d = 0; b = 65536; c = -65536; }
	if (degrees == 180) { a = d = -65536; }
	if (degrees == 270) { a = d = 0; b = -65536; c = 65536; }
	Be32(tkhd, static_cast<uint32_t>(a));
	Be32(tkhd, static_cast<uint32_t>(b));
	Be32(tkhd, 0);
	Be32(tkhd, static_cast<uint32_t>(c));
	Be32(tkhd, static_cast<uint32_t>(d));
	Be32(tkhd, 0);
	Be32(tkhd, 0);
	Be32(tkhd, 0);
	Be32(tkhd, 0x40000000);

	std::vector<unsigned char> hdlr(8, 0);
	Be32(hdlr, Type(video ? "vide" : "soun"));
	std::vector<unsigned char> mdia;
	Append(mdia, Box("hdlr", hdlr));
	std::vector<unsigned char> trak;
	Append(trak, Box("tkhd", tkhd));
	Append(trak, Box("mdia", mdia, true));
	return Box("trak", trak);
}

static std::wstring TempPath(const wchar_t* suffix)
{
	wchar_t directory[MAX_PATH] = {};
	GetTempPathW(MAX_PATH, directory);
	wchar_t path[MAX_PATH] = {};
	swprintf_s(path, L"%sQ1View-video-orientation-%lu-%s.mp4", directory,
		GetCurrentProcessId(), suffix);
	return path;
}

static void WriteFile(const std::wstring& path, const std::vector<unsigned char>& bytes)
{
	FILE* file = nullptr;
	Require(_wfopen_s(&file, path.c_str(), L"wb") == 0, "create temporary MP4");
	Require(fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size(), "write temporary MP4");
	fclose(file);
}

static void TestRotation(int expected, bool version1 = false)
{
	std::vector<unsigned char> moov;
	Append(moov, Track(0, false)); // An audio track must not hide the video matrix.
	Append(moov, Track(expected, true, version1));
	std::vector<unsigned char> file;
	Append(file, Box("ftyp", std::vector<unsigned char>(8, 0)));
	Append(file, Box("moov", moov));
	const std::wstring path = TempPath(std::to_wstring(expected).c_str());
	WriteFile(path, file);
	int actual = -1;
	Require(q1::ReadVideoDisplayRotation(path, actual), "read synthetic video display matrix");
	Require(actual == expected, "map MP4 matrix to clockwise cardinal rotation");
	DeleteFileW(path.c_str());
}

int wmain(int argc, wchar_t** argv)
{
	TestRotation(0);
	TestRotation(90);
	TestRotation(180, true);
	TestRotation(270);

	const std::wstring malformed = TempPath(L"malformed");
	WriteFile(malformed, {0, 0, 0, 64, 'm', 'o', 'o', 'v'});
	int rotation = -1;
	Require(!q1::ReadVideoDisplayRotation(malformed, rotation), "reject truncated MP4 boxes");
	DeleteFileW(malformed.c_str());

	if (argc == 3) {
		const int expected = _wtoi(argv[2]);
		Require(q1::ReadVideoDisplayRotation(argv[1], rotation), "read supplied video display matrix");
		Require(rotation == expected, "supplied video has expected clockwise rotation");
		wprintf(L"%s: clockwise rotation %d degrees\n", argv[1], rotation);
	}

	std::puts("Video orientation tests passed");
	return 0;
}
