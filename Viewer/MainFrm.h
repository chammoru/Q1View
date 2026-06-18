// MainFrm.h : interface of the CMainFrame class
//


#pragma once

#define WM_RELOAD (WM_USER + 100)
#define WM_APPLY_SYNC_INPUT (WM_APP + 100)
#define WM_APPLY_SYNC_VIEW_STATE (WM_APP + 101)
// Posted by the Store update worker thread back to the main frame.
#define WM_STORE_UPDATE_AVAILABLE (WM_APP + 102)
#define WM_STORE_UPDATE_DONE (WM_APP + 103)

enum ViewerSyncInputCommand
{
	VIEWER_SYNC_SEEK_FRAME = 1,
	VIEWER_SYNC_FIRST_FILE,
	VIEWER_SYNC_LAST_FILE,
	VIEWER_SYNC_PREVIOUS_FILE,
	VIEWER_SYNC_NEXT_FILE,
	VIEWER_SYNC_VIEW_STATE,
	VIEWER_SYNC_ROTATE,
	VIEWER_SYNC_PLAYBACK,
	VIEWER_SYNC_DISPLAY_OPTIONS,
	VIEWER_SYNC_COLOR_SPACE,
	VIEWER_SYNC_RESOLUTION,
	VIEWER_SYNC_FPS,
};

enum ViewerSyncDisplayOptions
{
	VIEWER_DISPLAY_HEX_PIXEL = 0x01,
	VIEWER_DISPLAY_Y_ONLY = 0x02,
	VIEWER_DISPLAY_INTERPOLATE = 0x04,
	VIEWER_DISPLAY_COORDINATES = 0x08,
	VIEWER_DISPLAY_BOX_INFO = 0x10,
	VIEWER_DISPLAY_SOURCE_YUV = 0x20,
};

struct ViewerSyncInputState
{
	UINT command;
	LONG first;
	LONG second;
	double scalar;
	double x;
	double y;
};

class CThumbnailPane;
class CMainFrame;

// Width (logical px) of the draggable divider between the image view and the
// thumbnail drawer.
#define DRAWER_SPLIT_BAR  6

// Static splitter hosting the image view (column 0) and the thumbnail drawer
// (column 1). The divider is a thin, draggable bar so the user can resize the
// drawer (e.g. to read long file names); dragging redistributes width between
// the two panes inside the fixed window, never resizing the frame (issue #76).
class CDrawerSplitter : public CSplitterWnd
{
public:
	CMainFrame *mFrame = NULL;   // notified when the user drags the divider

	// Show/hide the draggable divider. Hidden (zero width) when the drawer is
	// closed so the image view fills the whole client with no leftover gap.
	void SetBarVisible(bool visible)
	{
		int w = visible ? DRAWER_SPLIT_BAR : 0;
		m_cxSplitter = m_cxSplitterGap = w;
		m_cySplitter = m_cySplitterGap = w;
		m_cxBorder = m_cyBorder = 0;
		m_cxBorderShare = m_cyBorderShare = 0;
	}
	int BarWidth() const { return m_cxSplitterGap; }

protected:
	// Flat, subtle divider instead of the default 3D splitter bar.
	virtual void OnDrawSplitter(CDC *pDC, ESplitType nType, const CRect &rect);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	DECLARE_MESSAGE_MAP()
};

