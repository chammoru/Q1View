// QSplitterWnd.cpp : implementation file
//

#include "stdafx.h"
#include "Comparer.h"
#include "QSplitterWnd.h"


// CQSplitterWnd

IMPLEMENT_DYNAMIC(CQSplitterWnd, CSplitterWnd)

CQSplitterWnd::CQSplitterWnd()
{
	m_cxSplitter = QSPLITTER_W; // m_cxBorder * 2 + 1
	m_cySplitter = QSPLITTER_W; // m_cyBorder * 2 + 1;
	m_cxSplitterGap = m_cxSplitter;
	m_cySplitterGap = m_cxSplitter;
}

CQSplitterWnd::~CQSplitterWnd()
{
}


BEGIN_MESSAGE_MAP(CQSplitterWnd, CSplitterWnd)
END_MESSAGE_MAP()



// CQSplitterWnd message handlers

void CQSplitterWnd::OnDrawSplitter(CDC* pDC, ESplitType nType, const CRect& rect)
{
	// TODO: Add your specialized code here and/or call the base class
	if (pDC != NULL && nType == splitBorder) {
		COLORREF clr = GetSysColor(COLOR_BTNFACE);

		int cxBorder = GetSystemMetrics(SM_CXBORDER);
		int cyBorder = GetSystemMetrics(SM_CYBORDER);

		// Implement here by refering to OnDrawSplitter in
		// VC\atlmfc\src\mfc\winsplit.cpp
		// FillSolidRect doesn't work here
		pDC->Draw3dRect(rect, clr, clr);
		CRect rcTmp = rect;
		rcTmp.InflateRect(-cxBorder, -cyBorder);
		pDC->Draw3dRect(rcTmp, clr, clr);
	} else {
		CSplitterWnd::OnDrawSplitter(pDC, nType, rect);
	}
}
