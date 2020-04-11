#pragma once


// CQMenuItem

class CQMenuItem : public CWnd
{
	DECLARE_DYNAMIC(CQMenuItem)

	bool mMouseIn;
	CRect mRcClient;
	CFont mFont;
	CBrush mNormBkBrush, mOverBkBrush;
	CBrush *mBkBrush;
	CMenu *mMenu;

public:
	CQMenuItem();
	virtual ~CQMenuItem();

	void DefaultSetting(CDC *pDC, CString &str);
	void CalcRect(CRect *rect);

protected:
	DECLARE_MESSAGE_MAP()
public:
	BOOL CQMenuItem::Create(LPCTSTR lpszWindowName, CRect &rect, CWnd* pParentWnd, CMenu *pMenu);
	afx_msg void OnPaint();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
};


