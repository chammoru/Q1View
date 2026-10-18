#pragma once

#include <vector>

namespace q1view {
// Container framing is immutable even when the user reinterprets its pixels.
struct RawFrameLayout {
	std::vector<ULONGLONG> offsets;
	UINT payloadBytes = 0;
	bool Read(CFile& file, long frame, BYTE* target, size_t requested) const {
		if (frame < 0 || size_t(frame) >= offsets.size() || requested > UINT_MAX) return false;
		file.Seek(offsets[size_t(frame)], CFile::begin);
		const UINT count = UINT((std::min)(requested, size_t(payloadBytes)));
		const UINT read = file.Read(target, count);
		if (read != count) return false;
		// Larger interpretations must not consume the next FRAME marker/payload.
		if (requested > read) memset(target + read, 0, requested - read);
		return true;
	}
};
}
