// QSplitterWnd.cpp : implementation file
//

#include "stdafx.h"
#include "Comparator.h"
#include "QSplitterWnd.h"
#include "QViewerCmn.h"


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



void CQSplitterWnd::OnDrawSplitter(CDC* pDC, ESplitType nType, const CRect& rect)
{
	if (pDC != NULL && nType == splitBorder) {
		COLORREF clr = Q1UI_COLOR_BORDER_SOFT;

		int cxBorder = GetSystemMetrics(SM_CXBORDER);
		int cyBorder = GetSystemMetrics(SM_CYBORDER);

		// Match CSplitterWnd's two-border drawing while using the app's
		// softer divider color.
		pDC->Draw3dRect(rect, clr, clr);
		CRect rcTmp = rect;
		rcTmp.InflateRect(-cxBorder, -cyBorder);
		pDC->Draw3dRect(rcTmp, clr, clr);
	} else {
		CSplitterWnd::OnDrawSplitter(pDC, nType, rect);
	}
}
