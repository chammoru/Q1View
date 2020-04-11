#pragma once


// CCustomFpsDlg dialog

class CCustomFpsDlg : public CDialog
{
	DECLARE_DYNAMIC(CCustomFpsDlg)

public:
	CCustomFpsDlg(int &fps, CWnd* pParent = NULL);   // standard constructor
	virtual ~CCustomFpsDlg();

// Dialog Data
	enum { IDD = IDD_CUSTOM_FPS };
	int &mFps;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
};
