// QMenuItem.cpp : implementation file
//

#include "stdafx.h"
#include "QMenuItem.h"


// CQMenuItem

IMPLEMENT_DYNAMIC(CQMenuItem, CWnd)

#define CQMENUITEM_CLASSNAME    _T("CQMenuItem")  // Window class name

CQMenuItem::CQMenuItem()
: mMouseIn(false)
, mTextBlank(" ")
{
	WNDCLASS wndcls;
	HINSTANCE hInst = AfxGetInstanceHandle();

	if (!::GetClassInfo(hInst, CQMENUITEM_CLASSNAME, &wndcls)) {
		// otherwise we need to register a new class
		wndcls.style         = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
		wndcls.lpfnWndProc   = ::DefWindowProc;
		wndcls.cbClsExtra    = wndcls.cbWndExtra = 0;
		wndcls.hInstance     = hInst;
		wndcls.hIcon         = NULL;
		wndcls.hCursor       = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
		wndcls.hbrBackground = (HBRUSH) (COLOR_3DFACE + 1);
		wndcls.lpszMenuName  = NULL;
		wndcls.lpszClassName = CQMENUITEM_CLASSNAME;

		if (!AfxRegisterClass(&wndcls))
			AfxThrowResourceException();
	}

	mRcClient.left = 0;
	mRcClient.top = 0;

	LOGFONT lf;
	::ZeroMemory(&lf, sizeof(lf));

	SystemParametersInfo(SPI_GETICONTITLELOGFONT,sizeof(LOGFONT),&lf,0);
	mFont.CreateFontIndirect(&lf);

	COLORREF clr;
	
	clr = ::GetSysColor(COLOR_MENU);
	mNormBkBrush.CreateSolidBrush(clr);

	BYTE r = GetRValue(clr);
	r = max(0, r - 0x33);
	BYTE g = GetGValue(clr);
	g = max(0, g - 0x33);
	BYTE b = GetBValue(clr);
	b = max(0, b - 0x33);

	clr = RGB(r, g, b);
	mOverBkBrush.CreateSolidBrush(clr);

	mBkBrush = &mNormBkBrush;
}

CQMenuItem::~CQMenuItem()
{
}


BEGIN_MESSAGE_MAP(CQMenuItem, CWnd)
	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
	ON_WM_MOUSEMOVE()
END_MESSAGE_MAP()



// CQMenuItem message handlers



BOOL CQMenuItem::Create(LPCTSTR lpszWindowName, CRect &rect, CWnd* pParentWnd, CMenu *pMenu,
	DWORD textHorizAlign)
{
	mMenu = pMenu;
	mTextHorizAlign = textHorizAlign;

	// TODO: Add your specialized code here and/or call the base class
	return CWnd::Create(_T("CQMenuItem"), mTextBlank + lpszWindowName + mTextBlank,
		WS_VISIBLE | WS_CHILD, rect, pParentWnd, 0xffff);
}

void CQMenuItem::DefaultSetting(CDC *pDC, CString &str)
{
	pDC->SetBkMode(TRANSPARENT);
	pDC->SelectObject(mFont);
	pDC->SelectObject(mBkBrush);
	pDC->SelectStockObject(NULL_PEN);

	GetWindowText(str);
}

void CQMenuItem::CalcRect(CRect *rect)
{
	CPaintDC dc(this);
	CString str;

	DefaultSetting(&dc, str);
	dc.DrawText(str, rect, mTextHorizAlign | DT_VCENTER | DT_CALCRECT);
}

void CQMenuItem::OnPaint()
{
	CPaintDC dc(this); // device context for painting
	// TODO: Add your message handler code here
	// Do not call CWnd::OnPaint() for painting messages
	CString str;
	int w = mRcClient.Width();
	int h = mRcClient.Height();

	CDC memDC;
	memDC.CreateCompatibleDC(&dc);

	CBitmap bitmap;
	bitmap.CreateCompatibleBitmap(&dc, w, h);

	memDC.SelectObject(bitmap);
	memDC.SetStretchBltMode(COLORONCOLOR);

	DefaultSetting(&memDC, str);

	CRect rect = mRcClient;
	rect.InflateRect(0, 0, 1, 1);
	memDC.Rectangle(&rect);
	memDC.DrawText(str, &mRcClient, DT_SINGLELINE | mTextHorizAlign | DT_VCENTER);

	dc.BitBlt(0, 0, w, h, &memDC, 0, 0, SRCCOPY);
}

void CQMenuItem::OnLButtonDown(UINT nFlags, CPoint point)
{
	// TODO: Add your message handler code here and/or call default
	if (mMenu) {
		mMouseIn = false;

		mBkBrush = &mNormBkBrush;
		Invalidate(FALSE);

		CRect rect;
		GetWindowRect(&rect);

		mMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON,
			rect.left, rect.bottom, GetParent());
	}

	CWnd::OnLButtonDown(nFlags, point);
}

BOOL CQMenuItem::OnEraseBkgnd(CDC* pDC)
{
	// TODO: Add your message handler code here and/or call default

	return FALSE;
	// return CWnd::OnEraseBkgnd(pDC);
}

void CQMenuItem::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);

	// TODO: Add your message handler code here
	mRcClient.right = cx;
	mRcClient.bottom = cy;
}

void CQMenuItem::OnMouseMove(UINT nFlags, CPoint point)
{
	// TODO: Add your message handler code here and/or call default
	if (mMenu) {
		SetCapture();

		CRect rcWindow;
		GetWindowRect(&rcWindow);
		ScreenToClient(&rcWindow);

		if (rcWindow.PtInRect(point))
		{
			if (!mMouseIn) {
				mMouseIn = true;
				mBkBrush = &mOverBkBrush;
				Invalidate(FALSE);
			}
		} else {
			mMouseIn = false;
			mBkBrush = &mNormBkBrush;
			::ReleaseCapture();
			Invalidate(FALSE);
		}
	}

	CWnd::OnMouseMove(nFlags, point);
}

void CQMenuItem::SetWindowText(LPCTSTR lpszWindowName)
{
	CWnd::SetWindowText(mTextBlank + lpszWindowName + mTextBlank);
}
