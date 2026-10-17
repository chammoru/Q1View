#pragma once

// Native BUTTON behavior/accessibility for popup commands; read-only filenames
// remain STATIC windows. Only measurement and painting are customized.
class CQMenuItem : public CWnd
{
	DECLARE_DYNAMIC(CQMenuItem)
	bool mMouseIn = false, mPopupOpen = false, mPopupQueued = false;
	static constexpr UINT OpenPopupMessage = WM_APP + 219;
	CRect mRcClient;
	CFont mFont;
	CMenu* mMenu = nullptr;
	DWORD mTextHorizAlign = DT_CENTER;
	void Paint(HDC dc);
	void OpenPopup();
public:
	CQMenuItem();
	virtual ~CQMenuItem();
	void DefaultSetting(CDC* dc, CString& text);
	void CalcRect(CRect* rect);
	void SetWindowText(LPCTSTR text);
	BOOL Create(LPCTSTR text, CRect& rect, CWnd* parent, CMenu* menu, DWORD align = DT_CENTER);
protected:
	LRESULT WindowProc(UINT message, WPARAM wp, LPARAM lp) override;
	DECLARE_MESSAGE_MAP()
	afx_msg void OnPaint();
	afx_msg void DrawItem(LPDRAWITEMSTRUCT item);
	afx_msg void OnClicked();
	afx_msg BOOL OnEraseBkgnd(CDC* dc);
	afx_msg void OnSize(UINT type, int cx, int cy);
	afx_msg void OnMouseMove(UINT flags, CPoint point);
	afx_msg void OnMouseLeave();
	afx_msg void OnSetFocus(CWnd* old);
	afx_msg void OnKillFocus(CWnd* next);
	afx_msg void OnEnable(BOOL enable);
	afx_msg void OnKeyDown(UINT key, UINT repeat, UINT flags);
};


