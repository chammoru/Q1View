
// MainFrm.h : interface of the CMainFrame class
//

#pragma once
#include "afxext.h"

#include "QSplitterWnd.h"

#include "qimage_metrics.h"

#define FRAME_INFO_H          28
#define FRAMES_INFO_H         84
#define POS_INFO_W            76

#define WM_OPEN_PENDING_FILE  (WM_APP + 1)
#define WM_RELOAD_PANE        (WM_USER + 100)

#define COMPARER_DEF_W        (CANVAS_DEF_W + /* left */ \
                              (CANVAS_DEF_W + QSPLITTER_W) + /* right */ \
                              (POS_INFO_W + QSPLITTER_W)) /* position */
#define COMPARER_DEF_H        (PANE_DEF_H + /* main */ \
                              (FRAME_INFO_H + QSPLITTER_W) + /* frame info */ \
                              (FRAMES_INFO_H + QSPLITTER_W))
#define VIT_INVALIDATE_DUR    200

#define ID_OPTIONS_DIFF_RESOLUTION (ID_OPTIONS_START + 0)

enum ComparatorTimerID
{
	CTI_ID_POS_INVALIDATE = 1,
	CTI_ID_PLAY,

	CTI_ID_MAX,
};

// Application-level help overlay: a borderless, top-level layered popup owned by
// the frame that covers the entire frame client (all comparison panes + info
// views) and dims everything behind a centered shortcut panel, so help is shown
// at the application level regardless of the multi-pane splitter layout. Mirrors
// the Viewer's overlay (issue #79). Layered *popups* composite reliably across
// Windows versions (layered child windows do not).
class CHelpOverlay : public CWnd
{
public:
	BOOL CreateOverlay(CWnd *pParent);
	bool IsShown() const
	{ return GetSafeHwnd() != NULL && (GetStyle() & WS_VISIBLE) != 0; }
	void Toggle();
	void Hide();
	void Relayout();          // re-cover the owner's client; repaint if visible

protected:
	bool OwnerScreenRect(CRect &rc) const;   // owner client rect in screen coords
	void Render();            // build the per-pixel-alpha image and push it
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
	DECLARE_MESSAGE_MAP()

private:
	CWnd *mOwner = NULL;      // the frame whose client this overlay covers
};

class CMainFrame : public CFrameWnd
{

protected: // create from serialization only
	CMainFrame();
	DECLARE_DYNCREATE(CMainFrame)

// Attributes
private:
	CQSplitterWnd mGraphSplitter;
	CQSplitterWnd mPosInfoSplitter;
	CQSplitterWnd mFrmInfoSplitter;
	CQSplitterWnd mCompSplitter;
	int mSplitMargin;
	CMenu mResolutionMenu, mMetricMenu, mFpsMenu, mViewsMenu, mOptionsMenu;

	// Full-window shortcut/help overlay (issue #79).
	CHelpOverlay mHelpOverlay;

public:
	int mMetricIdx;
	int mNumOfViews;
	int mPreViews;
	int mSplitBarW;

// Operations
public:
	void CheckResolutionRadio(int w, int h);
	void UpdateResolutionLabel(int w, int h);
	void UpdateFpsLabel(double fps);
	void CheckMetricRadio();
	void UpdateMetricLabel();
	void CheckFpsRadio(double fps);
	void CheckViewsRadio(int numOfViews);
	void CheckOptionsRadio(UINT nOptionID, bool checked);
	void UpdateViewsLabel(int numOfViews);
	void UpdateMagnication(float n);
	void RefreshFrmsInfoView();
	void RefreshAllViews();
	int ChangeNumOfViews(int newNumOfViews);
	// Show/hide the full-window help overlay ('?' key or the Help menu).
	void ToggleHelpOverlay();

// Overrides
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

// Implementation
public:
	virtual ~CMainFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif


// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()

	virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);

public:
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnMove(int x, int y);
	afx_msg void OnHelp();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnResolutionChange(UINT nID);
	afx_msg void OnMetricChange(UINT nID);
	afx_msg void OnFpsChange(UINT nID);
	afx_msg void OnViewsChange(UINT nID);
	afx_msg void OnOptionsChange(UINT nID);
	afx_msg LRESULT OnOpenPendingFile(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnReloadPane(WPARAM wParam, LPARAM lParam);
	virtual void ActivateFrame(int nCmdShow = -1);
};


