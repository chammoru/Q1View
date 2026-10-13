// FrmStateView.cpp : implementation file
//

#include "stdafx.h"
#include "Comparator.h"
#include "ComparatorDoc.h"
#include "ComparatorView.h"
#include "FrmInfoView.h"
#include "QViewerCmn.h"
#include "Q1UiFontWin.h"
#include "MainFrm.h"
#include "QImageStr.h"
#include "ComparatorUtil.h"
#include "FrmsInfoView.h"
#include "PosInfoView.h"

// CFrmInfoView

IMPLEMENT_DYNAMIC(CAnalysisScrollView, CScrollView)
IMPLEMENT_DYNCREATE(CFrmInfoView, CAnalysisScrollView)

CFrmInfoView::CFrmInfoView()
: mWClient(0)
, mHClient(0)
{
	LOGFONT lf;

	::ZeroMemory(&lf, sizeof(lf));
	lf.lfHeight = FRM_STATE_FONT_H;
	lf.lfWeight = FW_SEMIBOLD;
	::lstrcpy(lf.lfFaceName, q1view::WindowsUiTextFontFamily());

	mStateFont.CreateFontIndirect(&lf);
}

CFrmInfoView::~CFrmInfoView()
{
}

BEGIN_MESSAGE_MAP(CFrmInfoView, CAnalysisScrollView)
	ON_WM_CREATE()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
END_MESSAGE_MAP()

int CFrmInfoView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CAnalysisScrollView::OnCreate(lpCreateStruct) == -1)
		return -1;

	GetDocument()->mFrmInfoView = this;
	if (!mReferenceLabel.Create(_T("Reference not loaded"),WS_CHILD|SS_OWNERDRAW,CRect(),this,0x7903)) return -1;
	for (int i=0;i<3;++i)
		if (!mPairButtons[i].Create(_T(""),WS_CHILD|WS_TABSTOP|BS_OWNERDRAW,CRect(),this,0x7900+i)) return -1;

	return 0;
}


CRect CFrmInfoView::SourceColumnBounds(int slot) const
{
	CRect bounds(0,0,0,0);
	auto* frame=static_cast<CMainFrame*>(AfxGetMainWnd());
	auto* doc=GetDocument();
	if (!doc || !frame || slot<0 || slot>=frame->mNumOfViews) return bounds;
	auto* view=doc->mPane[slot].pView;
	if (!view || !view->GetSafeHwnd()) return bounds;
	view->GetClientRect(bounds);
	view->ClientToScreen(bounds); ScreenToClient(bounds);
	CRect client; GetClientRect(client);
	bounds.SetRect(max(0L,bounds.left),0,min(client.right,bounds.right),0);
	return bounds;
}

