#pragma once

#include "Q1UiAppearanceWin.h"

namespace q1view {

// Complete one owner-drawn control off-screen before presenting it. The caller
// keeps native BUTTON/STATIC behavior and suppresses only the default erase.
// Allocation failure falls back to direct, themed painting, not COLOR_BTNFACE.
class WindowsUiBufferedPaint {
	HDC mTarget, mMemory = nullptr;
	HBITMAP mBitmap = nullptr;
	HGDIOBJ mPrevious = nullptr;
	RECT mRect;
	WindowsUiColor mBackground;
	int mSaved = 0;
public:
	WindowsUiBufferedPaint(HDC target, const RECT& rect, WindowsUiColor background)
		: mTarget(target), mRect(rect), mBackground(background)
	{
		const int width = rect.right-rect.left, height = rect.bottom-rect.top;
		if (target && width > 0 && height > 0) {
			mMemory = CreateCompatibleDC(target);
			mBitmap = mMemory ? CreateCompatibleBitmap(target,width,height) : nullptr;
			if (mBitmap) {
				mPrevious = SelectObject(mMemory,mBitmap);
				if (mPrevious && mPrevious != HGDI_ERROR)
					SetWindowOrgEx(mMemory,rect.left,rect.top,nullptr);
				else { DeleteObject(mBitmap); mBitmap=nullptr; }
			}
			if (!mBitmap && mMemory) { DeleteDC(mMemory); mMemory=nullptr; }
		}
		mSaved = SaveDC(Dc());
		FillRect(Dc(),&mRect,WindowsUiColorBrush(mBackground));
	}
	~WindowsUiBufferedPaint()
	{
		if (mSaved) RestoreDC(Dc(),mSaved);
		if (mMemory) {
			if (!BitBlt(mTarget,mRect.left,mRect.top,mRect.right-mRect.left,mRect.bottom-mRect.top,
				mMemory,mRect.left,mRect.top,SRCCOPY))
				FillRect(mTarget,&mRect,WindowsUiColorBrush(mBackground));
			SelectObject(mMemory,mPrevious); DeleteObject(mBitmap); DeleteDC(mMemory);
		}
	}
	HDC Dc() const { return mMemory ? mMemory : mTarget; }
	bool Buffered() const { return mMemory != nullptr; }
	WindowsUiBufferedPaint(const WindowsUiBufferedPaint&) = delete;
	WindowsUiBufferedPaint& operator=(const WindowsUiBufferedPaint&) = delete;
};

} // namespace q1view
