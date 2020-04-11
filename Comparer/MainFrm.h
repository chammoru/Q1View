
// MainFrm.h : interface of the CMainFrame class
//

#pragma once
#include "afxext.h"

#include "QSplitterWnd.h"

#include "qimage_metrics.h"

#define FRAME_INFO_H          20
#define FRAMES_INFO_H         60
#define POS_INFO_W            66

#define COMPARER_DEF_W        (CANVAS_DEF_W + /* left */ \
                              (CANVAS_DEF_W + QSPLITTER_W) + /* right */ \
                              (POS_INFO_W + QSPLITTER_W)) /* position */
#define COMPARER_DEF_H        (PANE_DEF_H + /* main */ \
                              (FRAME_INFO_H + QSPLITTER_W) + /* frame info */ \
                              (FRAMES_INFO_H + QSPLITTER_W))
#define VIT_INVALIDATE_DUR    200

enum ComparerTimerID
{
	CTI_ID_POS_INVALIDATE = 1,
	CTI_ID_PLAY,

	CTI_ID_MAX,
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
	int mSplitBarW;
	CMenu mResolutionMenu, mMetricMenu;

public:
	int mMetricIdx;

// Operations
public:
	void CheckResolutionRadio(int w, int h);
	void UpdateResolutionLabel(int w, int h);
	void CheckMetricRadio();
	void UpdateMetricLabel();
	void UpdateMagnication(float n);
	void RefreshFrmsInfoView();
	void RefreshAllViews();

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
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnResolutionChange(UINT nID);
	afx_msg void OnMetricChange(UINT nID);
	virtual void ActivateFrame(int nCmdShow = -1);
};