// Application-level help overlay (issue #79): a borderless, layered child window
// that covers the entire frame client -- image view *and* thumbnail drawer -- so
// the shortcut panel is centered on the whole window and dims everything behind
// it, regardless of whether the drawer is open. (Previously help was painted in
// the image view's DC, so it sat inside the image column and shifted with it
// when the drawer opened.)
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
	CMenu mResolutionMenu, mCsMenu, mFpsMenu;
	BITMAPINFO mCopyBmi;
	bool mSyncInput;
	bool mSyncViewStatePending;
	ViewerSyncInputState mPendingSyncViewState;

	// Whether the right-aligned "Update" menu item (shown only when a Store
	// update is available) has been inserted into the menu bar.
	bool mUpdateMenuShown;

	// Left-side thumbnail drawer.
	CDrawerSplitter mwndSplitter;
	CThumbnailPane *mpDrawer;
	bool mDrawerVisible;
	int  mDrawerWidth;
	bool mSplitterReady;

	// Open/close slide animation state. The drawer column slides between 0 and
	// mDrawerWidth while the frame size stays fixed (the image view absorbs the
	// change), so no window-geometry bookkeeping is needed.
	bool mDrawerAnimating;
	bool mDrawerAnimOpening;
	int  mDrawerAnimStep;
	int  mDrawerAnimSteps;

	// Full-window shortcut/help overlay (issue #79).
	CHelpOverlay mHelpOverlay;

// Operations
public:

// Overrides
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual void OnUpdateFrameTitle(BOOL bAddToTitle);
	virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);

// Implementation
public:
	virtual ~CMainFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
	void UpdateResolutionLabel(int w, int h);
	void CheckResolutionRadio(int w, int h);
	void NextResolution();
	void ApplyResolution(int w, int h);
	void UpdateCsLabel(const TCHAR *csLabel);
	void CheckCsRadio(int cs);
	void UpdateMagnication(float n, int wDst, int hDst);
	void UpdateFpsLabel(double fps);
	void CheckFpsRadio(double fps);
	void AddMainMenu();
	void RefreshView();
	bool IsSyncInputEnabled() const { return mSyncInput; }
	void ToggleSyncInput();
	void BroadcastSyncInput(const ViewerSyncInputState &input);
	// Width (incl. splitter bar) the drawer reserves in the frame client area.
	int  GetDrawerReservedWidth() const;
	// Called by the document when the active file changes, to sync the drawer.
	void OnDocPathChanged(LPCTSTR lpszPath);
	// Called by CDrawerSplitter after the user finishes dragging the divider:
	// adopts the new drawer width (clamped) and refits the image to the view.
	void OnDrawerDividerDragged();
	// Show/hide the full-window help overlay ('?' key or the Help menu).
	void ToggleHelpOverlay();

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnDropFiles(HDROP hDropInfo);
	afx_msg void OnHelp();
	afx_msg void OnFileOpen();
	afx_msg void OnExecComparator();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnResolutionChange(UINT nID);
	afx_msg void OnCsChange(UINT nID);
	afx_msg void OnFpsChange(UINT nID);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnMove(int x, int y);
	afx_msg void OnDestroy();
	afx_msg void OnToggleDrawer();
	afx_msg void OnUpdateToggleDrawer(CCmdUI *pCmdUI);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	// "Update" menu command: download and install the available Store update.
	afx_msg void OnUpdateNow();
	// Store worker-thread notifications (see WM_STORE_UPDATE_* in this header).
	afx_msg LRESULT OnStoreUpdateAvailable(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnStoreUpdateDone(WPARAM wParam, LPARAM lParam);

private:
	BOOL ResolveComparatorPath(CString &cmperPath);
	BOOL LaunchComparator(const CString &cmperPath, const CString &quotedArgs);
	void PinDrawerColumn();
	void StartDrawerAnimation(bool opening);
	void ApplyDrawerColumn(int drawerCol);
	void FinalizeDrawerAnimation();
	// Refit the image into the current view column (after a drawer resize).
	void RefitView();
	afx_msg LRESULT Reload(WPARAM wParam, LPARAM lParam);
	afx_msg BOOL OnCopyData(CWnd *pWnd, COPYDATASTRUCT *pCopyDataStruct);
	afx_msg LRESULT OnApplySyncInput(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnApplySyncViewState(WPARAM wParam, LPARAM lParam);

public:
	virtual void ActivateFrame(int nCmdShow = -1);
	afx_msg void OnEditCopy();
	afx_msg void OnEditPaste();
	void UpdateCs(const struct qcsc_info* const ci);
};
