// Viewer.h : main header file for the Viewer application
//
#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"       // main symbols

// CViewerApp:
// See Viewer.cpp for the implementation of this class
//

class CViewerApp : public CWinApp
{
public:
	CViewerApp();
	bool IsExplicitFileOpenInProgress() const { return mExplicitFileOpen; }
	bool IsVideoAutoplayEnabled();
	void SetVideoAutoplayEnabled(bool enabled);

// Overrides
public:
	virtual BOOL InitInstance();
	virtual CDocument* OpenDocumentFile(LPCTSTR lpszFileName) override;

// Implementation
	DECLARE_MESSAGE_MAP()
	afx_msg void OnFileNew();
	afx_msg void OnFileOpen();

private:
	bool mExplicitFileOpen;
};

extern CViewerApp theApp;
