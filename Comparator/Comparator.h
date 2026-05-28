
// Comparator.h : main header file for the Comparator application
//
#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"       // main symbols

#define CSV_SEPARATOR _T("|")

// CComparatorApp:
// See Comparator.cpp for the implementation of this class
//

class CComparatorApp : public CWinApp
{
public:
	CComparatorApp();


// Overrides
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

// Implementation
	afx_msg void OnAppAbout();
	DECLARE_MESSAGE_MAP()
	afx_msg void OnFileOpen();

private:
	ULONG_PTR mGdiplusToken;
};

extern CComparatorApp theApp;
