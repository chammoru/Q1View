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

// Overrides
public:
	virtual BOOL InitInstance();

// Implementation
	DECLARE_MESSAGE_MAP()
	afx_msg void OnFileNew();
	afx_msg void OnFileOpen();
};

extern CViewerApp theApp;