#include <windows.h>
#include "VideoOrientation.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <limits>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace {

constexpr uint32_t FourCC(char a, char b, char c, char d)
{
	return (uint32_t(uint8_t(a)) << 24) | (uint32_t(uint8_t(b)) << 16) |
		(uint32_t(uint8_t(c)) << 8) | uint32_t(uint8_t(d));
}

uint32_t ReadBe32(const unsigned char* p)
{
	return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
		(uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

uint64_t ReadBe64(const unsigned char* p)
{
	return (uint64_t(ReadBe32(p)) << 32) | ReadBe32(p + 4);
}

class FileReader {
public:
	explicit FileReader(const std::wstring& path) : mFile(nullptr), mSize(0)
	{
		if (_wfopen_s(&mFile, path.c_str(), L"rb") != 0)
			return;
		if (_fseeki64(mFile, 0, SEEK_END) != 0) {
			fclose(mFile);
			mFile = nullptr;
			return;
		}
		__int64 size = _ftelli64(mFile);
		if (size < 0) {
			fclose(mFile);
			mFile = nullptr;
			return;
		}
		mSize = uint64_t(size);
	}

	~FileReader()
	{
		if (mFile)
			fclose(mFile);
	}

	bool IsOpen() const { return mFile != nullptr; }
	uint64_t Size() const { return mSize; }

	bool Read(uint64_t offset, void* destination, size_t bytes)
	{
		if (!mFile || offset > mSize || bytes > mSize - offset ||
			offset > uint64_t(std::numeric_limits<__int64>::max()))
			return false;
		if (_fseeki64(mFile, __int64(offset), SEEK_SET) != 0)
			return false;
		return fread(destination, 1, bytes, mFile) == bytes;
	}

private:
	FILE* mFile;
	uint64_t mSize;
};

struct Box {
	uint64_t start = 0;
	uint64_t size = 0;
	uint64_t payload = 0;
	uint32_t type = 0;
	uint64_t End() const { return start + size; }
};

bool ReadBox(FileReader& file, uint64_t position, uint64_t parentEnd, Box& box)
{
	if (position > parentEnd || parentEnd - position < 8)
		return false;

	unsigned char header[16] = {};
	if (!file.Read(position, header, 8))
		return false;

	uint64_t size = ReadBe32(header);
	uint64_t headerSize = 8;
	if (size == 1) {
		if (parentEnd - position < 16 || !file.Read(position + 8, header + 8, 8))
			return false;
		size = ReadBe64(header + 8);
		headerSize = 16;
	} else if (size == 0) {
		size = parentEnd - position;
	}

	if (ReadBe32(header + 4) == FourCC('u', 'u', 'i', 'd'))
		headerSize += 16;
	if (size < headerSize || size > parentEnd - position)
		return false;

	box.start = position;
	box.size = size;
	box.payload = position + headerSize;
	box.type = ReadBe32(header + 4);
	return true;
}

bool MatrixRotation(int32_t a, int32_t b, int32_t c, int32_t d, int& degrees)
{
	auto magnitude = [](int32_t value) { return value < 0 ? -int64_t(value) : int64_t(value); };
	const int64_t diagonal = std::max(magnitude(a), magnitude(d));
	const int64_t offDiagonal = std::max(magnitude(b), magnitude(c));

	// MP4 matrices are 16.16 fixed point. Accept scaled cardinal matrices, but
	// reject arbitrary/sheared transforms instead of guessing an orientation.
	if (diagonal >= 32768 && offDiagonal * 4 < diagonal) {
		if (a > 0 && d > 0) degrees = 0;
		else if (a < 0 && d < 0) degrees = 180;
		else return false;
		return true;
	}
	if (offDiagonal >= 32768 && diagonal * 4 < offDiagonal) {
		if (b > 0 && c < 0) degrees = 90;
		else if (b < 0 && c > 0) degrees = 270;
		else return false;
		return true;
	}
	return false;
}

bool ReadTrackRotation(FileReader& file, const Box& track, int& degrees)
{
	bool video = false;
	bool haveMatrix = false;
	int matrixDegrees = 0;

	for (uint64_t position = track.payload; position < track.End();) {
		Box child;
		if (!ReadBox(file, position, track.End(), child))
			break;
		if (child.type == FourCC('t', 'k', 'h', 'd')) {
			unsigned char version = 0;
			if (file.Read(child.payload, &version, 1)) {
				const uint64_t matrixOffset = version == 1 ? 52 : 40;
				unsigned char matrix[20] = {};
				if ((version == 0 || version == 1) && child.End() - child.payload >= matrixOffset + 20 &&
					file.Read(child.payload + matrixOffset, matrix, sizeof(matrix))) {
					const int32_t a = int32_t(ReadBe32(matrix));
					const int32_t b = int32_t(ReadBe32(matrix + 4));
					const int32_t c = int32_t(ReadBe32(matrix + 12));
					const int32_t d = int32_t(ReadBe32(matrix + 16));
					haveMatrix = MatrixRotation(a, b, c, d, matrixDegrees);
				}
			}
		} else if (child.type == FourCC('m', 'd', 'i', 'a')) {
			for (uint64_t mdiaPosition = child.payload; mdiaPosition < child.End();) {
				Box mdiaChild;
				if (!ReadBox(file, mdiaPosition, child.End(), mdiaChild))
					break;
				if (mdiaChild.type == FourCC('h', 'd', 'l', 'r') && mdiaChild.End() - mdiaChild.payload >= 12) {
					unsigned char handler[4] = {};
					if (file.Read(mdiaChild.payload + 8, handler, sizeof(handler)))
						video = ReadBe32(handler) == FourCC('v', 'i', 'd', 'e');
				}
				mdiaPosition = mdiaChild.End();
			}
		}
		position = child.End();
	}

	if (!video || !haveMatrix)
		return false;
	degrees = matrixDegrees;
	return true;
}

} // namespace

namespace q1 {

bool ReadVideoDisplayRotation(const std::wstring& path, int& clockwiseDegrees)
{
	clockwiseDegrees = 0;
	FileReader file(path);
	if (!file.IsOpen())
		return false;

	for (uint64_t position = 0; position < file.Size();) {
		Box top;
		if (!ReadBox(file, position, file.Size(), top))
			break;
		if (top.type == FourCC('m', 'o', 'o', 'v')) {
			for (uint64_t childPosition = top.payload; childPosition < top.End();) {
				Box child;
				if (!ReadBox(file, childPosition, top.End(), child))
					break;
				if (child.type == FourCC('t', 'r', 'a', 'k') &&
					ReadTrackRotation(file, child, clockwiseDegrees))
					return true;
				childPosition = child.End();
			}
		}
		position = top.End();
	}
	return false;
}

} // namespace q1
