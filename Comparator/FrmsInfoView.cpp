#include "stdafx.h"
#include "Comparator.h"
#include "ComparatorDoc.h"
#include "MainFrm.h"
#include "FrmsInfoView.h"
#include "FileScanThread.h"
#include "FrmCmpStrategy.h"
#include <cmath>

IMPLEMENT_DYNCREATE(CFrmsInfoView,CAnalysisScrollView)
BEGIN_MESSAGE_MAP(CFrmsInfoView,CAnalysisScrollView)
	ON_WM_SIZE()
	ON_WM_CREATE()
	ON_WM_ERASEBKGND()
	ON_WM_MOUSEWHEEL()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_LBUTTONDOWN()
	ON_WM_KEYDOWN()
END_MESSAGE_MAP()

CFrmsInfoView::CFrmsInfoView() : mWClient(0),mHClient(0),mFileScanThread(nullptr) {}
CFrmsInfoView::~CFrmsInfoView() {}

int CFrmsInfoView::OnCreate(LPCREATESTRUCT cs)
{
	if (CAnalysisScrollView::OnCreate(cs)==-1) return -1;
	GetDocument()->mFrmsInfoView=this;
	mFileScanThread=GetDocument()->mFileScanThread;
	if (!mCollapse.Create(_T("Sequence analysis - collapse"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,CRect(),this,0x7950)) return -1;
	return 0;
}

void CFrmsInfoView::OnDraw(CDC* dc)
{
	auto* doc=GetDocument();
	const int metric=static_cast<CMainFrame*>(AfxGetMainWnd())->mMetricIdx;
	const auto& info=qmetric_info_table[metric];
	auto targets=doc->ComparisonTargets();
	const auto dpi=q1view::WindowsUiDpi(m_hWnd);
	const double scale=q1view::WindowsUiSettings().Scale();
	auto px=[&](int dip) { return q1view::WindowsUiPixels(dip,dpi,scale); };
	bool onlyStills=true;
	for (int target:targets) onlyStills=onlyStills && doc->PairFrames(target)<=1;
	const int header=px(30), chartHeight=px(onlyStills ? 68 : 180);
	q1view::EnsureWindowsUiFont(mLabelFont,q1view::WindowsUiFontRole::Numeric,m_hWnd);
	q1view::EnsureWindowsUiFont(mBodyFont,q1view::WindowsUiFontRole::Body,m_hWnd);
	bool stacked=ContentWidth() < (int)targets.size()*px(330);
	SetAnalysisExtent(mCollapsed || targets.empty() ? header : header+chartHeight*(stacked ? (int)targets.size() : 1));
	int width=ContentWidth(), scroll=GetScrollPos(SB_VERT);
	CRect client; GetClientRect(client);
	dc->FillSolidRect(CRect(0,scroll,client.Width(),scroll+client.Height()),Q1UI_COLOR_SURFACE);
	dc->SetBkMode(TRANSPARENT); dc->SelectObject(&mBodyFont);
	CRect current; mCollapse.GetWindowRect(current); ScreenToClient(current);
	const CRect desired(0,0,width,header);
	if (current!=desired)
		mCollapse.SetWindowPos(nullptr,0,0,width,header,SWP_NOZORDER|SWP_NOACTIVATE);
	for (int i=0;i<3;++i) { mPlots[i].SetRectEmpty(); mCards[i].SetRectEmpty(); }
	if (mCollapsed) return;
	if (targets.empty()) {
		CRect message(0,header,width,client.Height()+scroll); dc->SetTextColor(Q1UI_COLOR_TEXT_MUTED);
		dc->DrawText(_T("Open reference A and at least one candidate to compare"),message,DT_CENTER|DT_WORDBREAK); return;
	}
	// Shared X viewport and shared finite Y scale for the selected metric.
	double lo=0,hi=1; bool finite=false;
	for (int t:targets) {
		auto s=mFileScanThread->summary(t);
		for (int p=0;p<info.plane_count;++p) if (s.finite[metric][p]) {
			if (!finite) { lo=s.minimum[metric][p]; hi=s.maximum[metric][p]; finite=true; }
			else { lo=min(lo,s.minimum[metric][p]); hi=max(hi,s.maximum[metric][p]); }
		}
	}
	if (hi<=lo) { hi=lo+1; lo-=1; }
	int total=max(1,doc->mMaxFrames);
	if (mFrameSpan<=0 || mFrameSpan>total) mFrameSpan=total;
	mFirstFrame=max(0.,min(mFirstFrame,total-mFrameSpan));
	COLORREF colors[3]={Q1UI_COLOR_ACCENT,Q1UI_COLOR_WARNING,Q1UI_COLOR_DANGER};
	int index=0;
	for (int target:targets) {
		auto s=mFileScanThread->summary(target);
		int x=stacked ? 0 : width*index/(int)targets.size();
		int right=stacked ? width : width*(index+1)/(int)targets.size();
		CRect card(x,header+(stacked ? index*chartHeight : 0),right,header+(stacked ? (index+1)*chartHeight : chartHeight));
		mCards[target-1]=card;
		dc->Draw3dRect(card,target==doc->mActiveTarget ? Q1UI_COLOR_ACCENT : Q1UI_COLOR_BORDER_SOFT,Q1UI_COLOR_BORDER_SOFT);
		CString title=doc->PairTitle(target);
		title.AppendFormat(_T("  %s%s   %d / %d"),CString(CA2W(info.name)).GetString(),metric==METRIC_PSNR_IDX ? _T(" (dB)") : _T(""),s.measured[metric],s.eligible);
		CRect label=card; label.DeflateRect(px(8),px(4)); label.bottom=label.top+px(22);
		dc->SetTextColor(Q1UI_COLOR_TEXT); dc->DrawText(title,label,DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
		CRect plot=card; plot.DeflateRect(px(52),px(32),px(8),px(58));
		if (s.eligible<=1 || plot.Width()<5 || plot.Height()<5) {
			label.top+=px(26); label.bottom=card.bottom-px(4); dc->SetTextColor(Q1UI_COLOR_TEXT_MUTED);
			dc->DrawText(_T("Single-frame comparison\nCurrent results above; no temporal curve"),label,DT_WORDBREAK); ++index; continue;
		}
		mPlots[target-1]=plot;
		dc->Draw3dRect(plot,Q1UI_COLOR_BORDER_SOFT,Q1UI_COLOR_BORDER_SOFT);
		dc->SelectObject(&mLabelFont);
		CString value; value.Format(_T("%.3g"),hi);
		CRect tick(card.left+2,plot.top,plot.left-px(3),plot.top+px(20)); dc->DrawText(value,tick,DT_RIGHT|DT_SINGLELINE);
		value.Format(_T("%.3g"),lo); tick.top=plot.bottom-px(20); tick.bottom=plot.bottom; dc->DrawText(value,tick,DT_RIGHT|DT_SINGLELINE);
		value.Format(_T("#%d \x2014 #%d"),(int)mFirstFrame,(int)min((double)total-1,mFirstFrame+mFrameSpan-1));
		tick=CRect(plot.left,plot.bottom+2,plot.right,plot.bottom+px(21)); dc->DrawText(value,tick,DT_CENTER|DT_SINGLELINE);
		// Missing, uncomputed and out-of-domain samples break the line. Infinity
		// is an explicit marker above the finite plot, not a fabricated y value.
		for (int p=0;p<info.plane_count;++p) {
			const int styles[3]={PS_SOLID,PS_DASH,PS_DOT};
			CPen pen(styles[p],1,colors[p]); auto* old=dc->SelectObject(&pen);
			std::vector<CPoint> points; int previousFrame=-1;
			auto flush=[&]() {
				if (points.size()>1) dc->Polyline(points.data(),(int)points.size());
				else if (!points.empty()) dc->SetPixelV(points[0],colors[p]);
				points.clear();
			};
			for (int pixel=0;pixel<plot.Width();++pixel) {
				int frame=(int)(mFirstFrame+(mFrameSpan-1)*pixel/max(1,plot.Width()-1));
				if (frame==previousFrame) continue;
				previousFrame=frame; double values[QPLANES];
				if (!mFileScanThread->copyMetrics(frame,metric,values,target) || !std::isfinite(values[p])) {
					flush();
					if (frame<s.eligible && mFileScanThread->copyMetrics(frame,metric,values,target) && std::isinf(values[p])) {
						dc->SetTextColor(colors[p]); dc->TextOut(plot.left+pixel,plot.top-px(16),_T("\x221e"));
					}
					continue;
				}
				int y=plot.bottom-1-(int)((values[p]-lo)/(hi-lo)*max(1,plot.Height()-2));
				points.emplace_back(plot.left+pixel,y);
			}
			flush();
			dc->SelectObject(old);
		}
		long displayed[2]={doc->mPane[0].curFrameID,doc->mPane[target].curFrameID};
		for (int source=0;source<2;++source) if (displayed[source]>=mFirstFrame && displayed[source]<mFirstFrame+mFrameSpan) {
			int markerX=plot.left+int((displayed[source]-mFirstFrame)*max(1,plot.Width()-1)/max(1.,mFrameSpan-1));
			CPen marker(PS_DOT,1,source ? Q1UI_COLOR_WARNING : Q1UI_COLOR_ACCENT);
			auto* old=dc->SelectObject(&marker); dc->MoveTo(markerX,plot.top); dc->LineTo(markerX,plot.bottom); dc->SelectObject(old);
		}
		CString averages=_T("Avg ");
		const auto labels=doc->PairStrategy(target)->GetPlaneLabels();
		for (int p=0;p<info.plane_count;++p) {
			CString average=_T("\x2014");
			if (s.infinite[metric][p]) average=_T("\x221e");
			else if (s.finite[metric][p]) average.Format(_T("%.4f"),s.sum[metric][p]/s.finite[metric][p]);
			const TCHAR* patterns[3]={_T("\x2014"),_T("--"),_T("\x00b7\x00b7")};
			averages.AppendFormat(_T("%s%s%s %s"),p ? _T("  ") : _T(""),info.plane_count==1 ? _T("LPIPS") : CString(CA2W(labels[p])).GetString(),
				info.plane_count==1 ? _T("") : patterns[p],average.GetString());
		}
		CString state=!s.error.IsEmpty() ? s.error : !s.metricError[metric].IsEmpty() ? s.metricError[metric] :
			s.measured[metric]>=s.eligible ? _T("Ready") : (info.lazy && !doc->IsLpipsScanRunning() ? _T("Unavailable") : _T("Computing"));
		averages+=_T("\n")+state;
		tick=CRect(card.left+px(8),card.bottom-px(40),card.right-px(8),card.bottom);
		dc->SelectObject(&mBodyFont); dc->SetTextColor(Q1UI_COLOR_TEXT_MUTED); dc->DrawText(averages,tick,DT_WORDBREAK|DT_NOPREFIX);
		++index;
	}
}

void CFrmsInfoView::OnSize(UINT type,int cx,int cy) { CAnalysisScrollView::OnSize(type,cx,cy); mWClient=cx; mHClient=cy; }
BOOL CFrmsInfoView::OnEraseBkgnd(CDC*) { return TRUE; }
BOOL CFrmsInfoView::OnCommand(WPARAM w,LPARAM l)
{
	if (LOWORD(w)==0x7950 && HIWORD(w)==BN_CLICKED) {
		mCollapsed=!mCollapsed;
		mCollapse.SetWindowText(mCollapsed ? _T("Sequence analysis - expand") : _T("Sequence analysis - collapse"));
		auto* frame=static_cast<CMainFrame*>(AfxGetMainWnd()); CRect rc; frame->GetClientRect(rc);
		frame->SendMessage(WM_SIZE,SIZE_RESTORED,MAKELPARAM(rc.Width(),rc.Height())); Invalidate(FALSE); return TRUE;
	}
	return CAnalysisScrollView::OnCommand(w,l);
}
BOOL CFrmsInfoView::OnMouseWheel(UINT flags,short delta,CPoint screen)
{
	CPoint pt=screen; ScreenToClient(&pt); pt.y+=GetScrollPos(SB_VERT);
	for (auto plot:mPlots) if (plot.PtInRect(pt)) {
		double anchor=mFirstFrame+(pt.x-plot.left)*mFrameSpan/max(1,plot.Width());
		double span=max(2.,min((double)GetDocument()->mMaxFrames,mFrameSpan*(delta>0 ? .8 : 1.25)));
		mFirstFrame=anchor-(anchor-mFirstFrame)*span/mFrameSpan; mFrameSpan=span; Invalidate(FALSE); return TRUE;
	}
	return DoMouseWheel(flags,delta,screen);
}
void CFrmsInfoView::OnLButtonDblClk(UINT,CPoint) { mFirstFrame=mFrameSpan=0; Invalidate(FALSE); }
void CFrmsInfoView::OnLButtonDown(UINT flags,CPoint pt)
{
	pt.y+=GetScrollPos(SB_VERT);
	for (int t:GetDocument()->ComparisonTargets()) if (mCards[t-1].PtInRect(pt)) {
		GetDocument()->SelectComparison(t);
		if (mPlots[t-1].PtInRect(pt)) {
			long frame=(long)(mFirstFrame+(mFrameSpan-1)*(pt.x-mPlots[t-1].left)/max(1,mPlots[t-1].Width()-1));
			GetDocument()->KillPlayTimer(); GetDocument()->SetScenes(frame); GetDocument()->UpdateAllViews(nullptr);
		}
		SetFocus(); return;
	}
	CAnalysisScrollView::OnLButtonDown(flags,pt);
}
void CFrmsInfoView::OnKeyDown(UINT key,UINT repeat,UINT flags)
{
	auto* doc=GetDocument();
	if (key==VK_SPACE) doc->TogglePlay();
	else if (key==VK_LEFT) doc->OffsetScenes(-1);
	else if (key==VK_RIGHT) doc->OffsetScenes(1);
	else { CAnalysisScrollView::OnKeyDown(key,repeat,flags); return; }
	doc->MarkImgViewProcessing(); doc->UpdateAllViews(nullptr);
}
#ifdef _DEBUG
void CFrmsInfoView::AssertValid() const { CAnalysisScrollView::AssertValid(); }
void CFrmsInfoView::Dump(CDumpContext& dc) const { CAnalysisScrollView::Dump(dc); }
#endif
