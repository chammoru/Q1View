// MainFrm.h : interface of the CMainFrame class
//


#pragma once

#define WM_RELOAD (WM_USER + 100)
#define WM_APPLY_SYNC_INPUT (WM_APP + 100)
#define WM_APPLY_SYNC_VIEW_STATE (WM_APP + 101)

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

// Operations
public:

// Overrides
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual void OnUpdateFrameTitle(BOOL bAddToTitle);

// Implementation
public:
	virtual ~CMainFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
	void UpdateResolutionLabel(int w, int h);
	void CheckResolutionRadio(int w, int h);
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

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
public:
//	afx_msg void OnDropFiles(HDROP hDropInfo);
	afx_msg void OnHelp();
	afx_msg void OnFileOpen();
	afx_msg void OnExecComparer();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnResolutionChange(UINT nID);
	afx_msg void OnCsChange(UINT nID);
	afx_msg void OnFpsChange(UINT nID);

private:
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
