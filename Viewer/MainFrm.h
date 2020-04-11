// MainFrm.h : interface of the CMainFrame class
//


#pragma once

#define WM_RELOAD (WM_USER + 100)

class CMainFrame : public CFrameWnd
{
protected: // create from serialization only
	CMainFrame();
	DECLARE_DYNCREATE(CMainFrame)

// Attributes
private:
	CMenu mResolutionMenu, mCsMenu, mFpsMenu;
	BITMAPINFO mCopyBmi;

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
	void UpdateFpsLabel(int fps);
	void CheckFpsRadio(int fps);
	void AddMainMenu();
	void RefreshView();

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

public:
	virtual void ActivateFrame(int nCmdShow = -1);
	afx_msg void OnEditCopy();
	afx_msg void OnEditPaste();
};