void CFrmInfoView::OnDraw(CDC* dc)
{
	auto* doc=GetDocument();
	auto* frame=static_cast<CMainFrame*>(AfxGetMainWnd());
	q1view::EnsureWindowsUiFont(mStateFont,q1view::WindowsUiFontRole::Body,m_hWnd);
	CRect rc; GetClientRect(rc);
	const UINT dpi=q1view::WindowsUiDpi(m_hWnd);
	const double scale=q1view::WindowsUiSettings().Scale();
	auto px=[&](int dip) { return q1view::WindowsUiPixels(dip,dpi,scale); };
	const int metric=frame->mMetricIdx;
	CString texts[4]; CRect columns[4]; int extent=0;
	if (doc->mPane[0].isAvail())
		texts[0].Format(_T("Reference \x00b7 #%ld"),doc->mPane[0].curFrameID);
	else texts[0]=_T("Reference not loaded");
	auto* oldFont=dc->SelectObject(&mStateFont);
	for (int slot=0;slot<4;++slot) {
		columns[slot]=SourceColumnBounds(slot);
		if (columns[slot].Width()<=0 || (slot && !doc->mPane[slot].isAvail())) continue;
		if (slot) {
			CString& text=texts[slot];
			text=doc->PairTitle(slot);
			if (doc->PairFrames(slot) && slot==doc->mActiveTarget) text+=_T(" (Active)");
			if (!doc->mPane[0].isAvail()) text+=_T("\nLoad reference A to compare");
			else {
				const auto& value=doc->mPairResults[slot-1];
				text.AppendFormat(_T("  |  A #%ld / %c #%ld\n"),doc->mPane[0].curFrameID,_T('A')+slot,doc->mPane[slot].curFrameID);
				if (doc->PairFrames(slot)) text+=value.full.IsEmpty() ? value.state : value.full;
				else text+=_T("No comparable frames");
				if (!value.full.IsEmpty() && metric==METRIC_PSNR_IDX) text+=_T(" dB");
				if (!value.full.IsEmpty() && value.state!=_T("Ready")) text+=_T("\n")+value.state;
				if (!value.roi.IsEmpty()) text+=_T("\n")+value.roi;
				if (doc->mPane[0].curFrameID>=0 && doc->mPane[slot].curFrameID>=0 &&
					doc->mPane[0].curFrameID!=doc->mPane[slot].curFrameID)
					text+=_T("\nUnaligned: frame indices differ");
				if (doc->mRequestedFrame>=doc->PairFrames(slot)) text.AppendFormat(_T("\nNo aligned frame #%ld; showing actual frames above"),doc->mRequestedFrame);
				if (doc->mPane[0].GetFps()!=doc->mPane[slot].GetFps()) text+=_T("\nFrame-index comparison; FPS differs");
			}
		}
		// Reserve scrollbar width while measuring the last column, avoiding
		// a wrap/extent oscillation when overflow becomes visible.
		int width=columns[slot].Width()-(slot==frame->mNumOfViews-1 ? px(17) : 0);
		CRect measure(0,0,max(1,width-16),0);
		dc->DrawText(texts[slot],measure,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);
		extent=max(extent,measure.Height()+8);
	}
	dc->SelectObject(oldFont);
	SetAnalysisExtent(extent);
	dc->FillSolidRect(CRect(0,GetScrollPos(SB_VERT),rc.Width(),GetScrollPos(SB_VERT)+rc.Height()),Q1UI_COLOR_SURFACE);
	for (int slot=0;slot<4;++slot) {
		CWnd& footer=slot ? static_cast<CWnd&>(mPairButtons[slot-1]) : static_cast<CWnd&>(mReferenceLabel);
		if (columns[slot].Width()<=0 || (slot && !doc->mPane[slot].isAvail())) {
			footer.ShowWindow(SW_HIDE); continue;
		}
		CString old; footer.GetWindowText(old);
		if (old!=texts[slot]) footer.SetWindowText(texts[slot]);
		if (slot) {
			auto& button=mPairButtons[slot-1];
			button.active=doc->PairFrames(slot) && slot==doc->mActiveTarget;
			button.EnableWindow(doc->PairFrames(slot)>0);
			CString info; info.Format(_T("A: %s\n%c: %s\nA %dx%d / %c %dx%d\nFrame-index comparison"),
				doc->mPane[0].pathName.GetString(),_T('A')+slot,doc->mPane[slot].pathName.GetString(),
				doc->mPane[0].srcW,doc->mPane[0].srcH,_T('A')+slot,doc->mPane[slot].srcW,doc->mPane[slot].srcH);
			if (doc->PairFrames(slot)) {
				if (!qmetric_info_table[metric].lazy && doc->IsRGBCompare(doc->mPane,doc->mPane+slot))
					info+=_T("\nComparison basis: RGB aggregate");
				const auto& value=doc->mPairResults[slot-1];
				if (!value.channels.IsEmpty())
					info.AppendFormat(_T("\n\n%s channels%s\n%s"),CString(CA2W(qmetric_info_table[metric].name)).GetString(),
						metric==METRIC_PSNR_IDX ? _T(" (dB)") : _T(""),value.channels.GetString());
			}
			button.SetInfoTip(info);
		}
		CRect desired(columns[slot].left,-GetScrollPos(SB_VERT),min(columns[slot].right,(LONG)ContentWidth()),extent-GetScrollPos(SB_VERT));
		CRect current; footer.GetWindowRect(current); ScreenToClient(current);
		if (current!=desired || !footer.IsWindowVisible())
			footer.SetWindowPos(nullptr,desired.left,desired.top,max(0,desired.Width()),desired.Height(),SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
		footer.Invalidate(FALSE);
	}
}

BOOL CFrmInfoView::OnCommand(WPARAM w, LPARAM l)
{
	if (HIWORD(w)==BN_CLICKED && LOWORD(w)>=0x7900 && LOWORD(w)<0x7903) {
		GetDocument()->SelectComparison(LOWORD(w)-0x7900+1); return TRUE;
	}
	return CAnalysisScrollView::OnCommand(w,l);
}

BOOL CFrmInfoView::PreTranslateMessage(MSG* message)
{
	if (message->message==WM_KEYDOWN) {
		auto slots=GetDocument()->ComparisonTargets();
		int focused=-1;
		for (int i=0;i<(int)slots.size();++i)
			if (message->hwnd==mPairButtons[slots[i]-1].m_hWnd) focused=i;
		if (!slots.empty() && message->wParam==VK_TAB) {
			int direction=(GetKeyState(VK_SHIFT)&0x8000) ? -1 : 1;
			if (direction>0 && (focused==(int)slots.size()-1 || message->hwnd==mAnalysisScroll.m_hWnd)) {
				GetDocument()->mFrmsInfoView->FocusHeader(); return TRUE;
			}
			if (focused==0 && direction<0) { GetDocument()->mPosInfoView->SetFocus(); return TRUE; }
			int next=focused<0 ? 0 : (focused+direction+(int)slots.size())%(int)slots.size();
			auto& button=mPairButtons[slots[next]-1];
			CRect bounds; button.GetWindowRect(bounds); ScreenToClient(bounds);
			int scroll=GetScrollPos(SB_VERT);
			if (bounds.top<0) OnScrollBy(CSize(0,bounds.top));
			else if (bounds.bottom>mHClient) OnScrollBy(CSize(0,bounds.top));
			UpdateWindow(); button.SetFocus(); return TRUE;
		}
		if (focused>=0 && message->wParam==VK_RETURN) {
			mPairButtons[slots[focused]-1].SendMessage(BM_CLICK); return TRUE;
		}
	}
	return CAnalysisScrollView::PreTranslateMessage(message);
}


#ifdef _DEBUG
void CFrmInfoView::AssertValid() const
{
	CView::AssertValid();
}

#ifndef _WIN32_WCE
void CFrmInfoView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}
#endif
#endif //_DEBUG


BOOL CFrmInfoView::OnEraseBkgnd(CDC* pDC)
{
	CComparatorDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc || pDoc->mFrmState.IsEmpty()) {
		// Empty/loading/missing-reference states must not expose COLOR_WINDOW.
		CRect rc; GetClientRect(rc);
		pDC->FillSolidRect(rc,Q1UI_COLOR_SURFACE);
	}
	return TRUE;
}

void CFrmInfoView::OnSize(UINT nType, int cx, int cy)
{
	CAnalysisScrollView::OnSize(nType, cx, cy);

	GetClientRect(&mRcClient);

	mWClient = cx;
	mHClient = cy;
}
