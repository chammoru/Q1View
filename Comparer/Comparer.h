
// Comparer.h : main header file for the Comparer application
//
#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"       // main symbols


// CComparerApp:
// See Comparer.cpp for the implementation of this class
//

class CComparerApp : public CWinApp
{
public:
	CComparerApp();


// Overrides
public:
	virtual BOOL InitInstance();

// Implementation
	afx_msg void OnAppAbout();
	DECLARE_MESSAGE_MAP()
	afx_msg void OnFileOpen();
};

extern CComparerApp theApp;

#define REG_OPEN_SETTING        _T("OpenSetting")
#define REG_OPEN_SETTING_OTHERS _T("OtherNewNames")
